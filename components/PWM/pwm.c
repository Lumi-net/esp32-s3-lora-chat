#include "pwm.h"
#include "driver/gpio.h"

void pwm_init(void)
{
    ledc_timer_config_t ledctimer_structure = {
        .clk_cfg = LEDC_AUTO_CLK,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 1000,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
    };
    ledc_timer_config(&ledctimer_structure);

    ledc_channel_config_t ledcchannel_structure = {
        .channel = LEDC_CHANNEL_1,
        .duty = 512,
        .flags.output_invert = 0,
        .gpio_num = GPIO_NUM_9,
        .hpoint = 0,
        .intr_type = LEDC_INTR_DISABLE,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = LEDC_TIMER_1,
    };
    ledc_channel_config(&ledcchannel_structure);
}

void duty_set(uint16_t duty)
{
    if (duty > 1023) {
        duty = 1023;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty); 
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);        
}