#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WIFI_CONFIGS 5

typedef struct {
    char ssid[33];
    char password[65];
} wifi_config_info_t;

esp_err_t wifi_driver_init(void);

esp_err_t wifi_driver_scan(wifi_ap_record_t *ap_list, uint16_t *ap_count);

esp_err_t wifi_driver_connect(const char *ssid, const char *password);

esp_err_t wifi_driver_save_config(const char *ssid, const char *password);

esp_err_t wifi_driver_get_saved_configs(wifi_config_info_t *configs, int *count);

esp_err_t wifi_driver_delete_config(void);

bool wifi_driver_is_connected(void);

#ifdef __cplusplus
}
#endif
