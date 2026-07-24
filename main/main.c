#include <stdio.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "pwm.h"
#include "spi.h"
#include "uart.h"
#include "lcd.h"
#include "ui.h"
#include "LOCK.h"
#include "SHIFT.h"
#include "va.h"
#include "key.h"
#include "i2c.h"
#include "flash.h"
#include "sleep.h"
#include "mynvs.h"
#include "wifi.h"
#define INIT_DONE_BIT (1 << 0) // 定义初始化完成的事件位
#define ACK_TIMEOUT_MS 3000 // ACK 超时时间为 3 秒

static EventGroupHandle_t init_event_group;
// static QueueHandle_t appQueue;
static lv_obj_t *scr_loading = NULL; // 用于保存开机动画界面对象，以便后续释放内存
lv_obj_t *boot_text_label;

static SendState send_state = SEND_STATE_IDLE;
static uint8_t pending_ack_seq = 0;  // 记录等待匹配的随机数
static uint8_t pending_target_id = 0;   // 缓存目标ID，用于收到ACK后写Flash
static uint32_t send_start_time = 0;    // 记录发送开始的时间戳
static char pending_payload[121] = {0}; // 缓存发送的内容

static void update_boot_label_cb(void *arg) {
    char *text = (char *)arg;
    if (boot_text_label != NULL && text != NULL) {
        lv_label_set_text(boot_text_label, text);
    }
}

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(1);
}

void app_main_task(void *arg)
{
    UIEvent event;
    uint8_t key;
    bool is_initialized = false;
    WifiProvisioningState current_wifi_state = WIFI_STATE_IDLE;
    int64_t quick_sleep_deadline_us = 0; 
    bool just_woken_up_by_aux = false;
    uint32_t last_battery_check_time = 0;
    uint32_t last_time_check_time = 0;
    uint8_t current_soc = 100;
    uint32_t sntp_start_time = 0;

    while (1)
    {
        // 非阻塞检查外设是否初始化完成，进行 UI 切换
        if (!is_initialized) {
            EventBits_t bits = xEventGroupGetBits(init_event_group);
            if (bits & INIT_DONE_BIT) {
                ESP_LOGI("APP", "IC!1");
                is_initialized = true;
                last_activity_time_us = esp_timer_get_time();
                lv_label_set_text(boot_text_label, "Done!");
                ESP_LOGI("APP", "IC!2");

                create_ui();
                ESP_LOGI("APP", "IC!3");
                // 切换到菜单页面
                ui_show_menu_page();
                ESP_LOGI("APP", "IC!4");

                // 释放开机动画界面的内存，防止内存泄漏
                scr_loading = NULL;
                boot_text_label = NULL;

                ESP_LOGI("APP", "IC!5");
                xTaskCreatePinnedToCore(scanKeyTask, "scan_key", 2048, NULL, 5, NULL, 0);
                xTaskCreatePinnedToCore(uart_receive, "uart_receive", 4096, NULL, 5, NULL, 0);
            }
        }

        // 只有在初始化完成后，才处理业务事件 (避免初始化期间处理无效数据)
        if (is_initialized) {
            // 等待 ACK
            if (send_state == SEND_STATE_WAITING_ACK) {
                uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (current_time - send_start_time >= ACK_TIMEOUT_MS) {
                    ESP_LOGW("APP", "ACK timeout! seq: %d", pending_ack_seq);
                    send_state = SEND_STATE_IDLE; // 超时重置状态，不写Flash，不清空输入框
                    char toast_string[256];
                    snprintf(toast_string, sizeof(toast_string), "Message failed to send! \nPlease check if the recipient is online!");
                    show_toast_dialog(toast_string, 8000);
                }
            }
            // 读队列
            if (xQueueReceive(appQueue, &event, 0))
            {
                if (event.type == EVENT_UART)
                {
                    if (event.frame.data_len == 0) { // 收到ACK 说明自己发出去了
                        if (send_state == SEND_STATE_WAITING_ACK && event.frame.seq == pending_ack_seq) 
                        {
                            ESP_LOGI("APP", "ACK received! seq: %d", pending_ack_seq);
                            send_state = SEND_STATE_IDLE;

                            // 写Flash
                            LoRaFrameData msg_frame = {0};
                            msg_frame.self_id = self_id;
                            msg_frame.target_id = pending_target_id;

                            struct timeval tv;
                            gettimeofday(&tv, NULL);
                            struct tm *timeinfo = localtime(&tv.tv_sec);
                            if (timeinfo->tm_year >= (2024 - 1900)) {
                                msg_frame.month = timeinfo->tm_mon + 1;   // tm_mon 范围是 0-11，需要 +1
                                msg_frame.day = timeinfo->tm_mday;        // tm_mday 范围是 1-31
                                msg_frame.hour = timeinfo->tm_hour;       // tm_hour 范围是 0-23
                                msg_frame.minute = timeinfo->tm_min;      // tm_min 范围是 0-59
                            } else {
                                msg_frame.month = 0;
                                msg_frame.day = 0;
                                msg_frame.hour = 0;
                                msg_frame.minute = 0;
                                ESP_LOGW("APP", "RTC time not synced yet, sending 00:00");
                            }

                            uint16_t pld_len = strlen(pending_payload); // payload_len
                            if (pld_len > 129) pld_len = 129; 
                            
                            msg_frame.data_len = pld_len;
                            memcpy(msg_frame.data_str, pending_payload, pld_len);
                            msg_frame.data_str[pld_len] = '\0'; // 确保字符串闭合

                            msg_frame.checksum = calculateCRC8(
                                (const uint8_t *)&msg_frame, 
                                offsetof(LoRaFrameData, data_str) + msg_frame.data_len
                            );

                            esp_err_t err = chat_storage_append(&msg_frame);
                            if (err != ESP_OK) {
                                ESP_LOGE("FLASH", "Save chat failed: %s", esp_err_to_name(err));
                                char toast_string[256];
                                snprintf(toast_string, sizeof(toast_string), "Writing Flash Failed! \nPlease contact developer. \nError Code: %s", esp_err_to_name(err));
                                show_toast_dialog(toast_string, 8000);
                            } else {
                                ESP_LOGI("FLASH", "Chat saved to flash, len: %d", pld_len);
                            }

                            // 更新UI
                            lv_textarea_set_text(g_ta, "");
                            input_remaining_chars = 120;
                            lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                            ui_chat_append_new_message(&msg_frame);
                        }
                    }
                    else
                    {
                        if (event.frame.target_id == 0xFF && event.frame.data_len >= 2 && strncmp(event.frame.data_str, "HB", 2) == 0) // 收到心跳
                        {
                            // 收到心跳广播：更新时间戳
                            struct timeval tv;
                            gettimeofday(&tv, NULL);
                            struct tm *timeinfo = localtime(&tv.tv_sec);

                            uint8_t cur_month = 0, cur_day = 0, cur_hour = 0, cur_minute = 0;
                            if (timeinfo->tm_year >= (2024 - 1900)) {
                                cur_month = timeinfo->tm_mon + 1;
                                cur_day = timeinfo->tm_mday;
                                cur_hour = timeinfo->tm_hour;
                                cur_minute = timeinfo->tm_min;
                            }
                            
                            chat_list[event.frame.self_id].last_time = cur_month * 1000000 + 
                                                                    cur_day * 10000 + 
                                                                    cur_hour * 100 + 
                                                                    cur_minute;
                        }
                        else { // 收到消息 UART收到消息会先发ACK再传队列 这里不需要发了
                            esp_err_t err = chat_storage_append(&event.frame);
                            if (err != ESP_OK) {
                                ESP_LOGE("FLASH", "Save chat failed: %s", esp_err_to_name(err));
                                char toast_string[256];
                                snprintf(toast_string, sizeof(toast_string), "Writing Flash Failed! \nPlease contact developer. \nError Code: %s", esp_err_to_name(err));
                                show_toast_dialog(toast_string, 8000);
                            } else {
                                ESP_LOGI("FLASH", "Chat saved to flash, len: %d", event.frame.data_len);
                            }
                            
                            if (g_chat_target_id == event.frame.self_id) {
                                ui_chat_append_new_message(&event.frame);
                            }
                            
                        }
                    }
                    if (just_woken_up_by_aux) {
                        quick_sleep_deadline_us = esp_timer_get_time() + 2000000; // 覆盖之前的 500ms 保底时间
                        just_woken_up_by_aux = false; // 消费标志
                        ESP_LOGI("APP", "UART processed after AUX wakeup, will quick sleep in 2000ms.");
                    }
                }
                else if (event.type == EVENT_KEY)
                {
                    key = event.key;
                    switch(key)
                    {   
                        case 8: // BS
                            lv_textarea_delete_char(g_ta);
                            size_t talen = strlen(lv_textarea_get_text(g_ta));
                            input_remaining_chars = 120 - talen;
                            lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                            break;
                        case 17: // SEND
                        {
                            if (send_state != SEND_STATE_IDLE) { // 正在等待ACK
                                ESP_LOGW("APP", "Busy waiting for ACK, ignore send.");
                                break; 
                            }
                            const char* payload = lv_textarea_get_text(g_ta);
                            uint8_t frame_buf[131];
                            uint8_t frame_len = buildLoRaFrame(frame_buf, self_id, g_chat_target_id, payload);
                            // 之前为了输出一下帧 现在用不上了
                            // for (int i = 0; i < frame_len; i++) {
                            //     if (frame_buf[i] < 0x10) {
                            //         printf("0");
                            //     }
                            //     printf("%02X ", frame_buf[i]);
                            // }
                            // printf("\n");
                            // printf("Frame Length: %d bytes\n", frame_len);
                            send_lora_packet(g_chat_target_id, frame_buf, frame_len);

                            pending_ack_seq = frame_buf[2]; // 从构建好的帧中提取seq
                            pending_target_id = g_chat_target_id;     // 缓存目标 ID
                            strncpy(pending_payload, payload, 120);
                            pending_payload[120] = '\0';

                            send_state = SEND_STATE_WAITING_ACK;
                            send_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                            // 等待 ACK (转 EVENT_UART)
                            break;
                        }
                        case 5: // INPUTLOCK
                            switch (lockfun) {
                                case 1: // MENU
                                    ui_show_menu_page();
                                    break;
                                case 2: // UP
                                    lv_textarea_cursor_up(g_ta);
                                    break;
                                case 3: // SETTINGS
                                    ui_show_settings_page();
                                    break;
                                case 4: // HOME
                                    lv_textarea_set_cursor_pos(g_ta, 0);
                                    break;
                                case 5: // LEFT
                                    lv_textarea_cursor_left(g_ta);
                                    break;
                                case 6: // CONFIRM
                                    if (current_page_id == PAGE_MENU) {
                                        ui_show_chat_page(menu_get_chat_id());
                                    } else if (current_page_id == PAGE_SETTINGS) {
                                        ui_show_settings_detail_page();
                                    } else if (current_page_id == PAGE_SETTINGS_DETAIL) {
                                        handle_settings_detail_enter(); // 神奇吧 AI起的函数名
                                    }
                                    break;
                                case 7: // RIGHT
                                    lv_textarea_cursor_right(g_ta);
                                    break;
                                case 8: // END
                                    lv_textarea_set_cursor_pos(g_ta, LV_TEXTAREA_CURSOR_LAST);
                                    break;
                                case 9: // BACK
                                    if (current_page_id ==  PAGE_SETTINGS_DETAIL) {
                                        ui_show_settings_page();
                                    }
                                    break;
                                case 10: // DOWN
                                    lv_textarea_cursor_down(g_ta);
                                    break;
                                case 11: // DELETE
                                    lv_textarea_delete_char_forward(g_ta);
                                    size_t talen = strlen(lv_textarea_get_text(g_ta));
                                    input_remaining_chars = 120 - talen;
                                    lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                                    break;
                                case 12: // PGUP
                                    break;
                                case 16: // PGDOWN
                                    break; 
                            }
                            break;
                        default: // 正常字符输入
                            if (input_remaining_chars > 0) {
                                lv_textarea_add_char(g_ta, key);
                                input_remaining_chars--;
                                lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                            }
                            break;
                    }
                } else if (event.type == EVENT_WIFI) {
                    current_wifi_state = event.wifi_state;
                } 
            }

            if (true) {
                if (current_wifi_state == WIFI_STATE_AP_RUNNING) {
                    // 非阻塞检查网页端是否已提交配置
                    if (wifi_check_ap_config_done()) {
                        ESP_LOGI("APP", "Web config received, switching to STA...");
                        
                        // 更新 UI 提示
                        if (detail_title_label != NULL) lv_label_set_text(detail_title_label, "Config saved!\nConnecting to WiFi...");
                        
                        // 状态流转
                        current_wifi_state = WIFI_STATE_CONNECTING_STA;
                        
                        // 停止 AP (会自动关闭 Web Server)
                        wifi_ap_stop();
                        vTaskDelay(pdMS_TO_TICKS(200)); // 短暂延时确保底层资源释放
                    }
                }
                else if (current_wifi_state == WIFI_STATE_CONNECTING_STA) {
                    if (check_wifi_saved()) {
                        // 尝试连接 STA (这里会阻塞最多 15 秒，但因为是刚点击配置，用户有心理准备)
                        if (wifi_time_connect(NULL, NULL)) {
                            if (detail_title_label != NULL) lv_label_set_text(detail_title_label, "WiFi Connected!\nSyncing time...");
                            
                            // 【关键流转】进入 SNTP 等待状态，并异步启动 SNTP
                            current_wifi_state = WIFI_STATE_WAITING_SNTP;
                            sntp_start_time = xTaskGetTickCount();
                            wifi_sntp_start(); 
                        } else {
                            if (detail_title_label != NULL) lv_label_set_text(detail_title_label, "Connect Failed!\nCheck password.");
                            wifi_time_close();
                            current_wifi_state = WIFI_STATE_IDLE;
                        }
                    } else {
                        if (detail_title_label != NULL) lv_label_set_text(detail_title_label, "No saved WiFi configuration found.\nPlease configure WiFi first.");
                        current_wifi_state = WIFI_STATE_IDLE;
                    }
                }
                else if (current_wifi_state == WIFI_STATE_WAITING_SNTP) {
                    // 【非阻塞轮询】检查 SNTP 回调是否已触发
                    if (wifi_sntp_is_synced()) {
                        if (detail_title_label != NULL) lv_label_set_text(detail_title_label, "Time Synced!\nClosing WiFi...");
                        
                        // 同步成功，立即关闭 WiFi 射频省电！
                        wifi_time_close(); 
                        
                        if (detail_title_label != NULL) lv_label_set_text(detail_title_label, "Complete!");
                        current_wifi_state = WIFI_STATE_CONNECTED;
                        
                        // 1.5秒后自动返回设置主页
                        static uint32_t complete_time = 0;
                        if (complete_time == 0) complete_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                        
                        if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - complete_time >= 1500) {
                            complete_time = 0;
                            current_wifi_state = WIFI_STATE_IDLE;
                            ui_show_settings_page(); 
                        }
                    }
                    else if ((xTaskGetTickCount() - sntp_start_time) > pdMS_TO_TICKS(15000)) {
                        lv_label_set_text(detail_title_label, "Time Sync Failed!");
                        wifi_time_close();
                        current_wifi_state = WIFI_STATE_IDLE;
                    }
                }
            }

            { // 睡觉检查
                // 安全检查：只有在非关键业务状态下才允许进入睡眠
                // 1. 没有正在等待 LoRa 的 ACK
                // 2. WiFi 处于空闲状态
                bool is_system_idle = (send_state == SEND_STATE_IDLE) && 
                                    (current_wifi_state == WIFI_STATE_IDLE);

                bool should_sleep = false;
                
                // 优先级 1：检查“收到消息后的快速睡眠”
                if (quick_sleep_deadline_us > 0) {
                    if (esp_timer_get_time() >= quick_sleep_deadline_us) {
                        should_sleep = true;
                        quick_sleep_deadline_us = 0; // 消费掉标志，准备入睡
                    }
                } 
                // 优先级 2：检查“无操作超时睡眠”
                else if (is_system_idle && check_and_enter_sleep()) {
                    should_sleep = true;
                }

                if (is_system_idle && should_sleep) {
                    int wakeup_src = enter_light_sleep();
                    if (wakeup_src == 2) {
                        just_woken_up_by_aux = true; 
                        // 【保底】如果 AUX 误唤醒，5000ms 内没收到 UART 数据，也强制睡觉
                        quick_sleep_deadline_us = esp_timer_get_time() + 5000000;
                    }
                }
            }

            // { // 电量检测
            //     uint32_t current_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
            //     if (current_tick - last_battery_check_time >= 30000) { 
            //         last_battery_check_time = current_tick;
                    
            //         // 确保 LoRa 和 WiFi 没在发射 (防止大电流拉低电压导致电量显示暴跌)
            //         if (send_state == SEND_STATE_IDLE && current_wifi_state == WIFI_STATE_IDLE) {
            //             float voltage = 0, current = 0;
                        
            //             // 调用封装好的函数：上电 -> 读取 -> 断电 (全程约 30ms)
            //             ina219_get_data_once(&voltage, &current);
                        
            //             if (voltage > 2.5f && voltage < 4.3f) {
            //                 uint8_t new_soc = battery_voltage_to_percent(voltage);
                            
            //                 // 防抖动：电量变化超过 1% 才更新 UI
            //                 if (abs((int)new_soc - (int)current_soc) >= 1) {
            //                     current_soc = new_soc;
            //                     char soc_text[5];
            //                     snprintf(soc_text, sizeof(soc_text), "%u%%", new_soc);
            //                     lv_label_set_text(soc_label, soc_text);
            //                     ESP_LOGI("BATTERY", "V: %.2fV, I: %.2fA, SoC: %d%%", voltage, current, current_soc);
            //                 }
            //             }
            //         }
            //     }
            // }

            {
                uint32_t current_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (current_tick - last_time_check_time >= 2000) {
                    last_time_check_time = current_tick;
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    struct tm *timeinfo = localtime(&tv.tv_sec);
                    if (timeinfo->tm_year < 124) {
                        lv_label_set_text(time_label, "Set Time First!");
                        // 隐藏对象（参数1: 对象的指针）
                        lv_obj_add_flag(title_label, LV_OBJ_FLAG_HIDDEN);
                    } else {
                        if (lv_obj_has_flag(title_label, LV_OBJ_FLAG_HIDDEN)) {
                            lv_obj_remove_flag(title_label, LV_OBJ_FLAG_HIDDEN);
                        }
                        char time_text[12];
                        strftime(time_text, sizeof(time_text), "%m/%d %H:%M", timeinfo);
                        lv_label_set_text(time_label, time_text);
                    }
                    ESP_LOGI("TIME", "CT: %u", current_tick);
                }
            }
        }
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void peripheral_init_task(void *arg)
{
    lv_async_call(update_boot_label_cb, "Initializing Keyboard...");
    key_init();
    ESP_LOGI("KEY", "Initialized Keyboard...");
    vTaskDelay(pdMS_TO_TICKS(50));
    lv_async_call(update_boot_label_cb, "Initializing UART...");
    uart_init();
    ESP_LOGI("UART", "Initialized UART...");
    vTaskDelay(pdMS_TO_TICKS(50));
    lv_async_call(update_boot_label_cb, "Initializing Flash...");
    ext_flash_init();
    ESP_LOGI("FLASH", "Initialized Flash...");
    vTaskDelay(pdMS_TO_TICKS(50));
    // lv_async_call(update_boot_label_cb, "Initializing I2C...");
    // i2c_master_init();
    // ESP_LOGI("I2C", "Initialized I2C...");
    vTaskDelay(pdMS_TO_TICKS(50));
    lv_async_call(update_boot_label_cb, "Initializing NVS...");
    nvs_init();
    ESP_LOGI("NVS", "Initialized NVS...");
    vTaskDelay(pdMS_TO_TICKS(50));
    lv_async_call(update_boot_label_cb, "Reading NVS...");
    nvs_read_all_alias_to_list(); // 先读alias让chat_list里面的id被填上
    ESP_LOGI("NVS", "Reading NVS...");
    nvs_read_all_user_color_to_list();
    ESP_LOGI("NVS", "Reading NVS...");
    nvs_read_all_interface_color_to_list();
    ESP_LOGI("NVS", "Reading NVS...");
    nvs_read_sleep_time();
    ESP_LOGI("NVS", "Reading NVS...");
    nvs_read_brightness();
    ESP_LOGI("NVS", "Reading NVS...");
    vTaskDelay(pdMS_TO_TICKS(50));
    lv_async_call(update_boot_label_cb, "Restoring Flash Offset...");
    uint32_t valid_len = chat_storage_scan();
    g_write_offset = valid_len;
    ESP_LOGI("FLASH", "Restoring Flash Offset...");
    vTaskDelay(pdMS_TO_TICKS(50));
    lv_async_call(update_boot_label_cb, "Updating Latest Message Time...");
    update_chat_list_last_time();
    ESP_LOGI("FLASH", "Updating Latest Message Time...");
    vTaskDelay(pdMS_TO_TICKS(50));

    // 设置事件标志，通知 UI 任务初始化已完成
    xEventGroupSetBits(init_event_group, INIT_DONE_BIT);

    // 任务完成，销毁自身释放资源
    vTaskDelete(NULL);
}

void app_main(void)
{
    appQueue = xQueueCreate(10, sizeof(UIEvent));
    init_event_group = xEventGroupCreate();

    // 初始化显示外设
    pwm_init();
    spi_init();
    lcd_init();
    ui_init();

    scr_loading = lv_obj_create(NULL);
    lv_obj_t *spinner = lv_spinner_create(scr_loading);
    lv_spinner_set_anim_params(spinner, 1000, 270); // 1000ms 转一圈，圆弧角度 270 度
    lv_obj_set_size(spinner, 80, 80);
    lv_obj_center(spinner);
    
    boot_text_label = lv_label_create(scr_loading);
    lv_label_set_text(boot_text_label, "System Booting...");
    lv_obj_center(boot_text_label);
    lv_obj_align_to(
        boot_text_label,
        spinner,
        LV_ALIGN_OUT_BOTTOM_MID,
        0,
        20);
    
    lv_screen_load(scr_loading); // 加载开机动画屏幕

    last_activity_time_us = esp_timer_get_time();

    ESP_LOGI("SLEEP_BOOT", "auto_sleep_timeout: %u", auto_sleep_timeout);

    // 创建LVGL Tick定时器
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lvgl_tick_cb,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
    esp_timer_start_periodic(periodic_timer, 1000); // 1000us
    
    // xTaskCreatePinnedToCore(lv_timer_handler_task, "lvgl_t_h", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(app_main_task, "app_main", 8192, NULL, 5, NULL, 1);

    vTaskDelay(pdMS_TO_TICKS(50));
    
    xTaskCreatePinnedToCore(peripheral_init_task, "periph_init", 4096, NULL, 5, NULL, 0);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}