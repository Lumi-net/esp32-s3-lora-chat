#include "uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>
#include <sys/time.h>
#include "va.h"
#include "types.h"

#define LORA_MD0 GPIO_NUM_47
#define LORA_MD1 GPIO_NUM_48
#define LORA_CHANNEL 0x3C    // 470 MHz
#define LORA_GROUP   0x00    // Lumi-net组号

uint8_t self_id = 0x01;
static uint8_t tx_seq_num = 0;
int length = 0;
uint8_t ch;
static RxState rx_state = RX_WAIT_AA;
static uint8_t rx_buf[131];
static uint16_t rx_idx = 0;
static uint8_t target_len = 0;
LoRaFrameData frame_data;

// CRC8 查找表 (多项式 x^8 + x^5 + x^4 + 1, 初始值 0)
static const uint8_t crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};


// 唤醒 LoRa 模块 (MD0=1, MD1=0)
void lora_wake() {
    gpio_set_level(LORA_MD0, 1);
    gpio_set_level(LORA_MD1, 0);
    vTaskDelay(pdMS_TO_TICKS(150)); // 等待 T2 (120ms) + 余量，确保模块进入工作状态
}

// 让 LoRa 模块休眠 (MD0=1, MD1=1)
void lora_sleep() {
    vTaskDelay(pdMS_TO_TICKS(30)); // 延时确保串口数据已被 LoRa 模块完整接收
    gpio_set_level(LORA_MD0, 1);
    gpio_set_level(LORA_MD1, 1);
    vTaskDelay(pdMS_TO_TICKS(15));
}

// 让 LoRa 模块配置 (MD0=0, MD1=0)
void lora_config() {
    gpio_set_level(LORA_MD0, 0);
    gpio_set_level(LORA_MD1, 0);
    vTaskDelay(pdMS_TO_TICKS(150)); // 等待 T1 (120ms) + 余量，确保模块进入配置状态
}

// 定点传输
void lora_config_fixed_point() { 
    lora_config();
    // 命令格式: CMD(0x80) + REG(0x07) + LEN(0x02) + VAL(0x00, 0x02)
    uint8_t cmd[] = {0x80, 0x07, 0x02, 0x00, 0x02}; 
    uart_transmit(cmd, sizeof(cmd));
    vTaskDelay(pdMS_TO_TICKS(200)); // 等待配置完成并保存
    lora_sleep(); // 配置完成后进入休眠
}

void uart_init(void)
{
    // 初始化 MD0, MD1 引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LORA_MD0) | (1ULL << LORA_MD1),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // 初始化为休眠模式
    lora_sleep();

    uart_config_t uart_structure = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .parity = UART_PARITY_DISABLE,
        .rx_flow_ctrl_thresh = 100,
        .source_clk = UART_SCLK_DEFAULT,
        .stop_bits = UART_STOP_BITS_1,
    };
    uart_param_config(UART_NUM_1, &uart_structure);
    uart_set_pin(UART_NUM_1, GPIO_NUM_19, GPIO_NUM_20, -1, -1);
    uart_driver_install(UART_NUM_1, 1024, 1024, 0, NULL, 0);

    // 配置模块为定点传输模式
    lora_config_fixed_point();
}

uint8_t calculateCRC8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0; // 初始值
    for (uint8_t i = 0; i < len; i++) {
        crc = crc8_table[crc ^ data[i]];
    }
    return crc;
}

// ================= 帧构建函数 =================
uint8_t buildLoRaFrame(uint8_t* buf, uint8_t self_id, uint8_t target_id, const char* data_str) {
    buf[0] = 0xAA;
    buf[1] = 0x55;
    // 1. 随机数 (使用ESP32硬件RNG)
    // buf[2] = esp_random() & 0xFF;
    buf[2] = tx_seq_num++; // 改为使用递增的序号 到FF自动溢出回00

    // 2. ID
    buf[3] = self_id;
    buf[4] = target_id;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *timeinfo = localtime(&tv.tv_sec);

    if (timeinfo->tm_year >= (2024 - 1900)) {
        buf[5] = timeinfo->tm_mon + 1;   // tm_mon 范围是 0-11，需要 +1
        buf[6] = timeinfo->tm_mday;        // tm_mday 范围是 1-31
        buf[7] = timeinfo->tm_hour;       // tm_hour 范围是 0-23
        buf[8] = timeinfo->tm_min;      // tm_min 范围是 0-59
    } else {
        // 刚开机? 时间默认是 1970 年，这里填 0
        buf[5] = 0;
        buf[6] = 0;
        buf[7] = 0;
        buf[8] = 0;
        ESP_LOGW("APP", "RTC time not synced yet, sending 00:00");
    }

    // 4. 数据长度 & 数据内容
    size_t len = strlen(data_str);
    if (len > 120) len = 120;
    buf[9] = (uint8_t)len;
    memcpy(&buf[10], data_str, len);

    // 5. 校验和 (CRC8)
    uint8_t crc = calculateCRC8(&buf[2], 8 + len);
    buf[10 + len] = crc;

    return 10 + len + 1; // 返回实际帧总长度
}

ParseStatus parseLoRaFrame(const uint8_t* buf, uint8_t buf_len, LoRaFrameData* out) {
    if (!out) return PARSE_ERR_PTR;
    if (buf_len < 10 + 1) return PARSE_ERR_SHORT; // 至少需要: 10字节头 + 1字节CRC

    uint8_t data_len = buf[9];
    uint8_t expected_len = 10 + data_len + 1;
    if (buf_len != expected_len) return PARSE_ERR_LEN; // 长度不匹配

    // 校验 CRC8
    uint8_t calc_crc = calculateCRC8(&buf[2], 10 + data_len);
    if (calc_crc != buf[10 + data_len]) return PARSE_ERR_CRC;

    // 填充结构体
    out->seq = buf[2];
    out->self_id    = buf[3];
    out->target_id  = buf[4];
    out->month      = buf[5];
    out->day        = buf[6];
    out->hour       = buf[7];
    out->minute     = buf[8];
    out->data_len   = data_len;
    out->checksum   = buf[10 + data_len];

    // 安全拷贝字符串并添加结束符
    if (data_len > 0) {
        memcpy(out->data_str, &buf[10], data_len);
        out->data_str[data_len] = '\0';
    } else {
        out->data_str[0] = '\0'; // ACK帧没有数据
    }

    return PARSE_OK;
}

void uart_transmit(const uint8_t* data, size_t size) {
    uart_write_bytes(UART_NUM_1, data, size);
}

void send_ack(uint8_t original_seq, uint8_t original_sender_id) {
    lora_wake();

    uint8_t ack_buf[14]; // 3字节路由头 + 11字节ACK帧
    
    // 1. 构造 3字节路由头 (目标组号 + 目标地址 + 信道)
    ack_buf[0] = LORA_GROUP;
    ack_buf[1] = original_sender_id; // ACK 的目标是原发送方
    ack_buf[2] = LORA_CHANNEL;
    
    // 2. 构造 11字节 ACK 帧
    ack_buf[3] = 0xAA;
    ack_buf[4] = 0x55;
    ack_buf[5] = original_seq;       
    ack_buf[6] = self_id;               
    ack_buf[7] = original_sender_id;
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *timeinfo = localtime(&tv.tv_sec);
    if (timeinfo->tm_year >= (2024 - 1900)) {
        ack_buf[5] = timeinfo->tm_mon + 1;   // tm_mon 范围是 0-11，需要 +1
        ack_buf[6] = timeinfo->tm_mday;        // tm_mday 范围是 1-31
        ack_buf[7] = timeinfo->tm_hour;       // tm_hour 范围是 0-23
        ack_buf[8] = timeinfo->tm_min;      // tm_min 范围是 0-59
    } else {
        // 刚开机? 时间默认是 1970 年，这里填 0
        ack_buf[5] = 0;
        ack_buf[6] = 0;
        ack_buf[7] = 0;
        ack_buf[8] = 0;
        ESP_LOGW("APP", "RTC time not synced yet, sending 00:00");
    }

    ack_buf[12] = 0; // 数据长度为 0 表示 ACK
    
    // 3. CRC8 校验 (计算范围同数据帧，从 seq 开始的 8 字节)
    uint8_t crc = calculateCRC8(&ack_buf[5], 8);
    ack_buf[13] = crc;
    
    uart_transmit(ack_buf, 14);

    lora_sleep();
}

void uart_parse_byte(uint8_t ch)
{
    switch(rx_state)
    {
        case RX_WAIT_AA:
            if(ch == 0xAA)
            {
                rx_buf[0] = ch;
                rx_idx = 1;
                rx_state = RX_WAIT_55;
            }
            break;
        case RX_WAIT_55:
            if(ch == 0x55)
            {
                rx_buf[1] = ch;
                rx_idx = 2;
                rx_state = RX_RECEIVE_HEADER;
            }
            else
            {
                rx_state = RX_WAIT_AA;
            }
            break;
        case RX_RECEIVE_HEADER:
            rx_buf[rx_idx++] = ch;
            if(rx_idx == 10)       // 2字节帧头 + 8字节协议头
            {
                uint8_t len = rx_buf[9];
                if(len > 120)
                {
                    rx_state = RX_WAIT_AA;
                    rx_idx = 0;
                    break;
                }
                target_len = 2 + 8 + len + 1;
                rx_state = RX_RECEIVE_DATA;
            }
            break;
        case RX_RECEIVE_DATA:
            if(rx_idx >= sizeof(rx_buf))
            {
                rx_state = RX_WAIT_AA;
                rx_idx = 0;
                return;
            }
            rx_buf[rx_idx++] = ch;
            if(rx_idx == target_len)
            {
                ParseStatus ret;
                ret = parseLoRaFrame(
                        rx_buf,
                        target_len,
                        &frame_data);
                if(ret == PARSE_OK)
                {
                    // 判断帧类型：data_len > 0 为数据帧，data_len == 0 为 ACK 帧
                    if (frame_data.data_len > 0) 
                    {
                        // 数据帧：判断 target_id 是否为自己 (如果是广播 target_id=0xFF，可根据需求修改判断条件)
                        if (frame_data.target_id == self_id) 
                        {
                            // 发送 ACK 给原发送方
                            send_ack(frame_data.seq, frame_data.self_id); // 此处self_id即对方发来的自己的ID，ACK中为接收方ID
                        }
                        
                        UIEvent event;
                        event.type = EVENT_UART;
                        event.frame = frame_data;
                        xQueueSend(appQueue, &event, 0);
                    }
                    else 
                    {
                        // ACK 帧：判断 target_id 是否为自己 (确保是回复给自己的 ACK)
                        if (frame_data.target_id == self_id) 
                        {
                            UIEvent event;
                            event.type = EVENT_UART; // 复用 EVENT_UART，应用层通过 data_len == 0 区分
                            event.frame = frame_data;
                            xQueueSend(appQueue, &event, 0);
                        }
                    }
                }
                rx_state = RX_WAIT_AA;
                rx_idx = 0;
            }
            break;
    }
}

void uart_receive(void *arg)
{
    while (1)
    {
        if (uart_read_bytes(UART_NUM_1, &ch, 1, pdMS_TO_TICKS(100)) > 0)
        {
            uart_parse_byte(ch);
        }
    }
}

// 封装带路由头的发送函数 (定点传输核心)
void send_lora_packet(uint8_t target_id, const uint8_t* frame_buf, uint8_t frame_len) {
    uint8_t tx_buf[134]; // 3字节路由头 + 最大131字节应用层帧
    
    // 构造 3字节路由头：目标组号 + 目标地址 + 信道
    if (target_id == 0xFF) {
        tx_buf[0] = 0xFF; tx_buf[1] = 0xFF; // 广播地址
    } else {
        tx_buf[0] = LORA_GROUP; tx_buf[1] = target_id; // 定点地址
    }
    tx_buf[2] = LORA_CHANNEL;
    
    memcpy(&tx_buf[3], frame_buf, frame_len);
    
    lora_wake(); // 发送前唤醒模块
    uart_transmit(tx_buf, 3 + frame_len);
    lora_sleep();
}

void heartbeat_task(void *arg) {
    while(1) {
        // 每隔 180~240 秒随机时间广播一次
        uint32_t delay_s = 180 + (esp_random() % 60);
        vTaskDelay(pdMS_TO_TICKS(delay_s * 1000));
        
        // 构建心跳帧 (target_id=0xFF, data="HB")
        uint8_t frame_buf[131];
        uint8_t frame_len = buildLoRaFrame(frame_buf, self_id, 0xFF, "HB");
        
        lora_wake(); // 【关键】发送前唤醒
        send_lora_packet(0xFF, frame_buf, frame_len);
        lora_sleep(); // 【关键】发送后立刻休眠
    }
}