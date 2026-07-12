#ifndef __VA_H__
#define __VA_H__

#include <stdint.h>
#include "freertos/FreeRTOS.h"

typedef struct
{
    uint8_t id;
} chat_item_t;

extern QueueHandle_t appQueue;
extern chat_item_t chat_list[255];
extern uint8_t chat_cnt;

#endif