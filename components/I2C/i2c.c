#include "i2c.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"  // 【修改】替换为新的 I2C 主机头文件
#include "esp_log.h"
#include "math.h"

#define INA219_PWR_PIN      GPIO_NUM_35  // 给 INA219 供电的 GPIO
#define I2C_MASTER_SDA_IO   GPIO_NUM_36
#define I2C_MASTER_SCL_IO   GPIO_NUM_37
#define I2C_MASTER_NUM      I2C_NUM_0     // 【修改】去除了原代码中重复的宏定义
#define I2C_MASTER_FREQ_HZ  400000

// INA219 I2C地址 (A0和A1都接地时为0x40)
#define INA219_I2C_ADDRESS  (0x40)

// INA219寄存器地址
#define INA219_REG_CONFIG         0x00
#define INA219_REG_SHUNT_VOLTAGE  0x01
#define INA219_REG_BUS_VOLTAGE    0x02
#define INA219_REG_POWER          0x03
#define INA219_REG_CURRENT        0x04
#define INA219_REG_CALIBRATION    0x05

// 配置值：BRNG=1(32V范围), PG=0(/1, 40mV), BADC=12bit, SADC=12bit, Mode=连续转换
#define INA219_CONFIG_VALUE       0x399F

// 分流电阻值 (欧姆) - 根据实际电路修改，通常为0.1欧姆
#define SHUNT_RESISTOR_OHMS       0.1f

// 最大预期电流 (安培)
#define MAX_EXPECTED_CURRENT      2.0f

static const char *TAG = "INA219";

// 标记 INA219 是否处于上电状态
static bool is_ina219_powered = false;
static bool is_ina219_initialized = false;

// 【新增】I2C 总线与设备句柄 (新API核心)
static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;

// 校准参数
static float current_lsb = 0.0f;  // 电流LSB (A/bit)
static float power_lsb = 0.0f;    // 功率LSB (W/bit)

typedef struct {
    float voltage;
    uint8_t percent;
} battery_table_t;

const battery_table_t battery_table[] = {
    {4.20, 100}, {4.15, 95}, {4.11, 90}, {4.08, 80}, {4.02, 70},
    {3.98, 60},  {3.95, 50}, {3.91, 40}, {3.87, 30}, {3.83, 20},
    {3.79, 10},  {3.70, 5},  {3.30, 0}
};

/**
 * @brief 初始化I2C主机 (新API: 总线-设备模型)
 */
esp_err_t i2c_master_init(void)
{
    // 1. 配置并分配 I2C 总线
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true, // 启用内部上拉 (替代旧的 sda_pullup_en)
    };
    
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_handle);
    if (err != ESP_OK) {
        return err;
    }

    // 2. 配置并添加 I2C 设备
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA219_I2C_ADDRESS, // 【注意】新API不需要左移1位，驱动会自动处理R/W位
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    
    err = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
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
 * @brief 向INA219写入16位寄存器 (新API: i2c_master_transmit)
 */
esp_err_t ina219_write_register(uint8_t reg_addr, uint16_t value)
{
    uint8_t write_buf[3];
    write_buf[0] = reg_addr;
    write_buf[1] = (value >> 8) & 0xFF;  // MSB
    write_buf[2] = value & 0xFF;         // LSB
    
    // 直接发送，超时时间 1000ms
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), 1000);
}

/**
 * @brief 从INA219读取16位寄存器 (新API: i2c_master_transmit_receive)
 */
esp_err_t ina219_read_register(uint8_t reg_addr, uint16_t *value)
{
    uint8_t write_buf[1] = {reg_addr};
    uint8_t read_buf[2] = {0};
    
    // 写寄存器地址后直接读取，无需手动插入 Repeated Start
    esp_err_t err = i2c_master_transmit_receive(dev_handle, write_buf, sizeof(write_buf), read_buf, sizeof(read_buf), 1000);
    if (err == ESP_OK) {
        *value = (read_buf[0] << 8) | read_buf[1]; // 组合大端序数据
    }
    return err;
}

/**
 * @brief 计算校准寄存器值
 */
void calculate_calibration(void)
{
    current_lsb = MAX_EXPECTED_CURRENT / 32768.0f;
    current_lsb = ceilf(current_lsb * 100000.0f) / 100000.0f;
    float cal_value = 0.04096f / (current_lsb * SHUNT_RESISTOR_OHMS);
    uint16_t calibration_reg = (uint16_t)cal_value;
    power_lsb = 20.0f * current_lsb;

    ESP_LOGI(TAG, "Current LSB: %.6f A/bit", current_lsb);
    ESP_LOGI(TAG, "Power LSB: %.6f W/bit", power_lsb);
    ESP_LOGI(TAG, "Calibration Register: 0x%04X (%d)", calibration_reg, calibration_reg);

    ina219_write_register(INA219_REG_CALIBRATION, calibration_reg);
}

/**
 * @brief 初始化INA219
 */
esp_err_t ina219_init(void)
{
    esp_err_t err = ina219_write_register(INA219_REG_CONFIG, INA219_CONFIG_VALUE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure INA219: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    calculate_calibration();
    ESP_LOGI(TAG, "INA219 initialized successfully");
    return ESP_OK;
}

float ina219_get_bus_voltage(void)
{
    uint16_t value;
    if (ina219_read_register(INA219_REG_BUS_VOLTAGE, &value) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read bus voltage");
        return -1.0f;
    }
    uint16_t bus_voltage_raw = (value >> 3);
    return bus_voltage_raw * 0.004f;
}

float ina219_get_current(void)
{
    uint16_t value;
    if (ina219_read_register(INA219_REG_CURRENT, &value) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read current");
        return -1.0f;
    }
    int16_t current_raw = (int16_t)value;
    return current_raw * current_lsb;
}

/**
 * @brief 开启 INA219 电源并初始化
 */
void ina219_power_on_and_init(void)
{
    if (is_ina219_powered) return;

    // 1. 拉高 GPIO 给 INA219 供电
    gpio_set_direction(INA219_PWR_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(INA219_PWR_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(50)); // 等待硬件稳定

    // 2. 如果I2C总线在断电时被释放，则重新初始化
    if (!is_ina219_initialized || bus_handle == NULL) {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_MASTER_NUM,
            .sda_io_num = I2C_MASTER_SDA_IO,
            .scl_io_num = I2C_MASTER_SCL_IO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        i2c_new_master_bus(&bus_config, &bus_handle);

        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = INA219_I2C_ADDRESS,
            .scl_speed_hz = I2C_MASTER_FREQ_HZ,
        };
        i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
        is_ina219_initialized = true;
    }

    // 3. 初始化 INA219 寄存器
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

    // 【关键修改】在操作GPIO防倒灌前，必须先释放I2C驱动资源，避免引脚复用冲突报错
    if (dev_handle) {
        i2c_master_bus_rm_device(dev_handle);
        dev_handle = NULL;
    }
    if (bus_handle) {
        i2c_del_master_bus(bus_handle);
        bus_handle = NULL;
    }
    is_ina219_initialized = false;

    // 【核心防倒灌】将 SDA 和 SCL 设置为输出低电平
    gpio_set_direction(I2C_MASTER_SDA_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(I2C_MASTER_SCL_IO, GPIO_MODE_OUTPUT);
    gpio_set_level(I2C_MASTER_SDA_IO, 0);
    gpio_set_level(I2C_MASTER_SCL_IO, 0);

    // 拉低 GPIO，彻底切断 INA219 供电
    gpio_set_level(INA219_PWR_PIN, 0);

    // 恢复 I2C 引脚为输入/高阻态
    gpio_set_direction(I2C_MASTER_SDA_IO, GPIO_MODE_INPUT);
    gpio_set_direction(I2C_MASTER_SCL_IO, GPIO_MODE_INPUT);

    is_ina219_powered = false;
    ESP_LOGI(TAG, "INA219 Powered OFF.");
}

void ina219_get_data_once(float *voltage_v, float *current_a)
{
    ina219_power_on_and_init();
    vTaskDelay(pdMS_TO_TICKS(10)); // 等待 ADC 转换
    
    *voltage_v = ina219_get_bus_voltage();
    *current_a = ina219_get_current();
    
    ina219_power_off(); // 立刻断电省电
}

uint8_t battery_voltage_to_percent(float voltage)
{
    const size_t count = sizeof(battery_table) / sizeof(battery_table[0]);
    if (voltage >= battery_table[0].voltage) return 100;
    if (voltage <= battery_table[count - 1].voltage) return 0;

    for (size_t i = 0; i < count - 1; i++) {
        float v_high = battery_table[i].voltage;
        float v_low  = battery_table[i + 1].voltage;
        if (voltage <= v_high && voltage >= v_low) {
            uint8_t p_high = battery_table[i].percent;
            uint8_t p_low  = battery_table[i + 1].percent;
            float ratio = (voltage - v_low) / (v_high - v_low);
            return (uint8_t)(p_low + ratio * (p_high - p_low));
        }
    }
    return 0;
}