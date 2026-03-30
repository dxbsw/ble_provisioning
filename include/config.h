#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connect 响应中 sys_info 字段的默认值
 */
/** 设备名称 */
#define BLE_PROV_DEFAULT_SYS_NAME "ESP32-S3-DEVICE"
/** 固件版本 */
#define BLE_PROV_DEFAULT_FW_VER "1.0.0"
/** 硬件版本 */
#define BLE_PROV_DEFAULT_HW_VER "1.0.0"
/** 协议版本 */
#define BLE_PROV_DEFAULT_PROTO_VER "1.0.0"

/**
 * @brief Connect 响应中 sw_info 字段的默认值
 */
/** 软件名称 */
#define BLE_PROV_DEFAULT_SW_NAME "ESP32-S3-APP"
/** 软件版本 */
#define BLE_PROV_DEFAULT_SW_VER "1.0.0"
/** 软件描述 */
#define BLE_PROV_DEFAULT_SW_DESC "BLE Provisioning"
/** 软件发布日期 */
#define BLE_PROV_DEFAULT_SW_DATE "2023-10-27"

/**
 * @brief Connect 响应中 state 字段的默认值
 */
/** 设备状态 */
#define BLE_PROV_DEFAULT_STATE_DEV "normal"
/** 服务端状态 */
#define BLE_PROV_DEFAULT_STATE_SERVER "connected"

/**
 * @brief BLE 配网组件设备信息配置
 *
 * 用于 Connect 响应中的 sys_info / sw_info / state 字段。
 */
typedef struct {
    char sys_name[32];
    char mac[18];
    char fw_ver[16];
    char hw_ver[16];
    char proto_ver[16];

    char sw_name[32];
    char sw_ver[16];
    char sw_desc[64];
    char sw_date[16];

    char state_dev[16];
    char state_server[16];
} ble_prov_device_config_t;

/**
 * @brief 读取 BLE 配网组件设备信息配置
 *
 * 读取顺序：
 * 1) 先加载本文件中定义的默认值；
 * 2) 如果 NVS 中存在已保存配置，则覆盖默认值；
 * 3) 若 mac 仍为占位符/空，则尝试读取芯片真实 MAC 填充。
 *
 * @param[out] out_cfg 输出配置结构体指针
 * @return esp_err_t 参数非法返回 ESP_ERR_INVALID_ARG，其余情况返回 ESP_OK
 */
esp_err_t ble_prov_config_get(ble_prov_device_config_t *out_cfg);

/**
 * @brief 保存 BLE 配网组件设备信息配置到 NVS
 *
 * 保存位置：namespace="ble_prov"，key="dev_cfg"（blob）。
 *
 * @param[in] cfg 待保存的配置结构体指针
 * @return esp_err_t 参数非法返回 ESP_ERR_INVALID_ARG，其余返回 NVS 相关错误码或 ESP_OK
 */
esp_err_t ble_prov_config_set(const ble_prov_device_config_t *cfg);

#ifdef __cplusplus
}
#endif
