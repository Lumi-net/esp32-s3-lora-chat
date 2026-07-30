#ifndef KEY_H
#define KEY_H

#include "driver/gpio.h"
#include "esp_timer.h"

extern uint8_t lockfun;
extern bool shifted;
extern bool locked;
extern bool waited_to_choose;
extern uint8_t wait_choose;

void key_init(void);
uint8_t scanKey();
void scanKeyTask(void *arg);
void key_set_locked(bool state);
void key_set_shifted(bool state);

#endif