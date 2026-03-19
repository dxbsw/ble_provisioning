#include "ble_provisioning.h"
#include "ble_gatts_module.h"
#include "wifi_driver.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BLE_PROV";

static ble_prov_config_t s_config;

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

esp_err_t ble_provisioning_init(const ble_prov_config_t *config)
{
    if (config) {
        s_config = *config;
    } else {
        s_config.device_name = "ESP32-PROV";
    }

    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化 WiFi 驱动
    wifi_driver_init();

    // 启动配网任务
    xTaskCreate(prov_task, "prov_task", 4096, NULL, 5, NULL);

    return ESP_OK;
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

bool ble_provisioning_is_connected(void)
{
    return wifi_driver_is_connected();
}
