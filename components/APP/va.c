#include "va.h"


QueueHandle_t appQueue = NULL;

chat_item_t chat_list[255] =
{
    [0] = {.id = 0x00},
    [1] = {.id = 0x01},
}; // TODO: 从Flash获取聊天对象列表