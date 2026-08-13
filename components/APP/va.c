#include "va.h"

QueueHandle_t appQueue = NULL;

uint16_t auto_sleep_timeout = 300;

uint16_t scr_brightness = 500;

int64_t last_activity_time_us = 0;

chat_item_t chat_list[256] = {0};

color_item_t color_index[COLOR_MAX] = {0};

page_id_t current_page_id = PAGE_NONE;

LoRaStatus lora_status = LORA_STATUS_IDLE;
uint8_t ack_timeout_count = 0;

char status_title[17] = "Lumi-net";