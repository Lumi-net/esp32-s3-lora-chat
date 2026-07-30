#pragma once

#include <stdbool.h>
#include <stdint.h>

bool check_and_enter_sleep(void);

int enter_light_sleep(uint64_t timer_wakeup_us);