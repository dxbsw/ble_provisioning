#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 最多保存的 WiFi 凭据数量
 */
#define MAX_WIFI_CONFIGS 5

/**
 * @brief WiFi 凭据信息
 */
typedef struct {
    char ssid[33];
    char password[65];
} wifi_config_info_t;

typedef enum {
    WIFI_DRIVER_OPTION_DEFAULT = 0,
    WIFI_DRIVER_OPTION_ENABLE = 1,
    WIFI_DRIVER_OPTION_DISABLE = 2,
} wifi_driver_option_t;

typedef enum {
    WIFI_DRIVER_RECONNECT_MODE_DEFAULT = 0,
    WIFI_DRIVER_RECONNECT_MODE_FIXED = 1,
    WIFI_DRIVER_RECONNECT_MODE_LINEAR_STEP = 2,
} wifi_driver_reconnect_mode_t;

typedef struct {
    wifi_driver_option_t enable;
    wifi_driver_reconnect_mode_t mode;
    wifi_driver_option_t continue_after_max;
    uint32_t max_attempts;
    uint32_t fixed_interval_ms;
    uint32_t step_interval_ms;
    uint32_t max_interval_ms;
    uint32_t jitter_ms;
} wifi_driver_reconnect_config_t;

/**
 * @brief 初始化 WiFi STA 驱动与事件回调
 *
 * @return
 *      - ESP_OK: 成功
 *      - 其他: ESP-IDF 返回的错误码
 */
esp_err_t wifi_driver_init(void);

/**
 * @brief 扫描周边 WiFi
 *
 * @param[out] ap_list  扫描结果数组
 * @param[in,out] ap_count 输入为数组容量，输出为实际扫描到的数量（最多不超过输入值）
 *
 * @return
 *      - ESP_OK: 成功
 *      - 其他: ESP-IDF 返回的错误码
 */
esp_err_t wifi_driver_scan(wifi_ap_record_t *ap_list, uint16_t *ap_count);

/**
 * @brief 连接到指定 WiFi
 *
 * @param[in] ssid     WiFi 名称
 * @param[in] password WiFi 密码（可为 NULL）
 *
 * @return
 *      - ESP_OK: 连接成功并获取到 IP
 *      - ESP_ERR_INVALID_ARG: 参数错误
 *      - ESP_FAIL: 连接失败或超时
 *      - 其他: ESP-IDF 返回的错误码
 */
esp_err_t wifi_driver_connect(const char *ssid, const char *password);

esp_err_t wifi_driver_set_reconnect_config(const wifi_driver_reconnect_config_t *cfg);

void wifi_driver_set_reconnect_enabled(bool enabled);

bool wifi_driver_get_reconnect_enabled(void);

wifi_err_reason_t wifi_driver_get_last_disconnect_reason(void);

bool wifi_driver_last_disconnect_is_auth_error(void);

/**
 * @brief 保存 WiFi 凭据到 NVS（支持最多 MAX_WIFI_CONFIGS 条，自动去重与前插）
 *
 * @param[in] ssid     WiFi 名称
 * @param[in] password WiFi 密码（可为 NULL）
 *
 * @return
 *      - ESP_OK: 成功
 *      - 其他: NVS 返回的错误码
 */
esp_err_t wifi_driver_save_config(const char *ssid, const char *password);

/**
 * @brief 读取已保存的 WiFi 凭据列表
 *
 * @param[out] configs 输出数组
 * @param[out] count   输出数量
 *
 * @return
 *      - ESP_OK: 成功
 *      - 其他: NVS 返回的错误码
 */
esp_err_t wifi_driver_get_saved_configs(wifi_config_info_t *configs, int *count);

/**
 * @brief 删除已保存的 WiFi 凭据
 *
 * @return
 *      - ESP_OK: 成功
 *      - 其他: NVS 返回的错误码
 */
esp_err_t wifi_driver_delete_config(void);

/**
 * @brief 查询当前是否已连接并获取到 IP
 *
 * @return true 已连接，false 未连接
 */
bool wifi_driver_is_connected(void);

#ifdef __cplusplus
}
#endif
