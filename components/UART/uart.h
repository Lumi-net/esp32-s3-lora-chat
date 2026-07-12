#ifndef __MYUART_H__
#define __MYUART_H__

#include <stdint.h>
#include <stddef.h>
#include "types.h"

extern uint8_t self_id;
extern uint8_t receive_data[1024];
uint8_t calculateCRC8(const uint8_t* data, uint8_t len);
void uart_init(void);
void uart_receive(void *arg);
void uart_transmit(const uint8_t *data, size_t size);
uint8_t buildLoRaFrame(uint8_t* buf, uint8_t self_id, uint8_t target_id, const char* data_str);
ParseStatus parseLoRaFrame(const uint8_t* buf, uint8_t buf_len, LoRaFrameData* out);

#endif