#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "pwm.h"
#include "spi.h"
#include "freertos/semphr.h"
#include "uart.h"
#include "lcd.h"
#include "ui.h"
#include "va.h"
#include "key.h"
#include "i2c.h"
#include "flash.h"
#include "sleep.h"
#include "mynvs.h"
#include "wifi.h"

#define INIT_DONE_BIT (1 << 0)             // 定义初始化完成的事件位
#define ACK_TIMEOUT_MS 3000                // ACK 超时时间为 3 秒
#define WIFI_AP_TIMEOUT_MS (5 * 60 * 1000) // AP 配网超时时间 5 分钟

static SemaphoreHandle_t lvgl_mutex = NULL;
static EventGroupHandle_t init_event_group;
static lv_obj_t *scr_loading = NULL; // 用于保存开机动画界面对象，以便后续释放内存
lv_obj_t *boot_text_label;

static SendState send_state = SEND_STATE_IDLE;
static uint8_t pending_ack_seq = 0;       // 记录等待匹配的随机数
static uint8_t pending_target_id = 0;     // 缓存目标ID，用于收到ACK后写Flash
static uint32_t send_start_time = 0;      // 记录发送开始的时间戳
static char pending_payload[121] = {0};   // 缓存发送的内容
static uint32_t retry_delay_end_time = 0; // 重试间隔结束时间 (0=不等待)
static uint8_t last_seen_seq[256];        // 每个 self_id 最后收到的 seq (去重用)
static bool seen_peer[256];               // 记录是否曾收到该 self_id 的消息
static uint32_t ap_start_tick = 0;        // AP 配网启动时刻 (ms), 0=未在配网

static void update_boot_label_cb(void *arg)
{
    char *text = (char *)arg;
    if (boot_text_label != NULL && text != NULL)
    {
        lv_label_set_text(boot_text_label, text);
    }
}

static void async_boot_label(const char *text)
{
    if (lvgl_mutex)
        xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
    lv_async_call(update_boot_label_cb, (void *)text);
    if (lvgl_mutex)
        xSemaphoreGiveRecursive(lvgl_mutex);
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
    uint32_t last_battery_check_time = 0;
    uint32_t last_time_check_time = 0;
    uint32_t last_mem_check_time = 0;
    uint8_t current_soc = 100;
    uint32_t sntp_start_time = 0;

    while (1)
    {
        // 非阻塞检查外设是否初始化完成，进行 UI 切换
        if (!is_initialized)
        {
            EventBits_t bits = xEventGroupGetBits(init_event_group);
            if (bits & INIT_DONE_BIT)
            {
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
                heartbeat_init();
            }
        }

        // 只有在初始化完成后，才处理业务事件 (避免初始化期间处理无效数据)
        if (is_initialized)
        {
            // 等待 ACK
            if (send_state == SEND_STATE_WAITING_ACK)
            {
                uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

                if (retry_delay_end_time > 0)
                {
                    if (current_time >= retry_delay_end_time)
                    {
                        retry_delay_end_time = 0;
                        lora_status = LORA_STATUS_SENDING;
                        update_lora_status_indicator();
                        uint8_t frame_buf[131];
                        uint8_t frame_len = buildLoRaFrame(frame_buf, self_id, pending_target_id, pending_payload);
                        frame_buf[2] = pending_ack_seq;
                        frame_buf[10 + frame_buf[9]] = calculateCRC8(&frame_buf[2], 8 + frame_buf[9]);
                        send_lora_packet(pending_target_id, frame_buf, frame_len);
                        lora_status = LORA_STATUS_WAITING_ACK;
                        update_lora_status_indicator();
                        send_start_time = current_time;
                    }
                }
                else if (current_time - send_start_time >= ACK_TIMEOUT_MS)
                {
                    ack_timeout_count++;
                    ESP_LOGW("APP", "ACK timeout %d/5! seq: %d", ack_timeout_count, pending_ack_seq);
                    if (ack_timeout_count >= 5)
                    {
                        send_state = SEND_STATE_IDLE;
                        lora_status = LORA_STATUS_IDLE;
                        ack_timeout_count = 0;
                        update_lora_status_indicator();
                        char toast_string[256];
                        snprintf(toast_string, sizeof(toast_string), "Message failed to send! \nPlease check if the recipient is online!");
                        show_toast_dialog(toast_string, 8000);
                    }
                    else
                    {
                        lora_status = LORA_STATUS_TIMEOUT;
                        update_lora_status_indicator();
                        retry_delay_end_time = current_time + 500;
                    }
                }
            }
            // 读队列
            if (xQueueReceive(appQueue, &event, 0))
            {
                if (event.type == EVENT_UART)
                {
                    if (event.frame.data_len == 0)
                    { // 收到ACK 说明自己发出去了
                        if (send_state == SEND_STATE_WAITING_ACK && event.frame.seq == pending_ack_seq)
                        {
                            ESP_LOGI("APP", "ACK received! seq: %d", pending_ack_seq);
                            send_state = SEND_STATE_IDLE;
                            ack_timeout_count = 0;
                            retry_delay_end_time = 0;

                            // 写Flash
                            LoRaFrameData msg_frame = {0};
                            msg_frame.self_id = self_id;
                            msg_frame.target_id = pending_target_id;

                            struct timeval tv;
                            gettimeofday(&tv, NULL);
                            struct tm *timeinfo = localtime(&tv.tv_sec);
                            if (timeinfo->tm_year >= (2024 - 1900))
                            {
                                msg_frame.month = timeinfo->tm_mon + 1; // tm_mon 范围是 0-11，需要 +1
                                msg_frame.day = timeinfo->tm_mday;      // tm_mday 范围是 1-31
                                msg_frame.hour = timeinfo->tm_hour;     // tm_hour 范围是 0-23
                                msg_frame.minute = timeinfo->tm_min;    // tm_min 范围是 0-59
                            }
                            else
                            {
                                msg_frame.month = 0;
                                msg_frame.day = 0;
                                msg_frame.hour = 0;
                                msg_frame.minute = 0;
                                ESP_LOGW("APP", "RTC time not synced yet, sending 00:00");
                            }

                            uint16_t pld_len = strlen(pending_payload); // payload_len
                            if (pld_len > 129)
                                pld_len = 129;

                            msg_frame.data_len = pld_len;
                            memcpy(msg_frame.data_str, pending_payload, pld_len);
                            msg_frame.data_str[pld_len] = '\0'; // 确保字符串闭合

                            msg_frame.checksum = calculateCRC8(
                                (const uint8_t *)&msg_frame,
                                offsetof(LoRaFrameData, data_str) + msg_frame.data_len);

                            uint32_t sent_packed = (uint32_t)msg_frame.month * 1000000 +
                                                   (uint32_t)msg_frame.day * 10000 +
                                                   (uint32_t)msg_frame.hour * 100 +
                                                   (uint32_t)msg_frame.minute;
                            if (sent_packed > chat_list[pending_target_id].last_time)
                            {
                                chat_list[pending_target_id].last_time = sent_packed;
                                if (current_page_id == PAGE_MENU)
                                    menu_refresh();
                            }

                            esp_err_t err = chat_storage_append(&msg_frame);
                            if (err != ESP_OK)
                            {
                                ESP_LOGE("FLASH", "Save chat failed: %s", esp_err_to_name(err));
                                char toast_string[256];
                                snprintf(toast_string, sizeof(toast_string), "Writing Flash Failed! \nPlease contact developer. \nError Code: %s", esp_err_to_name(err));
                                show_toast_dialog(toast_string, 8000);
                            }
                            else
                            {
                                ESP_LOGI("FLASH", "Chat saved to flash, len: %d", pld_len);
                            }

                            // 更新UI
                            lv_textarea_set_text(g_ta, "");
                            input_remaining_chars = 120;
                            lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                            ui_chat_append_new_message(&msg_frame);
                            lora_status = LORA_STATUS_IDLE;
                            update_lora_status_indicator();
                        }
                        else
                        {
                            lora_status = LORA_STATUS_IDLE;
                            update_lora_status_indicator();
                        }
                    }
                    else
                    {
                        if (event.frame.target_id == 0xFF && event.frame.data_len >= 2 && strncmp(event.frame.data_str, "HB", 2) == 0) // 收到心跳
                        {
                            // 收到心跳广播：更新时间戳
                            if (event.frame.self_id != self_id)
                            {
                                struct timeval tv;
                                gettimeofday(&tv, NULL);
                                struct tm *timeinfo = localtime(&tv.tv_sec);
                                uint32_t online_packed;
                                if (timeinfo->tm_year >= (2024 - 1900))
                                {
                                    online_packed =
                                        (uint32_t)(timeinfo->tm_mon + 1) * 1000000 +
                                        (uint32_t)timeinfo->tm_mday * 10000 +
                                        (uint32_t)timeinfo->tm_hour * 100 +
                                        (uint32_t)timeinfo->tm_min;
                                }
                                else
                                {
                                    online_packed =
                                        (uint32_t)event.frame.month * 1000000 +
                                        (uint32_t)event.frame.day * 10000 +
                                        (uint32_t)event.frame.hour * 100 +
                                        (uint32_t)event.frame.minute;
                                }
                                chat_list[event.frame.self_id].last_online = online_packed;
                                chat_list[0x00].last_online = online_packed;
                                if (current_page_id == PAGE_MENU)
                                    menu_refresh();
                            }
                            lora_status = LORA_STATUS_IDLE;
                            update_lora_status_indicator();
                        }
                        else
                        { // 收到消息 UART收到消息会先发ACK再传队列 这里不需要发了
                            if (event.frame.data_len > 0 && seen_peer[event.frame.self_id] && last_seen_seq[event.frame.self_id] == event.frame.seq)
                            {
                                ESP_LOGI("APP", "Dup msg from %02X seq=%d, skip", event.frame.self_id, event.frame.seq);
                            }
                            else
                            {
                                seen_peer[event.frame.self_id] = true;
                                last_seen_seq[event.frame.self_id] = event.frame.seq;
                                uint32_t packed = (uint32_t)event.frame.month * 1000000 +
                                                  (uint32_t)event.frame.day * 10000 +
                                                  (uint32_t)event.frame.hour * 100 +
                                                  (uint32_t)event.frame.minute;
                                if (packed > chat_list[event.frame.self_id].last_time)
                                {
                                    chat_list[event.frame.self_id].last_time = packed;
                                    if (current_page_id == PAGE_MENU)
                                        menu_refresh();
                                }
                                struct timeval tv_now;
                                gettimeofday(&tv_now, NULL);
                                struct tm *tm_now = localtime(&tv_now.tv_sec);
                                uint32_t online_packed;
                                if (tm_now->tm_year >= (2024 - 1900))
                                {
                                    online_packed =
                                        (uint32_t)(tm_now->tm_mon + 1) * 1000000 +
                                        (uint32_t)tm_now->tm_mday * 10000 +
                                        (uint32_t)tm_now->tm_hour * 100 +
                                        (uint32_t)tm_now->tm_min;
                                }
                                else
                                {
                                    online_packed =
                                        (uint32_t)event.frame.month * 1000000 +
                                        (uint32_t)event.frame.day * 10000 +
                                        (uint32_t)event.frame.hour * 100 +
                                        (uint32_t)event.frame.minute;
                                }
                                chat_list[event.frame.self_id].last_online = online_packed;

                                // ALL 广播消息：同时更新 ALL 条目的时间
                                if (event.frame.target_id == 0x00)
                                {
                                    if (packed > chat_list[0x00].last_time)
                                    {
                                        chat_list[0x00].last_time = packed;
                                    }
                                    chat_list[0x00].last_online = online_packed;
                                }
                                esp_err_t err = chat_storage_append(&event.frame);
                                if (err != ESP_OK)
                                {
                                    ESP_LOGE("FLASH", "Save chat failed: %s", esp_err_to_name(err));
                                    char toast_string[256];
                                    snprintf(toast_string, sizeof(toast_string), "Writing Flash Failed! \nPlease contact developer. \nError Code: %s", esp_err_to_name(err));
                                    show_toast_dialog(toast_string, 8000);
                                }
                                else
                                {
                                    ESP_LOGI("FLASH", "Chat saved to flash, len: %d", event.frame.data_len);
                                }

                                if (g_chat_target_id == event.frame.self_id ||
                                    (g_chat_target_id == 0x00 && event.frame.target_id == 0x00))
                                {
                                    ui_chat_append_new_message(&event.frame);
                                }
                                lora_status = LORA_STATUS_IDLE;
                                update_lora_status_indicator();
                            }
                        }
                    }
                }
                else if (event.type == EVENT_KEY)
                {
                    key = event.key;
                    switch (key)
                    {
                    case 20: // KEY_STATE_CHANGE sentinel
                        break;
                    case 19: // LOCK_TOGGLE sentinel
                        break;
                    case 8: // BS
                        lv_textarea_delete_char(g_ta);
                        size_t talen = strlen(lv_textarea_get_text(g_ta));
                        input_remaining_chars = 120 - talen;
                        lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                        break;
                    case 17: // SEND
                    {
                        if (current_page_id != PAGE_CHAT)
                            break;
                        if (send_state != SEND_STATE_IDLE)
                        { // 正在等待ACK
                            ESP_LOGW("APP", "Busy waiting for ACK, ignore send.");
                            break;
                        }
                        const char *payload = lv_textarea_get_text(g_ta);
                        uint8_t frame_buf[131];
                        uint8_t lora_target = (g_chat_target_id == 0x00) ? 0xFF : g_chat_target_id;
                        uint8_t frame_len = buildLoRaFrame(frame_buf, self_id, g_chat_target_id, payload);
                        lora_status = LORA_STATUS_SENDING;
                        update_lora_status_indicator();
                        send_lora_packet(lora_target, frame_buf, frame_len);

                        if (g_chat_target_id == 0x00)
                        {
                            // ALL 广播：fire-and-forget，立即写闪存更新UI
                            lora_status = LORA_STATUS_IDLE;
                            update_lora_status_indicator();

                            LoRaFrameData msg_frame = {0};
                            msg_frame.self_id = self_id;
                            msg_frame.target_id = 0x00;
                            struct timeval tv;
                            gettimeofday(&tv, NULL);
                            struct tm *timeinfo = localtime(&tv.tv_sec);
                            if (timeinfo->tm_year >= (2024 - 1900))
                            {
                                msg_frame.month = timeinfo->tm_mon + 1;
                                msg_frame.day = timeinfo->tm_mday;
                                msg_frame.hour = timeinfo->tm_hour;
                                msg_frame.minute = timeinfo->tm_min;
                            }
                            memcpy(msg_frame.data_str, payload, strlen(payload));
                            msg_frame.data_len = strlen(payload);
                            chat_storage_append(&msg_frame);

                            uint32_t sent_packed = (uint32_t)msg_frame.month * 1000000 +
                                                   (uint32_t)msg_frame.day * 10000 +
                                                   (uint32_t)msg_frame.hour * 100 +
                                                   (uint32_t)msg_frame.minute;
                            if (sent_packed > chat_list[0x00].last_time)
                            {
                                chat_list[0x00].last_time = sent_packed;
                                if (current_page_id == PAGE_MENU)
                                    menu_refresh();
                            }

                            ui_chat_append_new_message(&msg_frame);
                            lv_textarea_set_text(g_ta, "");
                            input_remaining_chars = 120;
                            lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                            break;
                        }

                        pending_ack_seq = frame_buf[2];       // 从构建好的帧中提取seq
                        pending_target_id = g_chat_target_id; // 缓存目标 ID
                        strncpy(pending_payload, payload, 120);
                        pending_payload[120] = '\0';

                        send_state = SEND_STATE_WAITING_ACK;
                        lora_status = LORA_STATUS_WAITING_ACK;
                        update_lora_status_indicator();
                        send_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                        // 等待 ACK (转 EVENT_UART)
                        break;
                    }
                    case 5: // INPUTLOCK
                        switch (lockfun)
                        {
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
                            if (current_page_id == PAGE_MENU)
                            {
                                ui_show_chat_page(menu_get_chat_id());
                            }
                            else if (current_page_id == PAGE_SETTINGS)
                            {
                                ui_show_settings_detail_page();
                            }
                            else if (current_page_id == PAGE_SETTINGS_DETAIL)
                            {
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
                            if (current_page_id == PAGE_CHAT)
                            {
                                ui_show_menu_page();
                            }
                            else if (current_page_id == PAGE_SETTINGS_DETAIL)
                            {
                                lv_textarea_set_placeholder_text(g_ta, "Please Input...");
                                input_remaining_chars = 120;
                                lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                                lv_textarea_set_text(g_ta, "");
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
                            if (current_page_id == PAGE_MENU)
                            {
                                menu_up();
                            }
                            else if (current_page_id == PAGE_SETTINGS)
                            {
                                settings_up();
                            }
                            else if (current_page_id == PAGE_CHAT)
                            {
                                lv_obj_scroll_by_bounded(g_chat_scroll_container, 0,
                                                         (lv_obj_get_height(g_chat_scroll_container) - 10), LV_ANIM_ON);
                            }
                            break;
                        case 16: // PGDOWN
                            if (current_page_id == PAGE_MENU)
                            {
                                menu_down();
                            }
                            else if (current_page_id == PAGE_SETTINGS)
                            {
                                settings_down();
                            }
                            else if (current_page_id == PAGE_CHAT)
                            {
                                lv_obj_scroll_by_bounded(g_chat_scroll_container, 0,
                                                         -lv_obj_get_height(g_chat_scroll_container) - 10, LV_ANIM_ON);
                            }
                            break;
                        }
                        break;
                    default: // 正常字符输入
                        if (input_remaining_chars > 0)
                        {
                            lv_textarea_add_char(g_ta, key);
                            input_remaining_chars--;
                            lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                        }
                        break;
                    }
                    update_keyboard_icons();
                }
                else if (event.type == EVENT_WIFI)
                {
                    current_wifi_state = event.wifi_state;
                }
            }

            {
                if (current_wifi_state == WIFI_STATE_AP_RUNNING)
                {
                    if (ap_start_tick == 0)
                        ap_start_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;

                    // 非阻塞检查网页端是否已提交配置
                    if (wifi_check_ap_config_done())
                    {
                        ap_start_tick = 0;
                        last_activity_time_us = esp_timer_get_time(); // 别配一半网睡着了
                        ESP_LOGI("APP", "Web config received, switching to STA...");

                        // 更新 UI 提示
                        if (detail_title_label != NULL)
                            lv_label_set_text(detail_title_label, "Config saved!\nConnecting to WiFi...");

                        // 状态流转
                        current_wifi_state = WIFI_STATE_CONNECTING_STA;

                        // 停止 AP (会自动关闭 Web Server)
                        wifi_ap_stop();
                        vTaskDelay(pdMS_TO_TICKS(200)); // 短暂延时确保底层资源释放
                    }
                    else if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - ap_start_tick >= WIFI_AP_TIMEOUT_MS)
                    {
                        // 5 分钟未收到配置，自动关闭 AP 恢复空闲
                        ESP_LOGI("APP", "AP config timeout, closing AP");
                        ap_start_tick = 0;
                        if (detail_title_label != NULL)
                            lv_label_set_text(detail_title_label, "Config timeout!\nAP closed.");
                        wifi_ap_stop();
                        current_wifi_state = WIFI_STATE_IDLE;
                    }
                }
                else if (current_wifi_state == WIFI_STATE_CONNECTING_STA)
                {
                    if (check_wifi_saved())
                    {
                        last_activity_time_us = esp_timer_get_time(); // 别配一半网睡着了
                        // 尝试连接 STA (这里会阻塞最多 15 秒，但因为是刚点击配置，用户有心理准备)
                        if (wifi_time_connect(NULL, NULL))
                        {
                            if (detail_title_label != NULL)
                                lv_label_set_text(detail_title_label, "WiFi Connected!\nSyncing time...");

                            // 【关键流转】进入 SNTP 等待状态，并异步启动 SNTP
                            current_wifi_state = WIFI_STATE_WAITING_SNTP;
                            sntp_start_time = xTaskGetTickCount();
                            wifi_sntp_start();
                        }
                        else
                        {
                            if (detail_title_label != NULL)
                                lv_label_set_text(detail_title_label, "Connect Failed!\nCheck password.");
                            wifi_time_close();
                            current_wifi_state = WIFI_STATE_IDLE;
                        }
                    }
                    else
                    {
                        if (detail_title_label != NULL)
                            lv_label_set_text(detail_title_label, "No saved WiFi configuration found.\nPlease configure WiFi first.");
                        current_wifi_state = WIFI_STATE_IDLE;
                    }
                }
                else if (current_wifi_state == WIFI_STATE_WAITING_SNTP)
                {
                    // 【非阻塞轮询】检查 SNTP 回调是否已触发
                    if (wifi_sntp_is_synced())
                    {
                        last_activity_time_us = esp_timer_get_time(); // 别配一半网睡着了
                        if (detail_title_label != NULL)
                            lv_label_set_text(detail_title_label, "Time Synced!\nClosing WiFi...");

                        // 同步成功，立即关闭 WiFi 射频省电！
                        wifi_time_close();

                        if (detail_title_label != NULL)
                            lv_label_set_text(detail_title_label, "Complete!");
                        current_wifi_state = WIFI_STATE_CONNECTED;
                    }
                    else if ((xTaskGetTickCount() - sntp_start_time) > pdMS_TO_TICKS(15000))
                    {
                        lv_label_set_text(detail_title_label, "Time Sync Failed!");
                        wifi_time_close();
                        current_wifi_state = WIFI_STATE_IDLE;
                    }
                }
                else if (current_wifi_state == WIFI_STATE_CONNECTED)
                {
                    static uint32_t complete_time = 0;
                    if (complete_time == 0)
                        complete_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

                    if ((xTaskGetTickCount() * portTICK_PERIOD_MS) - complete_time >= 1500)
                    {
                        complete_time = 0;
                        current_wifi_state = WIFI_STATE_IDLE;
                        ui_show_settings_page();
                        last_activity_time_us = esp_timer_get_time(); // 别配一半网睡着了
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
                if (quick_sleep_deadline_us > 0)
                {
                    if (esp_timer_get_time() >= quick_sleep_deadline_us)
                    {
                        should_sleep = true;
                        quick_sleep_deadline_us = 0; // 消费掉标志，准备入睡
                    }
                }
                // 优先级 2：检查“无操作超时睡眠”
                else if (is_system_idle && check_and_enter_sleep())
                {
                    should_sleep = true;
                }

                if (is_system_idle && should_sleep)
                {
                    uint64_t hb_deadline = heartbeat_get_next_deadline_us();
                    uint64_t now_us = esp_timer_get_time();
                    uint64_t timer_us = (hb_deadline > now_us) ? (hb_deadline - now_us) : 0;
                    int wakeup_src = enter_light_sleep(timer_us);
                    if (wakeup_src == 2 || wakeup_src == 3)
                    {
                        quick_sleep_deadline_us = esp_timer_get_time() + 5000000;
                    }
                }
            }

            { // 电量检测
                uint32_t current_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (current_tick - last_battery_check_time >= 30000)
                {
                    last_battery_check_time = current_tick;

                    // 确保 LoRa 和 WiFi 没在发射 (防止大电流拉低电压导致电量显示暴跌)
                    if (send_state == SEND_STATE_IDLE && current_wifi_state == WIFI_STATE_IDLE)
                    {
                        float voltage = 0, current = 0;

                        // 调用封装好的函数：上电 -> 读取 -> 断电 (全程约 30ms)
                        ina219_get_data_once(&voltage, &current);

                        if (voltage > 2.5f && voltage < 4.3f)
                        {
                            uint8_t new_soc = battery_voltage_to_percent(voltage);

                            // 防抖动：电量变化超过 1% 才更新 UI
                            if (abs((int)new_soc - (int)current_soc) >= 1)
                            {
                                current_soc = new_soc;
                                char soc_text[5];
                                snprintf(soc_text, sizeof(soc_text), "%u%%", new_soc);
                                lv_label_set_text(soc_label, soc_text);
                                ESP_LOGI("BATTERY", "V: %.2fV, I: %.2fA, SoC: %d%%", voltage, current, current_soc);
                            }
                        }
                    }
                }
            }

            {
                uint32_t current_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (current_tick - last_mem_check_time >= 10000)
                {
                    last_mem_check_time = current_tick;
                    size_t free_heap = esp_get_free_heap_size();
                    size_t min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
                    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
                    ESP_LOGI("MEM", "Free: %u KB  Min: %u KB  PSRAM: %u KB",
                             (unsigned)(free_heap / 1024), (unsigned)(min_free / 1024), (unsigned)(free_psram / 1024));
                }
            }
            {
                uint32_t current_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (current_tick - last_time_check_time >= 2000)
                {
                    last_time_check_time = current_tick;
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    struct tm *timeinfo = localtime(&tv.tv_sec);
                    if (timeinfo->tm_year < 124)
                    {
                        lv_label_set_text(time_label, "Set Time First!");
                        // 隐藏对象（参数1: 对象的指针）
                        lv_obj_add_flag(title_label, LV_OBJ_FLAG_HIDDEN);
                    }
                    else
                    {
                        if (lv_obj_has_flag(title_label, LV_OBJ_FLAG_HIDDEN))
                        {
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
        if (lvgl_mutex)
            xSemaphoreTakeRecursive(lvgl_mutex, portMAX_DELAY);
        update_lora_status_indicator();
        lv_timer_handler();
        if (lvgl_mutex)
            xSemaphoreGiveRecursive(lvgl_mutex);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void peripheral_init_task(void *arg)
{
    async_boot_label("Initializing Keyboard...");
    key_init();
    ESP_LOGI("KEY", "Initialized Keyboard...");
    vTaskDelay(pdMS_TO_TICKS(50));
    async_boot_label("Initializing UART...");
    uart_init();
    ESP_LOGI("UART", "Initialized UART...");
    vTaskDelay(pdMS_TO_TICKS(50));
    async_boot_label("Initializing Flash...");
    ext_flash_init();
    ESP_LOGI("FLASH", "Initialized Flash...");
    vTaskDelay(pdMS_TO_TICKS(50));
    async_boot_label("Initializing I2C...");
    i2c_master_init();
    ESP_LOGI("I2C", "Initialized I2C...");
    vTaskDelay(pdMS_TO_TICKS(50));
    async_boot_label("Initializing NVS...");
    nvs_init();
    ESP_LOGI("NVS", "Initialized NVS...");
    vTaskDelay(pdMS_TO_TICKS(50));
    async_boot_label("Reading NVS...");
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
    nvs_read_status_title();
    nvs_dump_all();
    vTaskDelay(pdMS_TO_TICKS(50));
    async_boot_label("Restoring Flash Offset...");
    uint32_t valid_len = chat_storage_scan();
    g_write_offset = valid_len;
    ESP_LOGI("FLASH", "Restoring Flash Offset...");
    vTaskDelay(pdMS_TO_TICKS(50));
    async_boot_label("Updating Latest Message Time...");
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
    lvgl_mutex = xSemaphoreCreateRecursiveMutex();

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
        .name = "periodic_gui"};
    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
    esp_timer_start_periodic(periodic_timer, 1000); // 1000us

    xTaskCreatePinnedToCore(app_main_task, "app_main", 8192, NULL, 5, NULL, 1);

    vTaskDelay(pdMS_TO_TICKS(50));

    xTaskCreatePinnedToCore(peripheral_init_task, "periph_init", 8192, NULL, 5, NULL, 0);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}