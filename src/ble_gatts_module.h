#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef esp_err_t (*ble_gatts_custom_proto_handler_t)(void *user_ctx,
                                                     const uint8_t *data,
                                                     size_t len,
                                                     char *response,
                                                     size_t response_len);

/**
 * @brief 初始化 BLE GATT 服务器
 * 
 * @param device_name 用于广播的设备名称
 * @return esp_err_t 成功返回 ESP_OK
 */
esp_err_t ble_gatts_module_init(const char *device_name);

/**
 * @brief 反初始化 BLE GATT 服务器
 * 
 * @return esp_err_t 成功返回 ESP_OK
 */
esp_err_t ble_gatts_module_deinit(void);

/**
 * @brief 开始 BLE 广播
 * 
 * @return esp_err_t 成功返回 ESP_OK
 */
esp_err_t ble_gatts_module_start_adv(void);

/**
 * @brief 停止 BLE 广播
 * 
 * @return esp_err_t 成功返回 ESP_OK
 */
esp_err_t ble_gatts_module_stop_adv(void);

/**
 * @brief 向连接的客户端发送通知 (Notification)
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @return esp_err_t 成功返回 ESP_OK
 */
esp_err_t ble_gatts_module_send_notify(const uint8_t *data, size_t len);

esp_err_t ble_gatts_module_set_custom_handler(ble_gatts_custom_proto_handler_t handler, void *user_ctx);

#ifdef __cplusplus
}
#endif
