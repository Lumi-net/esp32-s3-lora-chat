#ifndef __MYUART_H__
#define __MYUART_H__

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_random.h"
#include <string.h>
#include "queue.h"
#include "event.h"
#include "types.h"

extern uint8_t self_id;
extern uint8_t receive_data[1024];
void uart_init(void);
void uart_receive(void *arg);
void uart_transmit(const uint8_t *data, size_t size);
uint8_t buildLoRaFrame(uint8_t* buf, uint8_t self_id, uint8_t target_id, const char* data_str);
ParseStatus parseLoRaFrame(const uint8_t* buf, uint8_t buf_len, LoRaFrameData* out);

#endif