#include "i2c.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "math.h"

#define INA219_PWR_PIN      GPIO_NUM_10  // 给 INA219 供电的 GPIO
#define I2C_MASTER_SDA_IO   GPIO_NUM_8
#define I2C_MASTER_SCL_IO   GPIO_NUM_9
#define I2C_MASTER_NUM      I2C_NUM_0

// INA219 I2C地址 (A0和A1都接地时为0x40)
#define INA219_I2C_ADDRESS (0x40)
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000

// INA219寄存器地址
#define INA219_REG_CONFIG         0x00
#define INA219_REG_SHUNT_VOLTAGE  0x01
#define INA219_REG_BUS_VOLTAGE    0x02
#define INA219_REG_POWER          0x03
#define INA219_REG_CURRENT        0x04
#define INA219_REG_CALIBRATION    0x05

// 配置值
// 配置字：BRNG=1(32V范围), PG=0(/1, 40mV), BADC=12bit, SADC=12bit, Mode=连续转换
#define INA219_CONFIG_VALUE       0x399F

// 分流电阻值 (欧姆) - 根据实际电路修改，通常为0.1欧姆
#define SHUNT_RESISTOR_OHMS       0.1f
// 最大预期电流 (安培)
#define MAX_EXPECTED_CURRENT      2.0f

static const char *TAG = "INA219";

// 标记 INA219 是否处于上电状态
static bool is_ina219_powered = false;
static bool is_ina219_initialized = false;

// 校准参数
static float current_lsb = 0.0f;  // 电流LSB (A/bit)
static float power_lsb = 0.0f;    // 功率LSB (W/bit)

typedef struct {
    float voltage;
    uint8_t percent;
} battery_table_t;

const battery_table_t battery_table[] = {
    {4.20,100},
    {4.15,95},
    {4.11,90},
    {4.08,80},
    {4.02,70},
    {3.98,60},
    {3.95,50},
    {3.91,40},
    {3.87,30},
    {3.83,20},
    {3.79,10},
    {3.70,5},
    {3.30,0},
};

/**
 * @brief 初始化I2C主机
 */
esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,   // 根据实际引脚修改
        .scl_io_num = I2C_MASTER_SCL_IO,   // 根据实际引脚修改
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }
    
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err == ESP_OK) {
        is_ina219_initialized = true;
    }

    // 默认断电
    gpio_reset_pin(INA219_PWR_PIN);
    gpio_set_direction(INA219_PWR_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(INA219_PWR_PIN, 0);
    
    return err;
}

/**
 * @brief 向INA219写入16位寄存器
 */
esp_err_t ina219_write_register(uint8_t reg_addr, uint16_t value)
{
    esp_err_t err;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA219_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, (value >> 8) & 0xFF, true);  // MSB
    i2c_master_write_byte(cmd, value & 0xFF, true);          // LSB
    i2c_master_stop(cmd);
    
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return err;
}

/**
 * @brief 从INA219读取16位寄存器
 */
esp_err_t ina219_read_register(uint8_t reg_addr, uint16_t *value)
{
    esp_err_t err;
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA219_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA219_I2C_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, (uint8_t *)value, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, ((uint8_t *)value) + 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    err = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    if (err == ESP_OK) {
        // 转换为大端序
        *value = ((*value >> 8) & 0xFF) | ((*value << 8) & 0xFF00);
    }
    
    return err;
}

/**
 * @brief 计算校准寄存器值
 */
void calculate_calibration(void)
{
    // 计算电流LSB：最大预期电流 / 2^15
    current_lsb = MAX_EXPECTED_CURRENT / 32768.0f;
    
    // 向上取整到一个更友好的值
    current_lsb = ceilf(current_lsb * 100000.0f) / 100000.0f;
    
    // 计算校准寄存器值
    // Cal = trunc(0.04096 / (Current_LSB * RSHUNT))
    float cal_value = 0.04096f / (current_lsb * SHUNT_RESISTOR_OHMS);
    uint16_t calibration_reg = (uint16_t)cal_value;
    
    // 功率LSB = 20 * 电流LSB
    power_lsb = 20.0f * current_lsb;
    
    ESP_LOGI(TAG, "Current LSB: %.6f A/bit", current_lsb);
    ESP_LOGI(TAG, "Power LSB: %.6f W/bit", power_lsb);
    ESP_LOGI(TAG, "Calibration Register: 0x%04X (%d)", calibration_reg, calibration_reg);
    
    // 写入校准寄存器
    ina219_write_register(INA219_REG_CALIBRATION, calibration_reg);
}

/**
 * @brief 初始化INA219
 */
esp_err_t ina219_init(void)
{
    esp_err_t err;
    
    // 写入配置寄存器
    err = ina219_write_register(INA219_REG_CONFIG, INA219_CONFIG_VALUE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure INA219: %s", esp_err_to_name(err));
        return err;
    }
    
    vTaskDelay(pdMS_TO_TICKS(10));  // 等待配置完成
    
    // 计算并写入校准值
    calculate_calibration();
    
    ESP_LOGI(TAG, "INA219 initialized successfully");
    return ESP_OK;
}

/**
 * @brief 读取总线电压 (V)
 */
float ina219_get_bus_voltage(void)
{
    uint16_t value;
    
    if (ina219_read_register(INA219_REG_BUS_VOLTAGE, &value) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read bus voltage");
        return -1.0f;
    }
    
    // 总线电压寄存器位[15:3]包含数据，需要右移3位
    // LSB = 4mV
    uint16_t bus_voltage_raw = (value >> 3);
    float bus_voltage = bus_voltage_raw * 0.004f;  // 转换为伏特
    
    return bus_voltage;
}

// /**
//  * @brief 读取分流电压 (V)
//  */
// float ina219_get_shunt_voltage(void)
// {
//     uint16_t value;
    
//     if (ina219_read_register(INA219_REG_SHUNT_VOLTAGE, &value) != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to read shunt voltage");
//         return -1.0f;
//     }
    
//     // 分流电压LSB = 10μV
//     // 处理有符号数
//     int16_t shunt_voltage_raw = (int16_t)value;
//     float shunt_voltage = shunt_voltage_raw * 0.00001f;  // 转换为伏特
    
//     return shunt_voltage;
// }

/**
 * @brief 读取电流 (A)
 */
float ina219_get_current(void)
{
    uint16_t value;
    
    if (ina219_read_register(INA219_REG_CURRENT, &value) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read current");
        return -1.0f;
    }
    
    // 处理有符号数
    int16_t current_raw = (int16_t)value;
    float current = current_raw * current_lsb;
    
    return current;
}

// /**
//  * @brief 读取功率 (W)
//  */
// float ina219_get_power(void)
// {
//     uint16_t value;
    
//     if (ina219_read_register(INA219_REG_POWER, &value) != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to read power");
//         return -1.0f;
//     }
    
//     // 功率寄存器值 * 功率LSB
//     float power = value * power_lsb;
    
//     return power;
// }

// /**
//  * @brief 计算电量 (mAh)
//  * @param current_mA 当前电流 (mA)
//  * @param interval_ms 采样间隔 (ms)
//  * @param total_mah 累计电量指针 (mAh)
//  */
// void calculate_capacity(float current_mA, uint32_t interval_ms, float *total_mah)
// {
//     // 电量(mAh) = 电流(mA) × 时间(h)
//     // 时间(h) = 时间(ms) / 3600000
//     float capacity_increment = current_mA * (interval_ms / 3600000.0f);
//     *total_mah += capacity_increment;
// }

// /**
//  * @brief INA219读取任务
//  */
// void ina219_task(void *arg)
// {
//     float total_capacity_mah = 0.0f;  // 累计电量 (mAh)
//     uint32_t last_read_time = 0;
    
//     while (1) {
//         uint32_t current_time = esp_timer_get_time() / 1000;  // 毫秒
        
//         float bus_voltage = ina219_get_bus_voltage();
//         float shunt_voltage = ina219_get_shunt_voltage();
//         float current = ina219_get_current();
//         float power = ina219_get_power();
        
//         if (bus_voltage >= 0 && current >= 0) {
//             // 计算电量
//             if (last_read_time > 0) {
//                 uint32_t interval = current_time - last_read_time;
//                 calculate_capacity(current * 1000.0f, interval, &total_capacity_mah);
//             }
//             last_read_time = current_time;
            
//             ESP_LOGI(TAG, "Voltage: %.3f V | Current: %.3f A | Power: %.3f W | Capacity: %.2f mAh",
//                      bus_voltage, current, power, total_capacity_mah);
//         } else {
//             ESP_LOGE(TAG, "Failed to read sensors");
//         }
        
//         vTaskDelay(pdMS_TO_TICKS(1000));  // 每秒读取一次
//     }
// }

// /**
//  * @brief 应用程序主函数
//  */
// void app_main(void)
// {
//     ESP_LOGI(TAG, "Starting INA219 example");
    
//     // 初始化I2C
//     esp_err_t err = i2c_master_init();
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to initialize I2C: %s", esp_err_to_name(err));
//         return;
//     }
    
//     // 初始化INA219
//     err = ina219_init();
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to initialize INA219");
//         return;
//     }
    
//     // 创建读取任务
//     xTaskCreate(ina219_task, "ina219_task", 4096, NULL, 5, NULL);
// }

/**
 * @brief 开启 INA219 电源并初始化
 */
void ina219_power_on_and_init(void)
{
    if (is_ina219_powered) return;

    // 1. 拉高 GPIO 给 INA219 供电 (3.3V)
    gpio_set_direction(INA219_PWR_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(INA219_PWR_PIN, 1);
    
    // 2. 等待 INA219 内部电路稳定 (数据手册未明确，通常给 10~50ms 足够)
    vTaskDelay(pdMS_TO_TICKS(50)); 
    
    // // 3. 初始化 I2C 主机
    // i2c_config_t conf = {
    //     .mode = I2C_MODE_MASTER,
    //     .sda_io_num = I2C_MASTER_SDA_IO,
    //     .scl_io_num = I2C_MASTER_SCL_IO,
    //     .sda_pullup_en = GPIO_PULLUP_ENABLE,
    //     .scl_pullup_en = GPIO_PULLUP_ENABLE,
    //     .master.clk_speed = 400000,
    // };
    // i2c_param_config(I2C_MASTER_NUM, &conf);
    // i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    // 4. 初始化 INA219 (写入配置寄存器和校准寄存器)
    // 注意：断电后 INA219 内部寄存器会丢失，每次上电必须重新配置！
    ina219_init(); 
    
    is_ina219_powered = true;
    ESP_LOGI(TAG, "INA219 Powered ON and Initialized.");
}

/**
 * @brief 关闭 INA219 电源 (防倒灌处理)
 */
void ina219_power_off(void)
{
    if (!is_ina219_powered) return;

    // 1. 卸载 I2C 驱动
    // i2c_driver_delete(I2C_MASTER_NUM);

    // 2. 【核心防倒灌】将 SDA 和 SCL 设置为输入模式(高阻态)或输出低电平
    // 这里我们选择设置为输入模式，并禁用内部上拉/下拉
    gpio_set_direction(I2C_MASTER_SDA_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(I2C_MASTER_SCL_IO, GPIO_MODE_OUTPUT);
    gpio_set_level(I2C_MASTER_SDA_IO, 0);
    gpio_set_level(I2C_MASTER_SCL_IO, 0);

    // 3. 拉低 GPIO，彻底切断 INA219 供电
    gpio_set_level(INA219_PWR_PIN, 0);

    // 4. 恢复 I2C 引脚为输入/高阻态，以便下次 I2C 通信前由驱动接管
    // 或者保持输出低，直到下次 power_on 时由 i2c driver 重新配置
    // 这里建议恢复为输入，避免影响总线上可能的其他设备
    gpio_set_direction(I2C_MASTER_SDA_IO, GPIO_MODE_INPUT);
    gpio_set_direction(I2C_MASTER_SCL_IO, GPIO_MODE_INPUT);
    
    is_ina219_powered = false;
    ESP_LOGI(TAG, "INA219 Powered OFF.");
}

/**
 * @brief 获取一次电池数据，然后立刻断电
 * @param voltage_v 输出总线电压 (V)
 * @param current_a 输出电流 (A)
 */
void ina219_get_data_once(float *voltage_v, float *current_a)
{
    // 1. 上电并初始化
    ina219_power_on_and_init();

    // 2. 等待第一次 ADC 转换完成 (12-bit 转换约需 500us，给 5ms 确保稳定)
    vTaskDelay(pdMS_TO_TICKS(10));

    // 3. 读取数据 (调用您之前写的读取函数)
    *voltage_v = ina219_get_bus_voltage();
    *current_a = ina219_get_current();

    // 4. 立刻断电省电
    ina219_power_off();
}

uint8_t battery_voltage_to_percent(float voltage)
{
    const size_t count = sizeof(battery_table) / sizeof(battery_table[0]);

    // 高于最高电压
    if (voltage >= battery_table[0].voltage)
        return 100;

    // 低于最低电压
    if (voltage <= battery_table[count - 1].voltage)
        return 0;

    // 找到所在区间
    for (size_t i = 0; i < count - 1; i++)
    {
        float v_high = battery_table[i].voltage;
        float v_low  = battery_table[i + 1].voltage;

        if (voltage <= v_high && voltage >= v_low)
        {
            uint8_t p_high = battery_table[i].percent;
            uint8_t p_low  = battery_table[i + 1].percent;

            float ratio = (voltage - v_low) / (v_high - v_low);

            return (uint8_t)(p_low + ratio * (p_high - p_low));
        }
    }

    return 0;
}