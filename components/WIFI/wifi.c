#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h> // for isxdigit

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "wifi.h" // 引入头文件

static const char *TAG = "WIFI";
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_AP_CONFIG_DONE_BIT BIT1
#define WIFI_SNTP_SYNCED_BIT BIT2

static EventGroupHandle_t wifi_event_group = NULL;
static bool wifi_initialized = false;
static bool wifi_started = false;
static httpd_handle_t web_server = NULL; // 用于管理Web服务器生命周期

static const char html_page[] =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "<meta charset='utf-8'>"
    "<title>WiFi Config</title>"
    "</head>"
    "<body>"
    "<h2>ESP32 WiFi配置</h2>"
    "<form action='/save' method='post'>"
    "SSID:<br>"
    "<input name='ssid'><br><br>"
    "Password:<br>"
    "<input name='password' type='password'><br><br>"
    "<input type='submit' value='保存'>"
    "</form>"
    "</body>"
    "</html>";

// WiFi事件处理
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "wifi start");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "wifi disconnected");
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ESP_LOGI(TAG, "got ip");
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

// URL解码
int url_decode(char *dst, const char *src, int dst_len)
{
    int di = 0;
    while (*src && di < dst_len - 1)
    {
        if (*src == '%')
        {
            if (isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2]))
            {
                char hex[3];
                hex[0] = src[1];
                hex[1] = src[2];
                hex[2] = '\0';
                dst[di++] = (char)strtol(hex, NULL, 16);
                src += 3;
                continue;
            }
        }
        else if (*src == '+')
        {
            dst[di++] = ' ';
            src++;
            continue;
        }
        dst[di++] = *src++;
    }
    dst[di] = '\0';
    return di;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
        return ESP_FAIL;
    buf[len] = 0;

    char ssid_enc[128] = {0};
    char password_enc[128] = {0};

    // 解析表单数据
    sscanf(buf, "ssid=%127[^&]&password=%127s", ssid_enc, password_enc);

    char ssid[33] = {0};
    char password[65] = {0};

    url_decode(ssid, ssid_enc, sizeof(ssid));
    url_decode(password, password_enc, sizeof(password));

    ESP_LOGI(TAG, "Web Config SSID:%s", ssid);

    wifi_config_t sta_cfg = {0};
    memcpy(sta_cfg.sta.ssid, ssid, strlen(ssid));
    memcpy(sta_cfg.sta.password, password, strlen(password));

    // 存入NVS
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    if (ret == ESP_OK)
    {
        if (wifi_event_group != NULL)
        {
            xEventGroupSetBits(wifi_event_group, WIFI_AP_CONFIG_DONE_BIT);
        }
        httpd_resp_sendstr(req, "OK, reboot or connect now");
    }
    else
    {
        httpd_resp_sendstr(req, "FAILED");
    }
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler};
        httpd_register_uri_handler(server, &root);

        httpd_uri_t save = {
            .uri = "/save",
            .method = HTTP_POST,
            .handler = save_handler};
        httpd_register_uri_handler(server, &save);
    }
    return server;
}

/*
 * 初始化WiFi驱动 (同时创建STA和AP的netif，方便后续模式切换)
 */
esp_err_t wifi_time_init(void)
{
    if (wifi_initialized)
        return ESP_OK;

    wifi_event_group = xEventGroupCreate();
    if (!wifi_event_group)
        return ESP_ERR_NO_MEM;

    esp_err_t ret;
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        return ret;

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        return ret;

    // 同时创建 STA 和 AP 的网络接口，以支持后续的模式切换
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK)
        return ret;

    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    if (ret != ESP_OK)
        return ret;
    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    if (ret != ESP_OK)
        return ret;

    // 默认先设置为STA模式
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
        return ret;

    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    wifi_initialized = true;
    return ESP_OK;
}

/*
 * 启动 Soft-AP 及 Web 配网服务器
 */
esp_err_t wifi_ap_start(const char *password)
{
    if (!wifi_initialized)
    {
        esp_err_t ret = wifi_time_init();
        if (ret != ESP_OK)
            return ret;
    }

    esp_err_t ret;

    // 切换模式前必须先停止WiFi
    esp_wifi_stop();

    wifi_config_t ap_config = {0};

    strncpy((char *)ap_config.ap.ssid, "ESP32S3_Config", sizeof(ap_config.ap.ssid) - 1);

    strncpy((char *)ap_config.ap.password, password, sizeof(ap_config.ap.password) - 1);

    ap_config.ap.ssid_len = 0;
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK)
        return ret;

    ret = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ret != ESP_OK)
        return ret;

    ret = esp_wifi_start();
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "AP started, IP: 192.168.4.1");
        // 启动Web配网服务器
        if (web_server == NULL)
        {
            web_server = start_webserver();
        }
    }
    return ret;
}

/*
 * 停止 Soft-AP 及 Web 配网服务器
 */
void wifi_ap_stop(void)
{
    if (web_server != NULL)
    {
        httpd_stop(web_server);
        web_server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
    esp_wifi_stop();
}

/*
 * 开启WiFi并连接 (STA模式)
 */
bool wifi_time_connect(const char *ssid, const char *password)
{
    if (!wifi_initialized)
    {
        if (wifi_time_init() != ESP_OK)
            return false;
    }

    esp_err_t ret;

    // 如果之前处于AP模式，需要先停止并切换为STA模式
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);

    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_CONN)
    {
        ESP_LOGE(TAG, "wifi start failed %s", esp_err_to_name(ret));
        return false;
    }
    wifi_started = true;

    if (ssid != NULL)
    {
        wifi_config_t cfg = {0};
        strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
        if (password)
        {
            strncpy((char *)cfg.sta.password, password, sizeof(cfg.sta.password) - 1);
        }
        ret = esp_wifi_set_config(WIFI_IF_STA, &cfg);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "set config failed");
            return false;
        }
    }

    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
    ret = esp_wifi_connect();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "connect failed");
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "wifi connected");
        return true;
    }

    ESP_LOGW(TAG, "wifi timeout");
    return false;
}

// 【新增】SNTP 同步成功回调函数 (由 lwIP 底层线程安全调用)
static void sntp_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP time synchronized!");
    if (wifi_event_group != NULL)
    {
        xEventGroupSetBits(wifi_event_group, WIFI_SNTP_SYNCED_BIT);
    }
}

/*
 * 【重构】非阻塞启动 SNTP
 * 官方推荐：在获取 IP 后调用，使用 esp_sntp 封装层确保线程安全
 */
void wifi_sntp_start(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();

    // 清除可能残留的标志
    if (wifi_event_group)
    {
        xEventGroupClearBits(wifi_event_group, WIFI_SNTP_SYNCED_BIT);
    }

    // 使用 ESP-IDF 封装的安全 API
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "pool.ntp.org");

    // 【关键】注册同步回调，替代原来的 vTaskDelay 轮询
    esp_sntp_set_time_sync_notification_cb(sntp_sync_notification_cb);

    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started, waiting for callback...");
}

/*
 * 【重构】非阻塞检查 SNTP 是否已同步
 */
bool wifi_sntp_is_synced(void)
{
    if (wifi_event_group == NULL)
        return false;

    EventBits_t bits = xEventGroupGetBits(wifi_event_group);
    if (bits & WIFI_SNTP_SYNCED_BIT)
    {
        xEventGroupClearBits(wifi_event_group, WIFI_SNTP_SYNCED_BIT);

        // 同步完成后，停止 SNTP 释放 lwIP 资源
        esp_sntp_stop();
        return true;
    }
    return false;
}

// /*
//  * SNTP同步时间
//  */
// bool wifi_time_sync(void)
// {
//     setenv("TZ", "CST-8", 1);
//     tzset();

//     esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
//     esp_sntp_setservername(0, "ntp.aliyun.com");
//     esp_sntp_setservername(1, "pool.ntp.org");
//     esp_sntp_init();

//     time_t now;
//     struct tm timeinfo;
//     for (int i = 0; i < 20; i++) {
//         time(&now);
//         localtime_r(&now, &timeinfo);
//         // 2024年及以后认为有效 (1900 + 124 = 2024)
//         if (timeinfo.tm_year >= 124) {
//             ESP_LOGI(TAG, "time ok");
//             esp_sntp_stop();
//             return true;
//         }
//         vTaskDelay(pdMS_TO_TICKS(500));
//     }

//     esp_sntp_stop();
//     ESP_LOGW(TAG, "time sync failed");
//     return false;
// }

/*
 * 主动关闭WiFi
 */
void wifi_time_close(void)
{
    if (!wifi_started)
        return;
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_stop();
    wifi_started = false;
    ESP_LOGI(TAG, "wifi stopped");
}

bool wifi_check_ap_config_done(void)
{
    if (wifi_event_group == NULL)
        return false;

    EventBits_t bits = xEventGroupGetBits(wifi_event_group);
    if (bits & WIFI_AP_CONFIG_DONE_BIT)
    {
        // 清除标志位，确保下次不会误触发
        xEventGroupClearBits(wifi_event_group, WIFI_AP_CONFIG_DONE_BIT);
        return true;
    }
    return false;
}

/**
 * @brief 手动设置系统时间
 *
 * @param year   年份 (例如 2026)
 * @param month  月份 (1-12)
 * @param day    日期 (1-31)
 * @param hour   小时 (0-23)
 * @param minute 分钟 (0-59)
 * @param second 秒 (0-59)
 * @return true 设置成功, false 失败
 */
bool set_system_time_manual(int year, int month, int day, int hour, int minute, int second)
{
    // 1. 确保时区已设置 (建议在 app_main 初始化时全局设置一次即可)
    // setenv("TZ", "CST-8", 1);
    // tzset();

    // 2. 填充 tm 结构体 (注意：tm_year 是减去 1900 的值，tm_mon 是 0-11)
    struct tm timeinfo = {0};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;

    // 3. 转换为 time_t (Unix 时间戳)
    time_t new_time = mktime(&timeinfo);
    if (new_time == -1)
    {
        ESP_LOGE("MANUAL_TIME", "mktime failed! Invalid date.");
        return false;
    }

    // 4. 构造 timeval 并调用系统 API 设置时间
    struct timeval now = {
        .tv_sec = new_time,
        .tv_usec = 0};

    if (settimeofday(&now, NULL) != 0)
    {
        ESP_LOGE("MANUAL_TIME", "settimeofday failed!");
        return false;
    }

    ESP_LOGI("MANUAL_TIME", "Time set manually: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);
    return true;
}

bool check_wifi_saved(void)
{
    wifi_config_t wifi_config;
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);

    if (err == ESP_OK)
    {
        if (strlen((char *)wifi_config.sta.ssid) > 0) return true;
        else return false;
    }
    else 
    {
        return false;
    }
}