#ifndef __MYPWM_H__
#define __MYPWM_H__

#include "driver/ledc.h"

void pwm_init();
void duty_set(uint16_t duty);

#endif