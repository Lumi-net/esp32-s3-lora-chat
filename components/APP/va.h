#ifndef __VA_H__
#define __VA_H__

#include "freertos/FreeRTOS.h"
#include "types.h"

extern QueueHandle_t appQueue;
extern chat_item_t chat_list[256];
extern uint16_t auto_sleep_timeout;
extern uint16_t scr_brightness;
extern int64_t last_activity_time_us;
extern color_item_t color_index[COLOR_MAX];
extern page_id_t current_page_id;
extern LoRaStatus lora_status;
extern uint8_t ack_timeout_count;
extern char status_title[17];

#endif