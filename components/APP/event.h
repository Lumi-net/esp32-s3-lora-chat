#ifndef __EVENT_H__
#define __EVENT_H__

#include "types.h"

typedef enum
{
    EVENT_KEY,
    EVENT_UART
} EventType;

typedef struct
{
    EventType type;

    union
    {
        uint8_t key;
        LoRaFrameData frame;
    };
} UIEvent;

#endif