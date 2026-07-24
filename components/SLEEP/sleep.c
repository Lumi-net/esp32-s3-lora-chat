#include "sleep.h"
#include "key.h"          // 用于调用 key_init() 恢复键盘状态
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_pm.h"
#include "driver/rtc_io.h" // 新增：用于 rtc_gpio 系列函数配置 EXT0/EXT1
#include "va.h"           // 假设 last_activity_time_us, auto_sleep_timeout, appQueue 在这里声明
#include "pwm.h"          // 用于 duty_set()

static const char *TAG = "SLEEP";

#define WAKEUP_AUX_GPIO_PIN  GPIO_NUM_21
// 键盘列引脚掩码，用于 EXT1 唤醒
#define KEYPAD_COL_MASK  ((1ULL << GPIO_NUM_16) | (1ULL << GPIO_NUM_17) | \
                          (1ULL << GPIO_NUM_18) | (1ULL << GPIO_NUM_8))

/**
 * @brief 检查是否达到睡眠时间
 */
bool check_and_enter_sleep(void)
{
    int64_t current_time_us = esp_timer_get_time();
    int64_t elapsed_time_us = current_time_us - last_activity_time_us;
    int64_t timeout_us = (int64_t)auto_sleep_timeout * 1000000LL;
    if (elapsed_time_us >= timeout_us) {
        ESP_LOGI(TAG, "Timeout reached (%.2f seconds), preparing for light sleep",
                 (float)elapsed_time_us / 1000000.0);
        return true;
    }
    return false;
}

/**
 * @brief 配置睡眠唤醒源 (使用 EXT0 和 EXT1)
 */
static void configure_wakeup_sources(void)
{
    // ==========================================
    // 0. 确保键盘行引脚输出高电平 (开漏释放，由上拉维持)
    // 这样按键按下时，列引脚才能被拉低
    // ==========================================
    const int rowPins[4] = {GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_4};
    for (int i = 0; i < 4; i++) {
        gpio_set_level(rowPins[i], 1);
    }

    // ==========================================
    // 1. 配置 AUX (GPIO9) 为 EXT0 唤醒 (低电平唤醒)
    // ==========================================
    // 必须先将引脚初始化为 RTC IO
    rtc_gpio_init(WAKEUP_AUX_GPIO_PIN);
    rtc_gpio_set_direction(WAKEUP_AUX_GPIO_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(WAKEUP_AUX_GPIO_PIN);   // 启用内部上拉
    rtc_gpio_pulldown_dis(WAKEUP_AUX_GPIO_PIN);
    
    // 配置 EXT0 唤醒，0 表示低电平触发
    esp_sleep_enable_ext0_wakeup(WAKEUP_AUX_GPIO_PIN, 0); 

    // ==========================================
    // 2. 配置键盘列引脚为 EXT1 唤醒 (任意低电平唤醒)
    // ==========================================
    const int colPins[4] = {GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_8};
    for (int i = 0; i < 4; i++) {
        rtc_gpio_init(colPins[i]);
        rtc_gpio_set_direction(colPins[i], RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pullup_en(colPins[i]);        // 启用内部上拉
        rtc_gpio_pulldown_dis(colPins[i]);
    }
    
    // 配置 EXT1 唤醒，ESP_EXT1_WAKEUP_ANY_LOW 表示任意一个掩码中的引脚拉低即唤醒
    // 注：v5.5 推荐使用 esp_sleep_enable_ext1_wakeup_io，旧版可用 esp_sleep_enable_ext1_wakeup
    esp_sleep_enable_ext1_wakeup_io(KEYPAD_COL_MASK, ESP_EXT1_WAKEUP_ANY_LOW);
}

static bool check_wakeup_pins_idle(void) {
    // 检查 AUX (低电平触发，所以高电平才是空闲)
    if (gpio_get_level(WAKEUP_AUX_GPIO_PIN) == 0) {
        ESP_LOGW(TAG, "Reject: AUX pin is LOW!");
        return false;
    }
    // 检查键盘列 (任意低电平触发)
    if (gpio_get_level(GPIO_NUM_16) == 0 || gpio_get_level(GPIO_NUM_17) == 0 ||
        gpio_get_level(GPIO_NUM_18) == 0 || gpio_get_level(GPIO_NUM_8) == 0) {
        ESP_LOGW(TAG, "Reject: Keypad col pin is LOW!");
        return false;
    }
    return true;
}

/**
 * @brief 进入 Light-sleep 并处理唤醒后的逻辑
 * @return 1 如果是键盘唤醒, 2 如果是 AUX 唤醒, 0 如果是其他/未知唤醒
 */
int enter_light_sleep(void)
{
    int wakeup_source = 0;
    ESP_LOGI("POWER", "No activity for %lld ms. Entering Light Sleep...", (long long)(auto_sleep_timeout / 1000));

    // 1. 关闭屏幕背光以最大化省电
    duty_set(0);

    // 2. 配置唤醒源
    configure_wakeup_sources();

    if (!check_wakeup_pins_idle()) {
        // 恢复背光并返回，不进入睡眠
        duty_set(scr_brightness); 
        return 0; 
    }

    // 3. 进入 Light-sleep (此函数会阻塞，直到被唤醒)
    esp_err_t ret = esp_light_sleep_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enter light sleep: %s", esp_err_to_name(ret));
        
        // 【终极调试代码开始】
        ESP_LOGE(TAG, "========== DUMPING SLEEP REJECT REASON ==========");
        
        // 1. 打印是谁持有了电源锁 (如果是 wifi:1 或 uart:1，就是它们阻止了睡眠)
        ESP_LOGE(TAG, "--- Power Locks ---");
        esp_pm_dump_locks(stdout); 
        
        // 2. 打印唤醒引脚的实时电平
        ESP_LOGE(TAG, "--- Pin Levels (0=LOW, 1=HIGH) ---");
        ESP_LOGE(TAG, "AUX(GPIO9): %d", gpio_get_level(WAKEUP_AUX_GPIO_PIN));
        ESP_LOGE(TAG, "Col(16): %d, Col(17): %d, Col(18): %d, Col(8): %d", 
                gpio_get_level(GPIO_NUM_16), gpio_get_level(GPIO_NUM_17), 
                gpio_get_level(GPIO_NUM_18), gpio_get_level(GPIO_NUM_8));
                
        ESP_LOGE(TAG, "===================================================");
        // 【终极调试代码结束】
        
        return 0;
    }

    // ================= 唤醒后执行从这里开始 =================
    ESP_LOGI("POWER", "Woke up from Light Sleep!");

    // 4. 【关键】恢复 RTC IO 为普通数字 GPIO
    // 文档指出：EXT0/EXT1 唤醒后，引脚会被硬件自动配置为 RTC IO。
    // 在将其用作普通数字 GPIO (如调用 key_init) 前，必须调用 rtc_gpio_deinit。
    rtc_gpio_deinit(WAKEUP_AUX_GPIO_PIN);
    const int colPins[4] = {GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_8};
    for (int i = 0; i < 4; i++) {
        rtc_gpio_deinit(colPins[i]);
    }

    // 5. 恢复键盘 GPIO 到正常工作状态
    key_init();

    // 6. 判断唤醒源
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI("POWER", ">>> Wakeup Source: AUX (LoRa) via EXT0");
        wakeup_source = 2;
    } 
    else if (cause == ESP_SLEEP_WAKEUP_EXT1) {
        ESP_LOGI("POWER", ">>> Wakeup Source: Matrix Keypad via EXT1");
        wakeup_source = 1;
    } 
    else {
        ESP_LOGI("POWER", ">>> Wakeup Source: Other (Cause: %d)", cause);
    }

    // 7. 重置活动计时器，防止唤醒后立即再次判定超时
    last_activity_time_us = esp_timer_get_time();
    
    if (wakeup_source == 1) {
        duty_set(scr_brightness);
    }

    return wakeup_source;
}