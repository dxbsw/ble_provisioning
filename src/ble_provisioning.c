#include "ble_provisioning.h"
#include "ble_gatts_module.h"
#include "wifi_driver.h"
#include "ble_proto_parser.h"
#include "cJSON.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BLE_PROV";

static ble_prov_config_t s_config;

esp_err_t ble_prov_config_set_runtime_defaults(const ble_prov_device_config_t *cfg);

static void ble_prov_apply_defaults(ble_prov_config_t *cfg)
{
    if (!cfg->device_name) {
        cfg->device_name = "ESP32-PROV";
    }
    if (cfg->reconnect_enable == BLE_PROV_OPTION_DEFAULT) {
        cfg->reconnect_enable = BLE_PROV_OPTION_ENABLE;
    }
    if (cfg->reconnect_mode == BLE_PROV_RECONNECT_MODE_DEFAULT) {
        cfg->reconnect_mode = BLE_PROV_RECONNECT_MODE_LINEAR_STEP;
    }
    if (cfg->reconnect_continue_after_max == BLE_PROV_OPTION_DEFAULT) {
        cfg->reconnect_continue_after_max = BLE_PROV_OPTION_ENABLE;
    }
    if (cfg->reconnect_fixed_interval_ms == 0) {
        cfg->reconnect_fixed_interval_ms = 60000;
    }
    if (cfg->reconnect_step_interval_ms == 0) {
        cfg->reconnect_step_interval_ms = 5000;
    }
    if (cfg->reconnect_max_interval_ms == 0) {
        cfg->reconnect_max_interval_ms = 60000;
    }
    if (cfg->reconnect_jitter_ms == 0) {
        cfg->reconnect_jitter_ms = 1000;
    }
}

/**
 * @brief 配网任务
 * 
 * 负责处理 WiFi 连接逻辑和 BLE 配网的启动。
 * 
 * @param param 任务参数（未使用）
 */
static void prov_task(void *param) {
    wifi_config_info_t saved_configs[MAX_WIFI_CONFIGS];
    int config_count = 0;
    esp_err_t err;
    bool connected = false;

    if (s_config.default_wifi_ssid && s_config.default_wifi_ssid[0] != 0) {
        ESP_LOGI(TAG, "正在尝试默认 WiFi: %s", s_config.default_wifi_ssid);
        if (wifi_driver_connect(s_config.default_wifi_ssid, s_config.default_wifi_password) == ESP_OK) {
            connected = true;
        } else {
            ESP_LOGW(TAG, "默认 WiFi 连接失败，继续尝试已保存 WiFi");
        }
    }

    // 1. 检查是否有保存的 WiFi 配置
    err = wifi_driver_get_saved_configs(saved_configs, &config_count);
    if (err == ESP_OK && config_count > 0) {
        ESP_LOGI(TAG, "发现 %d 个已保存的 WiFi 配置", config_count);
        
        // 遍历所有保存的配置，按顺序尝试连接
        for (int i = 0; i < config_count; i++) {
            ESP_LOGI(TAG, "正在尝试第 %d/%d 个 WiFi: %s", i + 1, config_count, saved_configs[i].ssid);
            
            // 每个配置尝试连接 5 次
            for (int retry = 0; retry < 5; retry++) {
                ESP_LOGI(TAG, "  尝试 %d/5...", retry + 1);
                if (wifi_driver_connect(saved_configs[i].ssid, saved_configs[i].password) == ESP_OK) {
                    connected = true;
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            
            if (connected) break;
            
            ESP_LOGW(TAG, "连接 %s 失败，尝试下一个...", saved_configs[i].ssid);
        }

        if (connected) {
            ESP_LOGI(TAG, "成功连接到 WiFi。配网流程结束。");
#if BLE_KEEP_ALIVE_AFTER_WIFI_CONNECTED
            ESP_LOGI(TAG, "BLE 保持连接已启用。正在启动 BLE...");
            ble_gatts_module_init(s_config.device_name);
#else
            ESP_LOGI(TAG, "BLE 保持连接已禁用。BLE 将不会启动（或如果正在运行则停止）。");
            // 如果 BLE 已经在运行，则停止它。在此流程中它尚未启动，但为了安全起见注释掉：
            // ble_provisioning_stop(); 
#endif
            vTaskDelete(NULL);
            return;
        } else {
            ESP_LOGW(TAG, "所有保存的 WiFi 均无法连接。");
        }
    } else {
        ESP_LOGI(TAG, "未发现保存的 WiFi 配置。");
    }

    // 2. 如果未连接，启动 BLE 配网模式
    ESP_LOGI(TAG, "正在启动 BLE 配网模式...");
    ble_gatts_module_init(s_config.device_name);
    
    // 任务可以删除自身，BLE 将在自己的任务/事件中运行。
    vTaskDelete(NULL);
}

static void wifi_scan_notify_task(void *param) {
    cJSON *res = cJSON_CreateObject();
    wifi_ap_record_t ap_list[10];
    uint16_t ap_count = 10;
    if (wifi_driver_scan(ap_list, &ap_count) != ESP_OK) {
        cJSON_AddNumberToObject(res, "cmd", CMD_WIFI_SCAN);
        cJSON_AddStringToObject(res, "status", "fail");
        char *json_str = cJSON_PrintUnformatted(res);
        if (json_str) {
            ble_gatts_module_send_notify((uint8_t*)json_str, strlen(json_str));
            cJSON_free(json_str);
        }
        cJSON_Delete(res);
        vTaskDelete(NULL);
        return;
    }
    cJSON_AddNumberToObject(res, "cmd", CMD_WIFI_SCAN);
    cJSON_AddStringToObject(res, "status", "success");
    cJSON *scan_res = cJSON_CreateObject();
    cJSON_AddNumberToObject(scan_res, "count", ap_count);
    cJSON *list = cJSON_CreateArray();
    for (int i = 0; i < ap_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ssid", (char *)ap_list[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", ap_list[i].rssi);
        cJSON_AddItemToArray(list, item);
    }
    cJSON_AddItemToObject(scan_res, "list", list);
    cJSON_AddItemToObject(res, "scan_res", scan_res);
    char *json_str = cJSON_PrintUnformatted(res);
    if (json_str) {
        ble_gatts_module_send_notify((uint8_t*)json_str, strlen(json_str));
        cJSON_free(json_str);
    }
    cJSON_Delete(res);
    vTaskDelete(NULL);
}

esp_err_t ble_provisioning_init(const ble_prov_config_t *config, bool init_nvs)
{
    memset(&s_config, 0, sizeof(s_config));
    if (config) {
        s_config = *config;
    }
    ble_prov_apply_defaults(&s_config);
    ble_prov_config_set_runtime_defaults(&s_config.device_info);

    if (init_nvs) {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ret = nvs_flash_erase();
            if (ret != ESP_OK) {
                return ret;
            }
            ret = nvs_flash_init();
        }
        if (ret != ESP_OK) {
            return ret;
        }
    }

    // 初始化 WiFi 驱动
    esp_err_t ret = wifi_driver_init();
    if (ret != ESP_OK) {
        return ret;
    }

    wifi_driver_reconnect_config_t reconn_cfg = {0};
    reconn_cfg.enable = (s_config.reconnect_enable == BLE_PROV_OPTION_ENABLE)
                            ? WIFI_DRIVER_OPTION_ENABLE
                            : (s_config.reconnect_enable == BLE_PROV_OPTION_DISABLE) ? WIFI_DRIVER_OPTION_DISABLE
                                                                                     : WIFI_DRIVER_OPTION_DEFAULT;
    reconn_cfg.mode = (s_config.reconnect_mode == BLE_PROV_RECONNECT_MODE_FIXED)
                          ? WIFI_DRIVER_RECONNECT_MODE_FIXED
                          : (s_config.reconnect_mode == BLE_PROV_RECONNECT_MODE_LINEAR_STEP)
                                ? WIFI_DRIVER_RECONNECT_MODE_LINEAR_STEP
                                : WIFI_DRIVER_RECONNECT_MODE_DEFAULT;
    reconn_cfg.continue_after_max =
        (s_config.reconnect_continue_after_max == BLE_PROV_OPTION_ENABLE)
            ? WIFI_DRIVER_OPTION_ENABLE
            : (s_config.reconnect_continue_after_max == BLE_PROV_OPTION_DISABLE) ? WIFI_DRIVER_OPTION_DISABLE
                                                                                 : WIFI_DRIVER_OPTION_DEFAULT;
    reconn_cfg.max_attempts = s_config.reconnect_max_attempts;
    reconn_cfg.fixed_interval_ms = s_config.reconnect_fixed_interval_ms;
    reconn_cfg.step_interval_ms = s_config.reconnect_step_interval_ms;
    reconn_cfg.max_interval_ms = s_config.reconnect_max_interval_ms;
    reconn_cfg.jitter_ms = s_config.reconnect_jitter_ms;
    wifi_driver_set_reconnect_config(&reconn_cfg);

    // 启动配网任务
    if (xTaskCreate(prov_task, "prov_task", 4096, NULL, 5, NULL) != pdPASS) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ble_provisioning_set_custom_handler(ble_prov_custom_proto_handler_t handler, void *user_ctx)
{
    return ble_gatts_module_set_custom_handler((ble_gatts_custom_proto_handler_t)handler, user_ctx);
}

esp_err_t ble_provisioning_send_notify(const uint8_t *data, size_t len)
{
    return ble_gatts_module_send_notify(data, len);
}

esp_err_t ble_provisioning_start(void)
{
    // 检查是否已经初始化/启动？
    // 理想情况下应检查状态。目前直接初始化。
    return ble_gatts_module_init(s_config.device_name);
}

esp_err_t ble_provisioning_stop(void)
{
    return ble_gatts_module_stop_adv();
}

esp_err_t ble_provisioning_set_reconnect_enabled(bool enabled)
{
    wifi_driver_set_reconnect_enabled(enabled);
    return ESP_OK;
}

bool ble_provisioning_is_connected(void)
{
    return wifi_driver_is_connected();
}

esp_err_t ble_provisioning_scan_and_notify(void)
{
    if (xTaskCreate(wifi_scan_notify_task, "wifi_scan_notify", 4096, NULL, 5, NULL) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
