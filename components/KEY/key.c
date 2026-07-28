#include "key.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "va.h"

bool keyPressed = false;
uint8_t lastKey = 0;
int64_t lastDebounceTime = 0;
uint8_t keyMap[4][4] = {
  {1, 2, 3, 4},
  {5, 6, 7, 8},
  {9, 10, 11, 12},
  {13, 14, 15, 16}
};
static const char candidate1[2][10] = {
    {'0','1','2','3','4','5','6','7','8','9'},  // 未shift
    {'.','!','?','\'','-','*','/','+','&',' '}  // shifted
};
static const char candidate2[2][10] = {
    {' ','a','d','g','j','m','p','s','v','y'},  // 未shift
    {',','A','D','G','J','M','P','S','V','Y'}  // shifted
};
static const char candidate3[2][10] = {
    {'^','b','e','h','k','n','q','t','w','z'},  // 未shift
    {':','B','E','H','K','N','Q','T','W','Z'}  // shifted
};
static const char candidate4[2][10] = {
    {'=','c','f','i','l','o','r','u','x','('},  // 未shift
    {'_','C','F','I','L','O','R','U','X',')'}  // shifted
};
static const uint8_t num_to_char[17] = {
    10,             // 0 (占位)
    1, 2, 3,    // 1, 2, 3
    10,             // 4 (候选1)
    4, 5, 6,    // 5, 6, 7
    10,             // 8 (候选2)
    7, 8, 9,    // 9, 10, 11
    10,             // 12 (候选3)
    10,             // 13 (Shift)
    0,              // 14
    10, 10  // 15, 16
};
const int rowPins[4] = {GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_4};
const int colPins[4] = {GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_8};
bool shifted = false;
bool locked = false;
bool waited_to_choose = false;
uint8_t wait_choose;
uint8_t lockfun;


void key_init(void)
{
    gpio_config_t gpio_row_structure = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT_OD,
        .pin_bit_mask = (1ull<<GPIO_NUM_5)|(1ull<<GPIO_NUM_6)|(1ull<<GPIO_NUM_7)|(1ull<<GPIO_NUM_4),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&gpio_row_structure);
    gpio_config_t gpio_col_structure = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ull<<GPIO_NUM_16)|(1ull<<GPIO_NUM_17)|(1ull<<GPIO_NUM_18)|(1ull<<GPIO_NUM_8),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&gpio_col_structure);
}

uint8_t scanKey() {
    uint8_t choose = 0;
    uint8_t currentKey = 0;
    for (int row = 0; row < 4; row++) {
        // 所有行拉高
        for (int i = 0; i < 4; i++) {
            gpio_set_level(rowPins[i], 1);
        }
        // 当前行拉低
        gpio_set_level(rowPins[row], 0);
        // 等待电平稳定
        esp_rom_delay_us(10);
        // 检测列
        for (int col = 0; col < 4; col++) {
            if (gpio_get_level(colPins[col]) == 0) {
                currentKey = keyMap[row][col];
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        gpio_set_level(rowPins[i], 1);
    }

    if (currentKey != lastKey) {
        lastDebounceTime = esp_timer_get_time() / 1000;
        lastKey = currentKey;
    }
    if ((esp_timer_get_time() / 1000 - lastDebounceTime) > 20) {
        if (currentKey != 0 && !keyPressed) {
            keyPressed = true;
            last_activity_time_us = esp_timer_get_time();
            switch (currentKey) {
                case 1: case 2: case 3: case 5: case 6: case 7: case 9: case 10: case 11: case 14: // 数字键
                    if (waited_to_choose) {
                        waited_to_choose = false;
                        choose = 20; // state change
                    } else if (locked) {
                        choose = 5;
                        lockfun = currentKey;
                    } else {
                        waited_to_choose = true;
                        wait_choose = num_to_char[currentKey];
                        choose = 20; // state change
                    }
                    break;
                case 4: case 8: case 12: case 16: // 候选
                    if (waited_to_choose) {
                        const char (*current_table)[10];
                        if (currentKey == 4) current_table = candidate1;
                        else if (currentKey == 8) current_table = candidate2;
                        else if (currentKey == 12) current_table = candidate3;
                        else current_table = candidate4;

                        choose = current_table[shifted][wait_choose];
                        
                        if (shifted) shifted = false;
                        waited_to_choose = false;
                    } else if (locked) {
                        choose = 5;
                        lockfun = currentKey;
                    } else {
                        waited_to_choose = false;
                        choose = 20; // state change
                    }
                    break;
                case 13: // Shift
                    if (waited_to_choose) {
                        waited_to_choose = false; 
                    } else {
                        shifted = !shifted;
                    }
                    choose = 20; // state change
                    break;

                case 15: // BS&LOCK
                    if (waited_to_choose) {
                        waited_to_choose = false;
                        choose = 20; // state change
                    } else {
                        if (shifted) {
                            locked = !locked;
                            choose = 19; // LOCK_TOGGLE sentinel
                        } else {
                            choose = 8; // BS
                        }
                    }
                    break;
            }
        }
        if (currentKey == 0) {
            keyPressed = false;
        }
    }
    return choose;
}

void key_set_locked(bool state) {
    locked = state;
}

void scanKeyTask(void *arg) {
    while (1) {
        uint8_t key = scanKey();
        if (key != 0) {
            printf("Key Pressed: %d\n", key);
            UIEvent event = {
                .type = EVENT_KEY,
                .key = key
            };
            xQueueSend(appQueue, &event, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}