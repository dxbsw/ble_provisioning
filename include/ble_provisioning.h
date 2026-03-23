#pragma once

#include <stddef.h>
#include <stdint.h>
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
 * @brief 自定义 BLE 协议处理回调
 *
 * 当组件内置协议无法处理（例如非 JSON、JSON 解析失败、或 cmd 不支持）时，会调用该回调。
 *
 * @param[in]  user_ctx       用户上下文指针（由 ble_provisioning_set_custom_handler 设置）
 * @param[in]  data           主机写入到 Command 特征值的原始数据
 * @param[in]  len            data 长度
 * @param[out] response       同步响应缓冲区（若返回 ESP_OK，则会通过 Notify 发送 response）
 * @param[in]  response_len   response 缓冲区大小
 *
 * @return
 *      - ESP_OK: response 已填充，组件将通过 Notify 发送
 *      - BLE_PROV_PROTO_ASYNC: 已异步处理（回调内部自行调用 ble_provisioning_send_notify 发送 Notify）
 *      - 其他: 未处理/失败，组件将按默认行为返回错误响应
 */
typedef esp_err_t (*ble_prov_custom_proto_handler_t)(void *user_ctx,
                                                     const uint8_t *data,
                                                     size_t len,
                                                     char *response,
                                                     size_t response_len);

/**
 * @brief 自定义协议处理的异步返回码
 */
#define BLE_PROV_PROTO_ASYNC  0x100

/**
 * @brief 初始化 BLE 配网组件
 * 
 * 该函数将执行以下操作：
 * 1. 可选初始化 NVS（由 init_nvs 控制）
 * 2. 检查 NVS 中是否保存了 WiFi 凭据
 * 3. 如果存在保存的 WiFi，尝试连接（重试 5 次，循环 3 轮）
 * 4. 如果连接失败或没有保存的 WiFi，启动 BLE 配网模式
 * 
 * @param config 配置结构体指针
 * @param init_nvs 是否在组件内部初始化 NVS（true: 调用 nvs_flash_init；false: 跳过，要求外部自行初始化）
 * @return
 *      - ESP_OK: 成功
 *      - ESP_FAIL: 创建任务失败
 *      - 其他: ESP-IDF 返回的错误码
 */
esp_err_t ble_provisioning_init(const ble_prov_config_t *config, bool init_nvs);

/**
 * @brief 注册自定义 BLE 协议处理回调
 *
 * @param[in] handler  回调函数指针，传 NULL 表示清除
 * @param[in] user_ctx 用户上下文指针（会透传到回调）
 *
 * @return
 *      - ESP_OK: 成功
 */
esp_err_t ble_provisioning_set_custom_handler(ble_prov_custom_proto_handler_t handler, void *user_ctx);

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

/**
 * @brief 查询当前 WiFi 是否已连接
 *
 * @return true 已连接，false 未连接
 */
bool ble_provisioning_is_connected(void);

/**
 * @brief 触发一次 WiFi 扫描并通过 BLE Notify 上报扫描结果
 *
 * @return
 *      - ESP_OK: 成功创建扫描任务
 *      - ESP_FAIL: 创建任务失败
 */
esp_err_t ble_provisioning_scan_and_notify(void);

/**
 * @brief 通过 BLE Notify 发送数据给主机
 *
 * 用于自定义协议回调里主动上报数据。
 *
 * @param[in] data 数据指针
 * @param[in] len  数据长度
 *
 * @return
 *      - ESP_OK: 成功
 *      - 其他: BLE 未连接/句柄无效等错误
 */
esp_err_t ble_provisioning_send_notify(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
