#include "flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_flash.h"
#include "esp_flash_spi_init.h"
#include "uart.h"
#include "va.h"

#define FLASH_SIZE              (8 * 1024 * 1024UL)

#define FLASH_SECTOR_SIZE       4096

#define META_ADDR               0x000000
#define META_SIZE               FLASH_SECTOR_SIZE

#define CHAT_BLOCK_A_ADDR       META_SIZE

#define CHAT_BLOCK_SIZE         ((FLASH_SIZE - META_SIZE) / 2)

#define CHAT_BLOCK_B_ADDR       (CHAT_BLOCK_A_ADDR + CHAT_BLOCK_SIZE)

#define META_MAGIC              0x5AA5
#define CHAT_MAGIC              0xAA55

// TODO: 修改GPIO引脚
#define PIN_NUM_MISO 13
#define PIN_NUM_MOSI 11
#define PIN_NUM_CLK  12
#define PIN_NUM_CS   10

static esp_flash_t *s_ext_flash = NULL;
static uint32_t s_flash_size = 0;
static meta_t g_meta;
static uint32_t g_meta_offset = 0;
uint32_t g_write_offset;

static inline uint32_t get_active_addr(void)
{
    return (g_meta.active_block == 0)
        ? CHAT_BLOCK_A_ADDR
        : CHAT_BLOCK_B_ADDR;
}

static inline uint32_t get_inactive_addr(void)
{
    return (g_meta.active_block == 0)
        ? CHAT_BLOCK_B_ADDR
        : CHAT_BLOCK_A_ADDR;
}

// 将 LoRaFrameData 的时间打包为 MMDDHHMM 格式的整数
static inline uint32_t pack_frame_time(const LoRaFrameData *frame)
{
    return (uint32_t)frame->month * 1000000 + 
           (uint32_t)frame->day * 10000 + 
           (uint32_t)frame->hour * 100 + 
           (uint32_t)frame->minute;
}

static inline uint16_t chat_record_size(const LoRaFrameData *frame)
{
    return
        offsetof(LoRaFrameData, data_str) +
        frame->data_len +
        sizeof(frame->checksum) +
        sizeof(frame->data_len) +
        sizeof(uint16_t); // Magic
}

esp_err_t ext_flash_init(void)
{

    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));


    esp_flash_spi_device_config_t devcfg = {
        .host_id = SPI3_HOST,
        .cs_io_num = PIN_NUM_CS,
        .io_mode = SPI_FLASH_FASTRD,
        .freq_mhz = 40,
        .input_delay_ns = 0,
    };

    ESP_ERROR_CHECK(spi_bus_add_flash_device(&s_ext_flash, &devcfg));
    ESP_ERROR_CHECK(esp_flash_init(s_ext_flash));
    return esp_flash_get_physical_size(s_ext_flash,
                                      &s_flash_size);
}

esp_err_t ext_flash_read(uint32_t addr, void *buf, size_t len)
{
    if (s_ext_flash == NULL)
        return ESP_ERR_INVALID_STATE;

    return esp_flash_read(
            s_ext_flash,
            buf,
            addr,
            len);
}

esp_err_t ext_flash_write(uint32_t addr,
                          const void *buf,
                          size_t len)
{
    if (s_ext_flash == NULL)
        return ESP_ERR_INVALID_STATE;

    return esp_flash_write(
            s_ext_flash,
            buf,
            addr,
            len);
}

esp_err_t ext_flash_erase_sector(uint32_t addr)
{
    if (s_ext_flash == NULL)
        return ESP_ERR_INVALID_STATE;

    addr &= ~(4096 - 1);

    return esp_flash_erase_region(
            s_ext_flash,
            addr,
            4096);
}

uint32_t ext_flash_size(void)
{
    return s_flash_size;
}

esp_err_t meta_save(const meta_t *meta)
{
    esp_err_t err;

    if(g_meta_offset + sizeof(meta_t) > META_SIZE)
    {
        err = ext_flash_erase_sector(META_ADDR);

        if(err != ESP_OK)
            return err;

        g_meta_offset = 0;
    }

    err = ext_flash_write(META_ADDR + g_meta_offset,
                          meta,
                          sizeof(meta_t));

    if(err != ESP_OK)
        return err;

    g_meta_offset += sizeof(meta_t);

    return ESP_OK;
}

esp_err_t meta_init(void)
{
    meta_t meta;

    g_meta_offset = 0;

    while(g_meta_offset + sizeof(meta_t) <= META_SIZE)
    {
        if(ext_flash_read(META_ADDR + g_meta_offset,
                          &meta,
                          sizeof(meta)) != ESP_OK)
        {
            return ESP_FAIL;
        }

        if(meta.magic != META_MAGIC)
            break;


        if(meta.active_block > 1)
            break;

        g_meta = meta;

        g_meta_offset += sizeof(meta_t);
    }

    /* Flash为空，初始化 */

    if(g_meta_offset == 0)
    {
        g_meta.magic = META_MAGIC;
        g_meta.active_block = 0;

        ESP_ERROR_CHECK(ext_flash_erase_sector(META_ADDR));

        g_meta_offset = 0;

        return meta_save(&g_meta);
    }

    return ESP_OK;
}

uint32_t chat_storage_scan(void)
{
    uint32_t offset = 0;

    uint32_t base = get_active_addr();

    uint8_t tail_len;

    uint16_t magic;

    uint16_t fixed = offsetof(LoRaFrameData, data_str);

    while(offset + fixed < CHAT_BLOCK_SIZE)
    {
        LoRaFrameData frame;

        if(ext_flash_read(base + offset,
                          &frame,
                          fixed) != ESP_OK)
            break;

        if(frame.data_len > 129)
            break;

        uint32_t record_size = chat_record_size(&frame);

        if(offset + record_size > CHAT_BLOCK_SIZE)
            break;

        /* 读字符串 */

        if (ext_flash_read(base + offset + fixed,
                           frame.data_str,
                           frame.data_len) != ESP_OK)
            break;

        frame.data_str[frame.data_len] = '\0';

        /* 读 checksum */

        if (ext_flash_read(base + offset + fixed + frame.data_len,
                           &frame.checksum,
                           sizeof(frame.checksum)) != ESP_OK)
            break;

        /* 验证 checksum */

        uint8_t crc = calculateCRC8((const uint8_t *)&frame, offsetof(LoRaFrameData, data_str) +frame.data_len);

        if (crc != frame.checksum)
            break;

        /* 验证数据长度 */
        if (ext_flash_read(base + offset + fixed + frame.data_len + sizeof(frame.checksum),
                           &tail_len,
                           sizeof(tail_len)) != ESP_OK)
            break;

        if (tail_len != frame.data_len)
            break;

        /* 验证 Magic */

        if (ext_flash_read(base + offset + record_size - sizeof(magic),
                           &magic,
                           sizeof(magic)) != ESP_OK)
            break;

        if (magic != CHAT_MAGIC)
            break;

        offset += record_size;
    }

    return offset;
}

esp_err_t chat_storage_append(const LoRaFrameData *frame)
{
    uint16_t fixed =
        offsetof(LoRaFrameData, data_str);

    uint16_t magic = CHAT_MAGIC;

    size_t record_size = chat_record_size(frame);

    if (record_size > CHAT_BLOCK_SIZE) {
        return ESP_ERR_INVALID_SIZE;  // 虽然我也不知道怎么可能会出现这么大的数据，但是AI让我防一下
    }

    /* 检测是否需要切换块 */

    if(g_write_offset + record_size > CHAT_BLOCK_SIZE)
    {
        return chat_switch_block(frame);
    }

    esp_err_t err;

    uint32_t addr =
        get_active_addr() + g_write_offset;

    /* 固定部分 */

    err = ext_flash_write(addr,
                          frame,
                          fixed); // 只写帧的前fixed字节
    if(err != ESP_OK)
        return err;

    addr += fixed;

    /* data */

    err = ext_flash_write(addr, 
                          frame->data_str,
                          frame->data_len);
    if(err != ESP_OK)
        return err;

    addr += frame->data_len;

    /* checksum */

    err = ext_flash_write(addr,
                          &frame->checksum,
                          sizeof(frame->checksum));
    if(err != ESP_OK)
        return err;

    addr += sizeof(frame->checksum);

    /* data_len */

    err = ext_flash_write(addr,
                          &frame->data_len,
                          sizeof(frame->data_len));
    if(err != ESP_OK)
        return err;

    addr += sizeof(frame->data_len);

    /* 最后Magic */

    err = ext_flash_write(addr,
                          &magic,
                          sizeof(magic));
    if(err != ESP_OK)
        return err;

    g_write_offset += record_size;

    return ESP_OK;
}

esp_err_t chat_switch_block(const LoRaFrameData *frame)
{
    esp_err_t err;

    meta_t old_meta = g_meta;
    uint32_t old_offset = g_write_offset;

    uint32_t new_addr = get_inactive_addr();

    /* 擦除整个块 */

    for(uint32_t i = 0; i < CHAT_BLOCK_SIZE; i += FLASH_SECTOR_SIZE)
    {
        err = ext_flash_erase_sector(new_addr + i);

        if(err != ESP_OK)
            return err;
    }

    /* 临时准备新的Meta */

    meta_t new_meta = g_meta;
    uint32_t new_write_offset = g_write_offset;

    new_meta.active_block ^= 1;
    new_write_offset = 0;

    /* 暂时切换到RAM中的Meta */

    g_meta = new_meta;
    g_write_offset = new_write_offset;

    /* 写第一条 */

    err = chat_storage_append(frame);

    if(err != ESP_OK)
    {
        g_meta = old_meta;
        g_write_offset = old_offset;
        return err;
    }

    /* 保存Meta */

    err = meta_save(&g_meta);

    return err;
}

// 获取物理块地址的辅助函数
static inline uint32_t get_block_addr(uint8_t block_idx) {
    return (block_idx == 0) ? CHAT_BLOCK_A_ADDR : CHAT_BLOCK_B_ADDR;
}

// 1. 底层正向读取（单块内）
static esp_err_t read_forward_single_block(uint8_t block_idx, uint32_t offset, 
                                           LoRaFrameData *frame, uint32_t *next_offset)
{
    uint32_t addr = get_block_addr(block_idx) + offset;
    uint16_t fixed = offsetof(LoRaFrameData, data_str);
    uint8_t tail_len;
    uint16_t magic;
    esp_err_t err;

    // 固定部分
    err = ext_flash_read(addr, frame, fixed);
    if (err != ESP_OK || frame->data_len > 129) return ESP_FAIL;
    addr += fixed;

    // data
    err = ext_flash_read(addr, frame->data_str, frame->data_len);
    if (err != ESP_OK) return err;
    frame->data_str[frame->data_len] = '\0';
    addr += frame->data_len;

    // checksum
    err = ext_flash_read(addr, &frame->checksum, sizeof(frame->checksum));
    if (err != ESP_OK) return err;
    
    uint8_t crc = calculateCRC8((const uint8_t *)frame, fixed + frame->data_len);
    if (crc != frame->checksum) return ESP_FAIL;
    addr += sizeof(frame->checksum);

    // tail_len
    err = ext_flash_read(addr, &tail_len, sizeof(tail_len));
    if (err != ESP_OK) return err;
    if (tail_len != frame->data_len) return ESP_FAIL;
    addr += sizeof(tail_len);

    // Magic
    err = ext_flash_read(addr, &magic, sizeof(magic));
    if (err != ESP_OK) return err;
    if (magic != CHAT_MAGIC) return ESP_FAIL;

    *next_offset = offset + fixed + frame->data_len + sizeof(frame->checksum) + sizeof(tail_len) + sizeof(magic);
    return ESP_OK;
}

// 2. 底层反向读取（单块内）
static esp_err_t read_backward_single_block(uint8_t block_idx, uint32_t *offset, LoRaFrameData *frame)
{
    uint32_t end = *offset;
    if (end == 0) return ESP_ERR_NOT_FOUND; // 已经读到块首了

    uint16_t fixed = offsetof(LoRaFrameData, data_str);
    
    // 计算最小记录长度
    uint16_t min_record_size = fixed + 0 + sizeof(frame->checksum) + sizeof(uint8_t) + sizeof(uint16_t);
    if (end < min_record_size) return ESP_ERR_NOT_FOUND;

    uint32_t base = get_block_addr(block_idx);
    uint8_t tail_len;
    uint16_t magic;
    esp_err_t err;

    // 读取 Magic
    err = ext_flash_read(base + end - sizeof(magic), &magic, sizeof(magic));
    if (err != ESP_OK || magic != CHAT_MAGIC) return ESP_FAIL;

    // 读取尾部 data_len
    err = ext_flash_read(base + end - sizeof(magic) - sizeof(tail_len), &tail_len, sizeof(tail_len));
    if (err != ESP_OK || tail_len > 129) return ESP_FAIL;

    uint16_t record_size = fixed + tail_len + sizeof(frame->checksum) + sizeof(tail_len) + sizeof(magic);
    if (record_size > end) return ESP_FAIL; // 二次校验

    uint32_t start = end - record_size;

    // 正向读取这一条记录
    uint32_t dummy;
    err = read_forward_single_block(block_idx, start, frame, &dummy);
    if (err != ESP_OK) return err;

    // 更新 offset 为上一条记录的末尾（即当前记录的起始位置）
    *offset = start; 
    return ESP_OK;
}

// 初始化正向读取游标
void chat_read_forward_init(chat_cursor_t *cursor)
{
    cursor->stage = 0;
    // 先读 Inactive 块（旧数据，通常是满的）
    cursor->physical_block = 1 - g_meta.active_block; 
    cursor->offset = 0;
}

// 正向读取下一条
esp_err_t chat_storage_read_next(chat_cursor_t *cursor, LoRaFrameData *frame)
{
    uint32_t next_offset = 0;
    esp_err_t err;

    while (1) {
        // 确定当前块的读取边界
        uint32_t max_offset = (cursor->physical_block == g_meta.active_block) 
                              ? g_write_offset 
                              : CHAT_BLOCK_SIZE;

        // 如果当前块已经读完
        if (cursor->offset >= max_offset) {
            if (cursor->stage == 0) {
                // 切换到第二个块 (Active 块，新数据)
                cursor->stage = 1;
                cursor->physical_block = g_meta.active_block;
                cursor->offset = 0;
                continue; // 继续尝试读取
            } else {
                // 两个块都读完了
                return ESP_ERR_NOT_FOUND;
            }
        }

        // 尝试在当前块读取
        err = read_forward_single_block(cursor->physical_block, cursor->offset, frame, &next_offset);
        if (err == ESP_OK) {
            cursor->offset = next_offset; // 更新游标
            return ESP_OK;
        }
        
        // 如果读取失败（数据损坏），跳过当前记录？或者直接终止？
        // 这里选择终止，防止死循环
        return err; 
    }
}

// 初始化反向读取游标
void chat_read_backward_init(chat_cursor_t *cursor)
{
    cursor->stage = 0;
    // 先读 Active 块（最新数据）
    cursor->physical_block = g_meta.active_block;
    cursor->offset = g_write_offset; // 从当前写入位置（最新数据的末尾）开始
}

// 反向读取上一条
esp_err_t chat_storage_read_prev(chat_cursor_t *cursor, LoRaFrameData *frame)
{
    esp_err_t err;

    while (1) {
        // 尝试在当前块反向读取
        err = read_backward_single_block(cursor->physical_block, &cursor->offset, frame);
        
        if (err == ESP_OK) {
            return ESP_OK; // 成功读到一条
        }

        // 如果当前块读到了尽头 (ESP_ERR_NOT_FOUND)
        if (err == ESP_ERR_NOT_FOUND) {
            if (cursor->stage == 0) {
                // 切换到第二个块 (Inactive 块，旧数据)
                cursor->stage = 1;
                cursor->physical_block = 1 - g_meta.active_block;
                cursor->offset = CHAT_BLOCK_SIZE; // Inactive 块是满的，从末尾开始
                continue; // 继续尝试读取
            } else {
                // 两个块都读完了
                return ESP_ERR_NOT_FOUND;
            }
        }

        // 其他错误（如数据损坏）
        return err;
    }
}

void update_chat_list_last_time(void)
{
    chat_cursor_t cursor;
    LoRaFrameData frame;
    
    // 初始化正向游标（从最旧到最新）
    chat_read_forward_init(&cursor);
    
    // 遍历所有消息
    while (chat_storage_read_next(&cursor, &frame) == ESP_OK) {
        uint8_t id = frame.self_id;
        if (id < 256 && chat_list[id].id != 0xFF) {
            uint32_t current_time = pack_frame_time(&frame);
            
            // 因为是从旧到新遍历，所以直接覆盖即可，最后留下的就是最新的
            // 如果 chat_list 是未初始化的，记得先判断或初始化 last_time 为 0
            if (current_time > chat_list[id].last_time) {
                chat_list[id].last_time = current_time;
            }
        }
    }
}