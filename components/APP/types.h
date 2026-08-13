#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdint.h>
#include <time.h>

typedef enum
{
    LORA_MODE_CONFIG = 0, // MD0=0, MD1=0 (配置模式)
    LORA_MODE_WAKEUP = 1, // MD0=1, MD1=0 (唤醒/正常工作模式)
    LORA_MODE_SLEEP = 2   // MD0=1, MD1=1 (休眠/省电模式)
} LoraMode;

typedef enum
{
    RX_WAIT_AA,
    RX_WAIT_55,
    RX_RECEIVE_HEADER,
    RX_RECEIVE_DATA
} RxState;

typedef enum
{
    SEND_STATE_IDLE,       // 空闲状态
    SEND_STATE_WAITING_ACK // 等待 ACK 状态
} SendState;

typedef enum
{
    LORA_STATUS_IDLE,        // 空闲等待接收 (绿色)
    LORA_STATUS_RECEIVING,   // 正在接收 (蓝色)
    LORA_STATUS_SENDING,     // 正在发送 (黄色)
    LORA_STATUS_WAITING_ACK, // 已发送等待 ACK (紫色)
    LORA_STATUS_TIMEOUT,     // ACK 超时 (红色)
} LoRaStatus;

typedef struct
{
    uint8_t seq;
    uint8_t self_id;
    uint8_t target_id;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t data_len;
    char data_str[129 + 1]; // +1 用于'\0'
    uint8_t checksum;
} LoRaFrameData;

typedef enum
{
    PARSE_OK = 0,
    PARSE_ERR_PTR = 1,   // 指针为空
    PARSE_ERR_SHORT = 2, // 包太短（不足帧头+校验）
    PARSE_ERR_LEN = 3,   // 声明长度与实际长度不符
    PARSE_ERR_CRC = 4,   // CRC校验失败
    PARSE_ERR_READ = 5   // 读取不完整（LoRa超时丢字节）
} ParseStatus;

typedef enum
{
    EVENT_KEY,
    EVENT_UART,
    EVENT_WIFI
} EventType;

typedef enum
{
    WIFI_STATE_IDLE,
    WIFI_STATE_AP_RUNNING,     // AP 已启动，等待网页配置
    WIFI_STATE_CONNECTING_STA, // 网页配置完成，正在连接 STA
    WIFI_STATE_WAITING_SNTP,
    WIFI_STATE_CONNECTED // STA 连接成功
} WifiProvisioningState;

typedef struct
{
    EventType type;

    union
    {
        uint8_t key;
        LoRaFrameData frame;
        WifiProvisioningState wifi_state;
    };
} UIEvent;

typedef enum
{
    PAGE_NONE,
    PAGE_MENU,
    PAGE_SETTINGS,
    PAGE_SETTINGS_DETAIL,
    PAGE_CHAT,
    PAGE_HOME
} page_id_t;

typedef enum
{
    COLOR_BACKGROUND = 0,
    COLOR_MSG_TEXT,
    COLOR_MSG_TIME,
    COLOR_MSG_DATE,
    COLOR_STATUS_BAR,
    COLOR_TEXTAREA_BACKGROUND,
    COLOR_MAX,
} color_nvs_t;

typedef struct
{
    color_nvs_t id;
    uint32_t color;
} color_item_t;

typedef struct
{
    uint8_t id;
    char alias[17];
    uint32_t color;
    uint32_t last_time;
    time_t last_online;
} chat_item_t;

typedef struct
{
    uint8_t id;
    char content[17];
} settings_item_t;

typedef struct __attribute__((packed))
{
    uint16_t magic; // 0xAA55
} ChatRecordHeader;

typedef struct
{
    uint16_t magic;
    uint8_t active_block;
} meta_t;

// 读取游标，用于支持跨 A/B 块的连续读取
typedef struct
{
    uint8_t physical_block; // 当前正在读取的物理块 (0: Block A, 1: Block B)
    uint32_t offset;        // 当前块内的偏移量
    uint8_t stage;          // 读取阶段 (0: 正在读第一个块, 1: 正在读第二个块)
} chat_cursor_t;

#endif