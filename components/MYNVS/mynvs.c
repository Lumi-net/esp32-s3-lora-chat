#include "nvs.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "va.h"
#include "pwm.h"

static const char *TAG = "NVS";

#define NVS_NAMESPACE      "alias"      // NVS 命名空间
#define ALIAS_KEY_PREFIX   "a_"         // Key 的前缀
#define MAX_ALIAS_LEN      16           // 昵称最大字符数（不含 \0）

esp_err_t nvs_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated or version changed, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t nvs_set_alias(uint8_t id, const char *alias) {
    // 边界检查
    if (id >= (sizeof(chat_list) / sizeof(chat_list[0]))) {
        ESP_LOGE(TAG, "ID %d out of bounds!", id);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    char key[8];
    snprintf(key, sizeof(key), "%s%u", ALIAS_KEY_PREFIX, id); // 生成 key (如 "a_5")

    
    if (alias == NULL || strlen(alias) == 0) { // 为空或者没长度视为删除(事实上这里不应该能删除 因为加联系人的时候就必须设置一个别名 删除了就没名字了)
        err = nvs_erase_key(handle, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK; // 本来就没有也算成功
        }
        
        // 同步清空内存中的记录
        if (err == ESP_OK) {
            chat_list[id].id = 0xFF; // 标记为空
            memset(chat_list[id].alias, 0, sizeof(chat_list[id].alias));
            ESP_LOGI(TAG, "Cleared ID: %d from NVS and RAM", id);
        }
    } else {
        // 校验长度 (strlen不含\0)
        if (strlen(alias) > MAX_ALIAS_LEN) {
            ESP_LOGE(TAG, "Alias too long! Max is %d chars.", MAX_ALIAS_LEN);
            nvs_close(handle);
            return ESP_ERR_INVALID_SIZE;
        }
        
        err = nvs_set_str(handle, key, alias);
        
        // 同步更新内存中的记录
        if (err == ESP_OK) {
            chat_list[id].id = id;
            
            // 安全拷贝字符串 防止溢出
            strncpy(chat_list[id].alias, alias, sizeof(chat_list[id].alias) - 1);
            chat_list[id].alias[sizeof(chat_list[id].alias) - 1] = '\0';
            
            ESP_LOGI(TAG, "Saved ID: %d, Alias: [%s] to NVS and RAM", id, chat_list[id].alias);
        }
    }

    // 提交更改到 NVS Flash
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit NVS changes!");
        }
    }
    
    nvs_close(handle);
    return err;
}

esp_err_t nvs_set_user_color(uint8_t id, uint32_t color) { // 不可以删除 如果不想要了就随机生成一个 和开始一样
    if (id >= (sizeof(chat_list) / sizeof(chat_list[0]))) {
        ESP_LOGE(TAG, "ID %d out of bounds!", id);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // 使用 "uc_" 作为颜色专属前缀，例如 "uc_5"
    char key[16];
    snprintf(key, sizeof(key), "uc_%u", id);

    err = nvs_set_u32(handle, key, color);

    if (err == ESP_OK) {
        chat_list[id].color = color; // 同步更新内存
        ESP_LOGI(TAG, "Saved ID: %d, User Color: 0x%06X to NVS and RAM", id, color);
    }

    if (err == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t nvs_set_interface_color(color_nvs_t id, uint32_t color) {
    if (id >= COLOR_MAX) {
        ESP_LOGE(TAG, "ID %d out of bounds!", id);
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    // 使用 "ic_" 作为颜色专属前缀，例如 "ic_0"
    char key[16];
    snprintf(key, sizeof(key), "ic_%u", id);

    err = nvs_set_u32(handle, key, color);

    if (err == ESP_OK) {
        color_index[id].color = color; // 同步更新内存
        ESP_LOGI(TAG, "Saved ID: %d, Interface Color: 0x%06X to NVS and RAM", id, color);
    }

    if (err == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t nvs_set_sleep_time(uint16_t time) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u16(handle, "st", time);

    if (err == ESP_OK) {
        auto_sleep_timeout = time; // 同步更新内存
        ESP_LOGI(TAG, "Saved Auto Sleep Time %u to NVS and RAM", time);
    }

    if (err == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

esp_err_t nvs_set_brightness(uint16_t brightness) {
    if (brightness > 1000) {
        ESP_LOGE(TAG, "Brightness too high! Max is 1000");
        return ESP_ERR_INVALID_ARG;
    } else if (brightness < 100) {
        ESP_LOGE(TAG, "Brightness too low! Min is 100");
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_u16(handle, "br", brightness);

    if (err == ESP_OK) {
        scr_brightness = brightness; // 同步更新内存
        duty_set(brightness);
        ESP_LOGI(TAG, "Saved Brightness %u to NVS and RAM", brightness);
    }

    if (err == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

void nvs_read_all_alias_to_list(void) {
    for (int i = 0; i < 256; i++) {
        chat_list[i].id = 0xFF; // 手动标记所有槽位为空
    }
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for iteration");
        return;
    }

    // ESP_LOGI(TAG, "--- Start iterating all aliass ---");
    
    nvs_iterator_t it = NULL;
    // 查找nvs分区下idname命名空间中的所有字符串类型Key
    nvs_entry_find("nvs", NVS_NAMESPACE, NVS_TYPE_STR, &it);
    
    int count = 0;
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        
        // 从 key 中解析出 ID
        uint8_t id = 0;
        if (sscanf(info.key, "a_%hhu", &id) == 1) { // 从Key里面提取出id
            
            if (id < 256) {
                char nick[17] = {0}; // 大小与 alias[17] 保持一致
                size_t len = sizeof(nick);
                
                if (nvs_get_str(handle, info.key, nick, &len) == ESP_OK) {
                    // 1. 存入 ID
                    chat_list[id].id = id;
                    
                    // 2. 安全拷贝字符串，防止溢出，并确保以 '\0' 结尾
                    strncpy(chat_list[id].alias, nick, sizeof(chat_list[id].alias) - 1);
                    chat_list[id].alias[sizeof(chat_list[id].alias) - 1] = '\0';
                    
                    ESP_LOGI(TAG, "Read ID: 0x%02X (%3d), Alias: [%s]", 
                             id, id, chat_list[id].alias);
                    count++;
                } else {
                    ESP_LOGW(TAG, "Failed to get string for key: %s", info.key);
                }
            } else {
                ESP_LOGW(TAG, "ID %d out of bounds! (chat_list size is 256, max index 255)", id);
            }
        }
        
        nvs_entry_next(&it); // 移动到下一个
    }
    
    ESP_LOGI(TAG, "--- Total found: %d items ---", count);
    
    nvs_release_iterator(it); // 释放迭代器
    nvs_close(handle);
}

void nvs_read_all_user_color_to_list(void) {
    // 先为所有已存在的用户赋予默认颜色(理论上所有用户都有颜色 具体见设置颜色的函数)
    // 注意：这里只判断 id != 0xFF，绝对不要重置 chat_list[i].id = 0xFF，否则会覆盖 alias 数据！
    for (int i = 0; i < 256; i++) {
        if (chat_list[i].id != 0xFF) {
            chat_list[i].color = 0x000000; 
        }
    }

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for color iteration");
        return;
    }

    nvs_iterator_t it = NULL;
    // 颜色是 uint32_t，所以这里查找 NVS_TYPE_U32
    nvs_entry_find("nvs", NVS_NAMESPACE, NVS_TYPE_U32, &it);
    
    int count = 0;
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        
        uint8_t id = 0;
        // 匹配前缀为 "uc_" 的键，例如 "uc_5"。%hhu 对应 uint8_t
        if (sscanf(info.key, "uc_%hhu", &id) == 1) { 
            
            if (id < 256) {
                uint32_t color = 0;
                
                if (nvs_get_u32(handle, info.key, &color) == ESP_OK) {
                    chat_list[id].color = color;
                    
                    // 防御性编程：如果由于某种原因 alias 还没读，确保该槽位被标记为有效
                    if (chat_list[id].id == 0xFF) {
                        chat_list[id].id = id; 
                    }

                    ESP_LOGI(TAG, "Read ID: 0x%02X (%3d), Color: 0x%06X", 
                             id, id, color);
                    count++;
                } else {
                    ESP_LOGW(TAG, "Failed to get u32 for key: %s", info.key);
                }
            } else {
                ESP_LOGW(TAG, "ID %d out of bounds! (chat_list size is 256)", id);
            }
        }
        
        nvs_entry_next(&it); // 移动到下一个
    }
    
    ESP_LOGI(TAG, "--- Total custom colors found: %d items ---", count);
    
    nvs_release_iterator(it); // 释放迭代器
    nvs_close(handle);
}

void nvs_read_all_interface_color_to_list(void) {
    color_index[COLOR_BACKGROUND].id = COLOR_BACKGROUND;
    color_index[COLOR_BACKGROUND].color = 0x000000;
    color_index[COLOR_MSG_TEXT].id = COLOR_MSG_TEXT;
    color_index[COLOR_MSG_TEXT].color = 0xffffff;
    color_index[COLOR_MSG_TIME].id = COLOR_MSG_TEXT;
    color_index[COLOR_MSG_TIME].color = 0xaaaaaa;
    color_index[COLOR_MSG_DATE].id = COLOR_MSG_DATE;
    color_index[COLOR_MSG_DATE].color = 0xaaaaaa;
    color_index[COLOR_STATUS_BAR].id = COLOR_STATUS_BAR;
    color_index[COLOR_STATUS_BAR].color = 0x000000;
    color_index[COLOR_TEXTAREA_BACKGROUND].id = COLOR_TEXTAREA_BACKGROUND;
    color_index[COLOR_TEXTAREA_BACKGROUND].color = 0x666666;

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for color iteration");
        return;
    }

    nvs_iterator_t it = NULL;
    // 颜色是 uint32_t，所以这里查找 NVS_TYPE_U32
    nvs_entry_find("nvs", NVS_NAMESPACE, NVS_TYPE_U32, &it);
    
    int count = 0;
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        
        uint8_t id = 0;
        // 匹配前缀为 "ic_" 的键，例如 "ic_5"。%hhu 对应 uint8_t
        if (sscanf(info.key, "ic_%hhu", &id) == 1) { 
            
            if (id < COLOR_MAX) {
                uint32_t color = 0;
                
                if (nvs_get_u32(handle, info.key, &color) == ESP_OK) {
                    color_index[id].color = color;

                    ESP_LOGI(TAG, "Read ID: 0x%02X (%3d), Interface Color: 0x%06X", 
                             id, id, color);
                    count++;
                } else {
                    ESP_LOGW(TAG, "Failed to get u32 for key: %s", info.key);
                }
            } else {
                ESP_LOGW(TAG, "ID %d out of bounds! (color_index size is COLOR_MAX)", id);
            }
        }
        
        nvs_entry_next(&it); // 移动到下一个
    }
    
    ESP_LOGI(TAG, "--- Total custom interface colors found: %d items ---", count);
    
    nvs_release_iterator(it); // 释放迭代器
    nvs_close(handle);
}

void nvs_read_sleep_time(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for sleep time iteration");
        return;
    }

    nvs_iterator_t it = NULL;
    nvs_entry_find("nvs", NVS_NAMESPACE, NVS_TYPE_U16, &it);

    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        uint16_t time = 300;
        if (strcmp(info.key, "st") == 0) {
            if (nvs_get_u16(handle, info.key, &time) == ESP_OK) {
                auto_sleep_timeout = time;
                ESP_LOGI(TAG, "Read Sleep Time: %u", time);
            } else {
                ESP_LOGW(TAG, "Failed to get u16 for key: %s", info.key);
            }
        }
        
        nvs_entry_next(&it); // 移动到下一个
    }
    
    nvs_release_iterator(it); // 释放迭代器
    nvs_close(handle);
}

void nvs_read_brightness(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for brightness iteration");
        return;
    }

    nvs_iterator_t it = NULL;
    nvs_entry_find("nvs", NVS_NAMESPACE, NVS_TYPE_U16, &it);

    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        uint16_t brightness = 500;
        if (strcmp(info.key, "br") == 0) {
            if (nvs_get_u16(handle, info.key, &brightness) == ESP_OK) {
                if (brightness > 1000) brightness = 1000;
                scr_brightness = brightness;
                ESP_LOGI(TAG, "Read Brightness: %u", brightness);
            } else {
                ESP_LOGW(TAG, "Failed to get u16 for key: %s", info.key);
            }
        }
        
        nvs_entry_next(&it); // 移动到下一个
    }
    
    nvs_release_iterator(it); // 释放迭代器
    nvs_close(handle);
}