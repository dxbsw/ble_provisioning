#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BLE 保持连接配置
 * 
 * 设置为 1：在 WiFi 连接成功后，保持 BLE 广播/连接状态，允许持续通过蓝牙控制。
 * 设置为 0：在 WiFi 连接成功后，关闭 BLE 以节省功耗。
 */
#define BLE_KEEP_ALIVE_AFTER_WIFI_CONNECTED 1

/**
 * @brief BLE 配网配置结构体
 */
typedef struct {
    const char *device_name;    /**< BLE 设备名称 */
    // 如果需要，可以在此处添加其他配置项
} ble_prov_config_t;

/**
 * @brief 初始化 BLE 配网组件
 * 
 * 该函数将执行以下操作：
 * 1. 初始化 NVS（如果尚未初始化）
 * 2. 检查 NVS 中是否保存了 WiFi 凭据
 * 3. 如果存在保存的 WiFi，尝试连接（重试 5 次，循环 3 轮）
 * 4. 如果连接失败或没有保存的 WiFi，启动 BLE 配网模式
 * 
 * @param config 配置结构体指针
 * @return esp_err_t 成功返回 ESP_OK
 */
esp_err_t ble_provisioning_init(const ble_prov_config_t *config);

/**
 * @brief 手动启动 BLE 配网模式
 * 
 * @return esp_err_t 成功返回 ESP_OK
 */
esp_err_t ble_provisioning_start(void);

/**
 * @brief 停止 BLE 配网模式
 * 
 * @return esp_err_t 成功返回 ESP_OK
 */
esp_err_t ble_provisioning_stop(void);

bool ble_provisioning_is_connected(void);

#ifdef __cplusplus
}
#endif
