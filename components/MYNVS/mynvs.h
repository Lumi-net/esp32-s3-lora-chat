#pragma once
#include "esp_err.h"

esp_err_t nvs_init(void);
esp_err_t nvs_set_alias(uint8_t id, const char *alias);
esp_err_t nvs_set_user_color(uint8_t id, uint32_t color);
esp_err_t nvs_set_interface_color(color_nvs_t id, uint32_t color);
void nvs_read_all_alias_to_list(void);
void nvs_read_all_user_color_to_list(void);
void nvs_read_all_interface_color_to_list(void);