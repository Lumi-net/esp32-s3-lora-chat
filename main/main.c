#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "pwm.h"
#include "spi.h"
#include "uart.h"
#include "lcd.h"
#include "ui.h"
#include "LOCK.h"
#include "SHIFT.h"
#include "event.h"
#include "va.h"
#include "key.h"
#include "flash.h"

// TODO: 在状态栏加上当前状态和电量显示(INA219)
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


void app_main_task(void *arg)
{
    UIEvent event;
    uint8_t key;
    bool is_initialized = false;

    while (1)
    {
        // 非阻塞检查外设是否初始化完成，进行 UI 切换
        if (!is_initialized) {
            EventBits_t bits = xEventGroupGetBits(init_event_group);
            if (bits & INIT_DONE_BIT) {
                is_initialized = true;
                lv_label_set_text(boot_text_label, "Done!");

                create_ui();
                
                // 切换到主聊天界面
                ui_show_chat_page();
                
                // 释放开机动画界面的内存，防止内存泄漏
                if (scr_loading != NULL) {
                    lv_obj_del(scr_loading);
                    scr_loading = NULL;
                }

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
                    // TODO: 弹窗提示发送失败
                }
            }
            // 读队列
            if (xQueueReceive(appQueue, &event, 0))
            {
                if (event.type == EVENT_UART)
                {
                    if (event.frame.data_len == 0) { // 收到 ACK
                        if (send_state == SEND_STATE_WAITING_ACK && event.frame.seq == pending_ack_seq) 
                        {
                            ESP_LOGI("APP", "ACK received! seq: %d", pending_ack_seq);
                            send_state = SEND_STATE_IDLE;

                            // 写Flash
                            LoRaFrameData flash_frame = {0};
                            flash_frame.self_id = self_id;
                            flash_frame.target_id = pending_target_id;
                            flash_frame.month = 0;  // TODO: 替换为真实月份
                            flash_frame.day = 0;    // TODO: 替换为真实日期
                            flash_frame.hour = 0;   // TODO: 替换为真实小时
                            flash_frame.minute = 0; // TODO: 替换为真实分钟

                            uint16_t pld_len = strlen(pending_payload); // payload_len
                            if (pld_len > 129) pld_len = 129; 
                            
                            flash_frame.data_len = pld_len;
                            memcpy(flash_frame.data_str, pending_payload, pld_len);
                            flash_frame.data_str[pld_len] = '\0'; // 确保字符串闭合

                            flash_frame.checksum = calculateCRC8(
                                (const uint8_t *)&flash_frame, 
                                offsetof(LoRaFrameData, data_str) + flash_frame.data_len
                            );

                            esp_err_t err = chat_storage_append(&flash_frame);
                            if (err != ESP_OK) {
                                ESP_LOGE("FLASH", "Save chat failed: %s", esp_err_to_name(err));
                                char toast_string[256];
                                snprintf(toast_string, sizeof(toast_string), "写入Flash失败 但消息已发出 请联系开发者\n错误码%s", esp_err_to_name(err));
                                show_toast_dialog(toast_string, 8000);
                            } else {
                                ESP_LOGI("FLASH", "Chat saved to flash, len: %d", pld_len);
                            }

                            // 更新UI
                            lv_textarea_set_text(g_ta, "");
                            input_remaining_chars = 120;
                            lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                            chat_add_message("00:00", pending_payload);
                        }
                    }
                    else {
                        char time[6];
                        snprintf(time, sizeof(time),
                                "%02u:%02u",
                                (unsigned)(event.frame.hour % 24),
                                (unsigned)(event.frame.minute % 60));
                        chat_add_message(time, event.frame.data_str);
                        // TODO: 写Flash
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

                            uint8_t target_id = 0x02; // TODO: 以后跟着UI选的目标走
                            const char* payload = lv_textarea_get_text(g_ta);
                            uint8_t frame_buf[131];
                            uint8_t frame_len = buildLoRaFrame(frame_buf, self_id, target_id, payload);
                            // for (int i = 0; i < frame_len; i++) {
                            //     if (frame_buf[i] < 0x10) {
                            //         printf("0");
                            //     }
                            //     printf("%02X ", frame_buf[i]);
                            // }
                            // printf("\n");
                            // printf("Frame Length: %d bytes\n", frame_len);
                            uart_transmit(frame_buf, frame_len);

                            pending_ack_seq = frame_buf[2]; // 从构建好的帧中提取seq
                            pending_target_id = target_id;     // 缓存目标 ID
                            strncpy(pending_payload, payload, 120);
                            pending_payload[120] = '\0';

                            send_state = SEND_STATE_WAITING_ACK;
                            send_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                            // 等待 ACK (转 EVENT_UART)
                            break;
                        }
                        case 5:
                            switch (lockfun) {
                                case 2: // UP
                                    lv_textarea_cursor_up(g_ta);
                                    break;
                                case 4: // HOME
                                    lv_textarea_set_cursor_pos(g_ta, 0);
                                    break;
                                case 5: // LEFT
                                    lv_textarea_cursor_left(g_ta);
                                    break;
                                case 7: // RIGHT
                                    lv_textarea_cursor_right(g_ta);
                                    break;
                                case 8: // END
                                    lv_textarea_set_cursor_pos(g_ta, LV_TEXTAREA_CURSOR_LAST);
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
                            }
                            break;
                        default:
                            if (input_remaining_chars > 0) {
                                lv_textarea_add_char(g_ta, key);
                                input_remaining_chars--;
                                lv_label_set_text_fmt(input_cnt_left, "%d", input_remaining_chars);
                            }
                            break;
                    }
                }
            }
        }
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void peripheral_init_task(void *arg)
{
    lv_label_set_text(boot_text_label, "Initializing Keyboard...");
    key_init();
    lv_label_set_text(boot_text_label, "Initializing UART...");
    uart_init();
    lv_label_set_text(boot_text_label, "Initializing Flash...");
    ext_flash_init();

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
    create_ui();

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
    
    xTaskCreatePinnedToCore(peripheral_init_task, "periph_init", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(app_main_task, "app_main", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(lvgl_tick_task, "lvgl_tick", 1024, NULL, 5, NULL, 1);

    ui_show_chat_page();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}