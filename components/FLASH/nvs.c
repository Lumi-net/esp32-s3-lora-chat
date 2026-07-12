#include "nvs.h"

#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "NVS";

#define NVS_NAMESPACE      "idname"     // NVS 命名空间
#define KEY_PREFIX         "n_"         // Key 的前缀
#define MAX_NICK_LEN       16           // 昵称最大字符数（不含 \0）

esp_err_t nvs_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated or version changed, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t app_nvs_set_nickname(uint8_t id, const char *nickname) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    char key[8];
    snprintf(key, sizeof(key), "%s%u", KEY_PREFIX, id); // 生成 key

    // 如果传入 NULL 或空字符串视为删除该 ID
    if (nickname == NULL || strlen(nickname) == 0) {
        err = nvs_erase_key(handle, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = ESP_OK;
        }
    } else {
        // 校验长度
        if (strlen(nickname) > MAX_NICK_LEN) {
            ESP_LOGE(TAG, "Nickname too long! Max is %d chars.", MAX_NICK_LEN);
            nvs_close(handle);
            return ESP_ERR_INVALID_SIZE;
        }
        err = nvs_set_str(handle, key, nickname);
    }

    // 提交更改到 Flash
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    
    nvs_close(handle);
    return err;
}

esp_err_t app_nvs_get_nickname(uint8_t id, char *out_buf, size_t buf_size) {
    if (out_buf == NULL || buf_size == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    char key[8];
    snprintf(key, sizeof(key), "%s%u", KEY_PREFIX, id);

    size_t required_size = buf_size;
    err = nvs_get_str(handle, key, out_buf, &required_size);

    nvs_close(handle);
    return err;
}

void app_nvs_print_all_nicks(void) {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for iteration");
        return;
    }

    ESP_LOGI(TAG, "--- Start iterating all nicknames ---");
    
    nvs_iterator_t it = NULL;
    // 查找nvs分区下idname命名空间中的所有字符串类型Key
    esp_err_t res = nvs_entry_find("nvs", NVS_NAMESPACE, NVS_TYPE_STR, &it);
    
    int count = 0;
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        
        // 从 key 中解析出 ID
        uint8_t id = 0;
        if (sscanf(info.key, "n_%hhu", &id) == 1) {
            
            char nick[MAX_NICK_LEN + 1] = {0};
            size_t len = sizeof(nick);
            if (nvs_get_str(handle, info.key, nick, &len) == ESP_OK) {
                ESP_LOGI(TAG, "Found ID: 0x%02X (%3d), Nickname: [%s]", id, id, nick);
                count++;
            }
        }
        
        res = nvs_entry_next(&it); // 移动到下一个
    }
    
    ESP_LOGI(TAG, "--- Total found: %d items ---", count);
    
    nvs_release_iterator(it); // 释放迭代器
    nvs_close(handle);
}