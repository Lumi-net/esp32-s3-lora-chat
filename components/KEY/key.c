#include "key.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "queue.h"

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
    {'1','2','3','4','5','6','7','8','9','0'},  // 未shift
    {'!','?','\'','-','*','/','+','&',' ','.'}  // shifted
};
static const char candidate2[2][10] = {
    {'a','d','g','j','m','p','s','v','y',' '},  // 未shift
    {'A','D','G','J','M','P','S','V','Y',','}  // shifted
};
static const char candidate3[2][10] = {
    {'b','e','h','k','n','q','t','w','z','^'},  // 未shift
    {'B','E','H','K','N','Q','T','W','Z',':'}  // shifted
};
static const char candidate4[2][10] = {
    {'c','f','i','l','o','r','u','x','(','='},  // 未shift
    {'C','F','I','L','O','R','U','X',')','_'}  // shifted
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
            switch (currentKey) {
                case 1: case 2: case 3: case 5: case 6: case 7: case 9: case 10: case 11: case 14: // 数字键
                    if (waited_to_choose) {
                        waited_to_choose = false; // 您的设计：取消等待
                    } else if (locked) {
                        choose = 5;               // 外部定义的锁定功能码
                        lockfun = currentKey;
                    } else {
                        waited_to_choose = true;
                        wait_choose = num_to_char[currentKey]; // 查表获取数字字符
                    }
                    break;
                case 4: case 8: case 12: case 16: // 候选键
                    if (waited_to_choose) {
                        // 确定候选表
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
                    }
                    break;
                case 13: // Shift
                    if (waited_to_choose) {
                        waited_to_choose = false; 
                    } else {
                        shifted = !shifted;
                    }
                    break;

                case 15: // BS&LOCK
                    if (waited_to_choose) {
                        waited_to_choose = false;
                    } else {
                        if (shifted) {
                            locked = !locked;
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

void scanKeyTask(void *arg) {
    while (1) {
        uint8_t key = scanKey();
        if (key != 0) {
            printf("Key Pressed: %d\n", key);
            xQueueSend(appQueue, &key, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}



//     switch (currentKey) {
        //         case 1: // 1
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 1;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '1';
        //             }
        //             break;
        //         case 2: // 2
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 2;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '2';
        //             }
        //             break;
        //         case 3: // 3
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 3;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '3';
        //             }
        //             break;
        //         case 4: // 候选1
        //             if (waited_to_choose) {
        //                 if (shifted) {
        //                     switch (wait_choose) {
        //                         case '1':
        //                             choose = '!';
        //                             break;
        //                         case '2':
        //                             choose = '?';
        //                             break;
        //                         case '3':
        //                             choose = '\'';
        //                             break;
        //                         case '4':
        //                             choose = '-';
        //                             break;
        //                         case '5':
        //                             choose = '*';
        //                             break;
        //                         case '6':
        //                             choose = '/';
        //                             break;
        //                         case '7':
        //                             choose = '+';
        //                             break;
        //                         case '8':
        //                             choose = '&';
        //                             break;
        //                         case '9':
        //                             choose = ' ';
        //                             break;
        //                         case '0':
        //                             choose = '.';
        //                             break;
        //                     }
        //                     shifted = false;
        //                 }
        //                 else {
        //                     switch (wait_choose) {
        //                         case '1':
        //                             choose = '1';
        //                             break;
        //                         case '2':
        //                             choose = '2';
        //                             break;
        //                         case '3':
        //                             choose = '3';
        //                             break;
        //                         case '4':
        //                             choose = '4';
        //                             break;
        //                         case '5':
        //                             choose = '5';
        //                             break;
        //                         case '6':
        //                             choose = '6';
        //                             break;
        //                         case '7':
        //                             choose = '7';
        //                             break;
        //                         case '8':
        //                             choose = '8';
        //                             break;
        //                         case '9':
        //                             choose = '9';
        //                             break;
        //                         case '0':
        //                             choose = '0';
        //                             break;
        //                     }
        //                 }
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 4;
        //             }
        //             else {
        //                 waited_to_choose = false;
        //             }
        //             break;
        //         case 5: // 4
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 5;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '4';
        //             }
        //             break;
        //         case 6: // 5
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 6;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '5';
        //             }
        //             break;
        //         case 7: // 6
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 7;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '6';
        //             }
        //             break;
        //         case 8: // 候选2
        //             if (waited_to_choose) {
        //                 if (shifted) {
        //                     switch (wait_choose) {
        //                         case '1':
        //                             choose = 'A';
        //                             break;
        //                         case '2':
        //                             choose = 'D';
        //                             break;
        //                         case '3':
        //                             choose = 'G';
        //                             break;
        //                         case '4':
        //                             choose = 'J';
        //                             break;
        //                         case '5':
        //                             choose = 'M';
        //                             break;
        //                         case '6':
        //                             choose = 'P';
        //                             break;
        //                         case '7':
        //                             choose = 'S';
        //                             break;
        //                         case '8':
        //                             choose = 'V';
        //                             break;
        //                         case '9':
        //                             choose = 'Y';
        //                             break;
        //                         case '0':
        //                             choose = ',';
        //                             break;
        //                     }
        //                     shifted = false;
        //                 }
        //                 else {
        //                     switch (wait_choose) {
        //                         case '1':
        //                             choose = 'a';
        //                             break;
        //                         case '2':
        //                             choose = 'd';
        //                             break;
        //                         case '3':
        //                             choose = 'g';
        //                             break;
        //                         case '4':
        //                             choose = 'j';
        //                             break;
        //                         case '5':
        //                             choose = 'm';
        //                             break;
        //                         case '6':
        //                             choose = 'p';
        //                             break;
        //                         case '7':
        //                             choose = 's';
        //                             break;
        //                         case '8':
        //                             choose = 'v';
        //                             break;
        //                         case '9':
        //                             choose = 'y';
        //                             break;
        //                         case '0':
        //                             choose = ' ';
        //                             break;
        //                     }
        //                 }
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 8;
        //             }
        //             else {
        //                 waited_to_choose = false;
        //             }
        //             break;
        //         case 9: // 7
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 9;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '7';
        //             }
        //             break;
        //         case 10: // 8
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 10;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '8';
        //             }
        //             break;
        //         case 11: // 9
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 11;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '9';
        //             }
        //             break;
        //         case 12: // 候选3
        //             if (waited_to_choose) {
        //                 if (shifted) {
        //                     switch (wait_choose) {
        //                         case '1':
        //                             choose = 'B';
        //                             break;
        //                         case '2':
        //                             choose = 'E';
        //                             break;
        //                         case '3':
        //                             choose = 'H';
        //                             break;
        //                         case '4':
        //                             choose = 'K';
        //                             break;
        //                         case '5':
        //                             choose = 'N';
        //                             break;
        //                         case '6':
        //                             choose = 'Q';
        //                             break;
        //                         case '7':
        //                             choose = 'T';
        //                             break;
        //                         case '8':
        //                             choose = 'W';
        //                             break;
        //                         case '9':
        //                             choose = 'Z';
        //                             break;
        //                         case '0':
        //                             choose = ':';
        //                             break;
        //                     }
        //                     shifted = false;
        //                 }
        //                 else {
        //                     switch (wait_choose) {
        //                         case '1':
        //                             choose = 'b';
        //                             break;
        //                         case '2':
        //                             choose = 'e';
        //                             break;
        //                         case '3':
        //                             choose = 'h';
        //                             break;
        //                         case '4':
        //                             choose = 'k';
        //                             break;
        //                         case '5':
        //                             choose = 'n';
        //                             break;
        //                         case '6':
        //                             choose = 'q';
        //                             break;
        //                         case '7':
        //                             choose = 't';
        //                             break;
        //                         case '8':
        //                             choose = 'w';
        //                             break;
        //                         case '9':
        //                             choose = 'z';
        //                             break;
        //                         case '0':
        //                             choose = '^';
        //                             break;
        //                     }
        //                 }
        //                 waited_to_choose = false;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 12;
        //             }
        //             else {
        //                 waited_to_choose = false;
        //             }
        //             break;
        //         case 13: // SHIFT
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else {
        //                 if (shifted) {
        //                     shifted = false;
        //                     return '\0';
        //                 }
        //                 else {
        //                     shifted = true;
        //                     return '\0';
        //                 }
        //             }
        //             break;
        //         case 14: // 0
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else {
        //                 waited_to_choose = true;
        //                 wait_choose = '0';
        //             }
        //             break;
        //         case 15: // BS&LOCK
        //             if (waited_to_choose) {
        //                 waited_to_choose = false;
        //             }
        //             else {
        //                 if (shifted && !locked) {
        //                     locked = true;
        //                     return '\0';
        //                 }
        //                 else if (shifted && locked) {
        //                     locked = false;
        //                     return '\0';
        //                 }
        //                 else {
        //                     choose = 8; // BACKSPACE
        //                 }
        //             }
        //             break;
        //         case 16: // 候选4
        //             if (waited_to_choose) {
        //                 if (shifted) {
        //                     switch (wait_choose) {
        //                         case '1':
        //                             choose = 'C';
        //                             break;
        //                         case '2':
        //                             choose = 'F';
        //                             break;
        //                         case '3':
        //                             choose = 'I';
        //                             break;
        //                         case '4':
        //                             choose = 'L';
        //                             break;
        //                         case '5':
        //                             choose = 'O';
        //                             break;
        //                         case '6':
        //                             choose = 'R';
        //                             break;
        //                         case '7':
        //                             choose = 'U';
        //                             break;
        //                         case '8':
        //                             choose = 'X';
        //                             break;
        //                         case '9':
        //                             choose = ')';
        //                             break;
        //                         case '0':
        //                             choose = '_';
        //                             break;
        //                     }
        //                     shifted = false;
        //                 }
        //                 else {
        //                     switch (wait_choose) {
        //                         case '1':
        //                             choose = 'c';
        //                             break;
        //                         case '2':
        //                             choose = 'f';
        //                             break;
        //                         case '3':
        //                             choose = 'i';
        //                             break;
        //                         case '4':
        //                             choose = 'l';
        //                             break;
        //                         case '5':
        //                             choose = 'o';
        //                             break;
        //                         case '6':
        //                             choose = 'r';
        //                             break;
        //                         case '7':
        //                             choose = 'u';
        //                             break;
        //                         case '8':
        //                             choose = 'x';
        //                             break;
        //                         case '9':
        //                             choose = '(';
        //                             break;
        //                         case '0':
        //                             choose = '=';
        //                             break;
        //                     }
        //                 }
        //                 waited_to_choose = false;
        //             }
        //             else if (shifted) {
        //                 choose = 17;
        //             }
        //             else if (locked) {
        //                 choose = 5;
        //                 lockfun = 16;
        //             }
        //             else {
        //                 waited_to_choose = false;
        //             }
        //             break;
        //     }
        //     if (!waited_to_choose) {
        //         return choose;
        //     }
        // }
        // if (currentKey == '\0') {
        //     keyPressed = false;
        // }