#ifndef __FLASH_H__
#define __FLASH_H__

#include <stdint.h>
#include "types.h"
#include "esp_err.h"

esp_err_t ext_flash_init(void);
esp_err_t ext_flash_read(uint32_t addr, void *buf, size_t len);
esp_err_t ext_flash_write(uint32_t addr, const void *buf, size_t len);
esp_err_t ext_flash_erase_sector(uint32_t addr);
uint32_t ext_flash_size(void);
esp_err_t meta_init(void);
esp_err_t meta_save(const meta_t *meta);
uint32_t chat_storage_scan(void);
esp_err_t chat_storage_append(const LoRaFrameData *frame);
esp_err_t chat_switch_block(const LoRaFrameData *frame);
void chat_read_forward_init(chat_cursor_t *cursor);
esp_err_t chat_storage_read_next(chat_cursor_t *cursor, LoRaFrameData *frame);
void chat_read_backward_init(chat_cursor_t *cursor);
esp_err_t chat_storage_read_prev(chat_cursor_t *cursor, LoRaFrameData *frame);

#endif