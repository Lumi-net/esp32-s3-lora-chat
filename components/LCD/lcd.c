#include "lcd.h"

esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;

void lcd_init(void)
    {
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = GPIO_NUM_13,
        .cs_gpio_num = GPIO_NUM_10,
        .pclk_hz = 60000000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    // 将 LCD 连接到 SPI 总线
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io_handle));


    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_NUM_14,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,
    };
    // 为 ST7789 创建 LCD 面板句柄，并指定 SPI IO 设备句柄
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    //  复位、初始化、颜色反转、开启显示
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    duty_set(512); // 设置背光亮度为 50%
}
void lcd_bl(uint16_t duty) {
    duty_set(duty);
}