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
#include "queue.h"
#include "key.h"

QueueHandle_t appQueue;

void app_main_task(void *arg)
{
    UIEvent event;
    uint8_t key;
    while (1)
    {
        if (xQueueReceive(appQueue, &event, 0))
        {
            if (event.type == EVENT_UART)
            {
                char time[5];
                sprintf(time, "%02d:%02d", event.frame.hour, event.frame.minute);
                chat_add_message(*time, event.frame.data_str);
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
                        uint8_t target_id = 0x02; // 以后跟着UI选的目标走
                        const char* payload = lv_textarea_get_text(g_ta);
                        uint8_t frame_buf[131];
                        uint8_t frame_len = buildLoRaFrame(frame_buf, self_id, target_id, payload);
                        for (int i = 0; i < frame_len; i++) {
                            if (frame_buf[i] < 0x10) {
                                printf("0");
                            }
                            printf("%02X ", frame_buf[i]);
                        }
                        printf("\n");
                        printf("Frame Length: %d bytes\n", frame_len);
                        uart_transmit(frame_buf, frame_len);

                        // 这个地方以后确认发出去了再清空和更新
                        lv_textarea_set_text(g_ta, ""); 
                        chat_add_message("00:00", payload);
                        break;
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
        lv_task_handler(); // 处理LVGL任务
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
void app_main(void)
{
    appQueue = xQueueCreate(
        10,              // 队列最多放10个元素
        sizeof(uint8_t)  // 每个元素大小
    );
    key_init();
    pwm_init();
    spi_init();
    lcd_init();
    uart_init();
    ui_init();
    
    create_ui();
    ui_show_chat_page();

    xTaskCreate(app_main_task, "app_main", 4096, NULL, 5, NULL);
    xTaskCreate(scanKeyTask, "scan_key", 1024, NULL, 5, NULL);
    xTaskCreate(lvgl_tick_task, "lvgl_tick", 512, NULL, 5, NULL);
    xTaskCreate(uart_receive, "uart_receive", 2048, NULL, 5, NULL);


    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}