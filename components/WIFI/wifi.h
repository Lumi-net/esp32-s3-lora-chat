#pragma once

#include "esp_err.h"

esp_err_t wifi_time_init(void);
esp_err_t wifi_ap_start(const char *password);
void wifi_ap_stop(void);
bool wifi_time_connect(const char *ssid, const char *password);
void wifi_sntp_start(void);
bool wifi_sntp_is_synced(void);
void wifi_time_close(void);
bool wifi_check_ap_config_done(void);
bool check_wifi_saved(void);
bool set_system_time_manual(int year, int month, int day, int hour, int minute, int second);