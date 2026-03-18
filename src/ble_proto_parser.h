#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// 命令 ID 定义
#define CMD_CONNECT       0x01  // 连接命令
#define CMD_WIFI_SCAN     0x02  // WiFi 扫描命令
#define CMD_WIFI_CONNECT  0x03  // WiFi 连接命令
#define CMD_WIFI_QUERY    0x04  // WiFi 查询命令
#define CMD_WIFI_MODIFY   0x05  // WiFi 修改命令
#define CMD_REBOOT        0x06  // 重启命令
#define CMD_UPDATE_CHECK  0x07  // 更新检查命令
#define CMD_UPDATE_START  0x08  // 更新开始命令
#define CMD_OTA_DATA      0xF0  // OTA 数据传输

// 异步操作特殊返回码
#define ESP_BLE_PROTO_ASYNC  0x100

/**
 * @brief 解析传入的 JSON 数据并执行相应命令
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @param response_buffer 用于存储响应 JSON 的缓冲区
 * @param response_len 响应缓冲区的大小
 * @return esp_err_t 如果响应准备就绪返回 ESP_OK，如果是异步处理返回 ESP_BLE_PROTO_ASYNC，否则返回错误码
 */
esp_err_t ble_proto_parse_json(const uint8_t *data, size_t len, char *response_buffer, size_t response_len);

/**
 * @brief 处理 OTA 数据 (未实现)
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t ble_proto_handle_ota(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
