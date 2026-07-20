#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t i2c_master_init(void);
void ina219_get_data_once(float *voltage_v, float *current_a);
uint8_t battery_voltage_to_percent(float voltage);