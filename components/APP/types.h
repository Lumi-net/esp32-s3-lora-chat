#ifndef __TYPES_H__
#define __TYPES_H__ 

#include <stdint.h>

typedef enum
{
    RX_WAIT_AA,
    RX_WAIT_55,
    RX_RECEIVE_HEADER,
    RX_RECEIVE_DATA
} RxState;

typedef enum {
    SEND_STATE_IDLE,         // 空闲状态
    SEND_STATE_WAITING_ACK   // 等待 ACK 状态
} SendState;

typedef struct {
    uint8_t seq;
    uint8_t self_id;
    uint8_t target_id;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t data_len;
    char    data_str[129 + 1]; // +1 用于'\0'
    uint8_t checksum;
} LoRaFrameData;

typedef enum {
    PARSE_OK          = 0,
    PARSE_ERR_PTR     = 1,  // 指针为空
    PARSE_ERR_SHORT   = 2,  // 包太短（不足帧头+校验）
    PARSE_ERR_LEN     = 3,  // 声明长度与实际长度不符
    PARSE_ERR_CRC     = 4,  // CRC校验失败
    PARSE_ERR_READ    = 5   // 读取不完整（LoRa超时丢字节）
} ParseStatus;

typedef struct __attribute__((packed))
{
    uint16_t magic;        // 0xAA55
} ChatRecordHeader;

typedef struct
{
    uint16_t magic;
    uint8_t active_block;
} meta_t;

// 读取游标，用于支持跨 A/B 块的连续读取
typedef struct {
    uint8_t physical_block; // 当前正在读取的物理块 (0: Block A, 1: Block B)
    uint32_t offset;        // 当前块内的偏移量
    uint8_t stage;          // 读取阶段 (0: 正在读第一个块, 1: 正在读第二个块)
} chat_cursor_t;

#endif