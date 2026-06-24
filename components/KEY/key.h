#ifndef KEY_H
#define KEY_H

#include "driver/gpio.h"
#include "esp_timer.h"

extern uint8_t lockfun;


void key_init(void);
uint8_t scanKey();
void scanKeyTask(void *arg);

#endif