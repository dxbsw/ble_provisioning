#include "wifi_driver.h"
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "WIFI_DRIVER";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static bool s_is_connected = false;

// 事件处理回调函数
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // 在这里不自动连接，由 wifi_driver_connect 手动触发
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_connected = false;
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "获取到 IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_is_connected = true;
        if (s_wifi_event_group) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

esp_err_t wifi_driver_init(void)
{
    static bool initialized = false;
    if (initialized) return ESP_OK;

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    initialized = true;
    ESP_LOGI(TAG, "WiFi 驱动初始化完成。");
    return ESP_OK;
}

esp_err_t wifi_driver_scan(wifi_ap_record_t *ap_list, uint16_t *ap_count)
{
    uint16_t number = *ap_count;
    uint16_t ap_num = 0;
    
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_num));
    
    if (ap_num > number) {
        ap_num = number;
    }
    
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_list));
    *ap_count = ap_num;
    return ESP_OK;
}

esp_err_t wifi_driver_connect(const char *ssid, const char *password)
{
    if (!ssid) return ESP_ERR_INVALID_ARG;

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (password) {
        strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    }

    ESP_LOGI(TAG, "正在连接到 %s...", ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(10000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "已连接到 AP SSID:%s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "连接 SSID:%s 失败", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "意外事件");
        return ESP_FAIL;
    }
}

typedef struct {
    uint8_t count;
    wifi_config_info_t configs[MAX_WIFI_CONFIGS];
} my_wifi_storage_t;

static const char *NVS_KEY_STORAGE = "wifi_store";

esp_err_t wifi_driver_save_config(const char *ssid, const char *password)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    my_wifi_storage_t storage = {0};
    size_t required_size = sizeof(my_wifi_storage_t);
    
    // 1. 读取现有配置
    err = nvs_get_blob(my_handle, NVS_KEY_STORAGE, &storage, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(my_handle);
        return err;
    }
    
    // 如果是从旧版本迁移（只有 ssid/password 键），这里先忽略，假定用户接受覆盖或已迁移
    // 实际上 get_saved_configs 会负责迁移，所以 save 时直接覆盖或新建即可

    // 2. 检查 SSID 是否已存在
    int existing_index = -1;
    for (int i = 0; i < storage.count; i++) {
        if (strcmp(storage.configs[i].ssid, ssid) == 0) {
            existing_index = i;
            break;
        }
    }

    // 3. 如果存在，将其移除（后续统一插入到头部）
    if (existing_index >= 0) {
        for (int i = existing_index; i < storage.count - 1; i++) {
            storage.configs[i] = storage.configs[i+1];
        }
        storage.count--;
    }

    // 4. 移动现有配置（向后移位）
    // 如果已满，丢弃最后一个（索引 MAX-1）
    int move_count = storage.count;
    if (move_count >= MAX_WIFI_CONFIGS) {
        move_count = MAX_WIFI_CONFIGS - 1;
    }
    
    for (int i = move_count; i > 0; i--) {
        storage.configs[i] = storage.configs[i-1];
    }

    // 5. 插入新配置到头部
    strncpy(storage.configs[0].ssid, ssid, sizeof(storage.configs[0].ssid) - 1);
    storage.configs[0].ssid[sizeof(storage.configs[0].ssid) - 1] = 0;
    
    if (password) {
        strncpy(storage.configs[0].password, password, sizeof(storage.configs[0].password) - 1);
        storage.configs[0].password[sizeof(storage.configs[0].password) - 1] = 0;
    } else {
        storage.configs[0].password[0] = 0;
    }

    // 6. 更新数量
    if (storage.count < MAX_WIFI_CONFIGS) {
        storage.count++;
    }

    // 7. 保存到 NVS
    err = nvs_set_blob(my_handle, NVS_KEY_STORAGE, &storage, sizeof(my_wifi_storage_t));
    if (err != ESP_OK) {
        nvs_close(my_handle);
        return err;
    }

    err = nvs_commit(my_handle);
    nvs_close(my_handle);
    return err;
}

esp_err_t wifi_driver_get_saved_configs(wifi_config_info_t *configs, int *count)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    my_wifi_storage_t storage = {0};
    size_t required_size = sizeof(my_wifi_storage_t);
    
    // 1. 尝试读取 Blob
    err = nvs_get_blob(my_handle, NVS_KEY_STORAGE, &storage, &required_size);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // 2. 尝试迁移旧版本数据 (key: ssid, password)
        size_t ssid_len = sizeof(storage.configs[0].ssid);
        err = nvs_get_str(my_handle, "ssid", storage.configs[0].ssid, &ssid_len);
        
        if (err == ESP_OK) {
            // 发现旧数据，读取密码
            size_t pass_len = sizeof(storage.configs[0].password);
            err = nvs_get_str(my_handle, "password", storage.configs[0].password, &pass_len);
            if (err == ESP_ERR_NVS_NOT_FOUND) {
                storage.configs[0].password[0] = 0;
            }
            
            storage.count = 1;
            
            // 保存为新格式
            nvs_set_blob(my_handle, NVS_KEY_STORAGE, &storage, sizeof(my_wifi_storage_t));
            // 删除旧键
            nvs_erase_key(my_handle, "ssid");
            nvs_erase_key(my_handle, "password");
            nvs_commit(my_handle);
            
            ESP_LOGI(TAG, "已迁移旧 WiFi 配置到新存储格式");
        } else {
            *count = 0;
            nvs_close(my_handle);
            return ESP_OK; // 无数据
        }
    } else if (err != ESP_OK) {
        nvs_close(my_handle);
        return err;
    }

    // 3. 返回数据
    *count = storage.count;
    for (int i = 0; i < storage.count; i++) {
        configs[i] = storage.configs[i];
    }

    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t wifi_driver_delete_config(void)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;

    // 删除存储 Blob
    err = nvs_erase_key(my_handle, NVS_KEY_STORAGE);
    
    // 同时尝试删除旧键，以防万一
    nvs_erase_key(my_handle, "ssid");
    nvs_erase_key(my_handle, "password");

    nvs_commit(my_handle);
    nvs_close(my_handle);
    return err;
}

bool wifi_driver_is_connected(void)
{
    return s_is_connected;
}
