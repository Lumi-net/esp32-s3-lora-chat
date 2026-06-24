#ifndef __MYLCD_H__
#define __MYLCD_H__ 

#include "pwm.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

extern esp_lcd_panel_io_handle_t io_handle;
extern esp_lcd_panel_handle_t panel_handle;

void lcd_init();
void lcd_bl(uint16_t duty);

#endif