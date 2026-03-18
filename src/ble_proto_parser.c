#include "ble_proto_parser.h"
#include "ble_gatts_module.h"
#include "wifi_driver.h"
#include "esp_log.h"
#include "cJSON.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "BLE_PROTO";

// 弱定义函数：重启前钩子
// 用户可以在其他地方重新定义此函数以覆盖默认行为
__attribute__((weak)) void pre_restart_hook(void) {
    ESP_LOGI(TAG, "默认重启前钩子 (可覆盖此函数以实现自定义逻辑)");
}

/**
 * @brief 延时重启任务
 * 
 * @param param 任务参数
 */
static void delayed_reboot_task(void *param) {
    ESP_LOGW(TAG, "将在 3 秒后重启...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 执行钩子函数
    pre_restart_hook();
    
    ESP_LOGW(TAG, "现在重启！");
    esp_restart();
    vTaskDelete(NULL);
}

static void create_response_base(cJSON *root, int cmd, const char *status) {
    cJSON_AddNumberToObject(root, "cmd", cmd);
    cJSON_AddStringToObject(root, "status", status);
}

// 辅助函数：发送异步响应
static void send_async_response(cJSON *res) {
    char *json_str = cJSON_PrintUnformatted(res);
    if (json_str) {
        ble_gatts_module_send_notify((uint8_t*)json_str, strlen(json_str));
        cJSON_free(json_str); // cJSON_Print 分配了内存，需要释放
    }
    cJSON_Delete(res);
}

static esp_err_t handle_connect(cJSON *req, cJSON *res) {
    create_response_base(res, CMD_CONNECT, "success");
    
    cJSON *sys_info = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_info, "name", "ESP32-S3-DEVICE");
    cJSON_AddStringToObject(sys_info, "mac", "XX:XX:XX:XX:XX:XX");
    cJSON_AddStringToObject(sys_info, "fw_ver", "1.0.0");
    cJSON_AddStringToObject(sys_info, "hw_ver", "1.0.0");
    cJSON_AddStringToObject(sys_info, "proto_ver", "1.0.0");
    cJSON_AddItemToObject(res, "sys_info", sys_info);

    cJSON *sw_info = cJSON_CreateObject();
    cJSON_AddStringToObject(sw_info, "name", "ESP32-S3-APP");
    cJSON_AddStringToObject(sw_info, "ver", "1.0.0");
    cJSON_AddStringToObject(sw_info, "desc", "BLE Provisioning");
    cJSON_AddStringToObject(sw_info, "date", "2023-10-27");
    cJSON_AddItemToObject(res, "sw_info", sw_info);

    cJSON *state = cJSON_CreateObject();
    cJSON_AddStringToObject(state, "wifi", wifi_driver_is_connected() ? "connected" : "disconnected");
    cJSON_AddStringToObject(state, "dev", "normal");
    cJSON_AddStringToObject(state, "server", "connected");
    cJSON_AddItemToObject(res, "state", state);

    return ESP_OK;
}

// WiFi 扫描异步任务
static void wifi_scan_task(void *param) {
    cJSON *res = cJSON_CreateObject();
    wifi_ap_record_t ap_list[10];
    uint16_t ap_count = 10;
    
    esp_err_t err = wifi_driver_scan(ap_list, &ap_count);
    if (err != ESP_OK) {
        create_response_base(res, CMD_WIFI_SCAN, "fail");
    } else {
        create_response_base(res, CMD_WIFI_SCAN, "success");
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
    }

    send_async_response(res);
    vTaskDelete(NULL);
}

static esp_err_t handle_wifi_scan(cJSON *req, cJSON *res) {
    xTaskCreate(wifi_scan_task, "wifi_scan", 4096, NULL, 5, NULL);
    return ESP_BLE_PROTO_ASYNC;
}

typedef struct {
    char ssid[33];
    char password[65];
} connect_params_t;

// WiFi 连接异步任务
static void wifi_connect_task(void *param) {
    connect_params_t *params = (connect_params_t *)param;
    cJSON *res = cJSON_CreateObject();

    // 先保存配置
    wifi_driver_save_config(params->ssid, params->password);

    // 尝试连接
    esp_err_t err = wifi_driver_connect(params->ssid, params->password);
    
    create_response_base(res, CMD_WIFI_CONNECT, err == ESP_OK ? "success" : "fail");
    cJSON_AddStringToObject(res, "msg", err == ESP_OK ? "connected" : "connect failed");

    send_async_response(res);
    free(params);
    vTaskDelete(NULL);
}

static esp_err_t handle_wifi_connect(cJSON *req, cJSON *res) {
    cJSON *wifi = cJSON_GetObjectItem(req, "wifi");
    if (!wifi) {
        create_response_base(res, CMD_WIFI_CONNECT, "fail");
        return ESP_FAIL;
    }
    
    cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
    cJSON *pwd = cJSON_GetObjectItem(wifi, "pwd");
    
    if (!ssid || !cJSON_IsString(ssid)) {
        create_response_base(res, CMD_WIFI_CONNECT, "fail");
        return ESP_FAIL;
    }

    connect_params_t *params = malloc(sizeof(connect_params_t));
    if (!params) return ESP_ERR_NO_MEM;

    strlcpy(params->ssid, ssid->valuestring, sizeof(params->ssid));
    if (pwd && cJSON_IsString(pwd)) {
        strlcpy(params->password, pwd->valuestring, sizeof(params->password));
    } else {
        params->password[0] = 0;
    }

    xTaskCreate(wifi_connect_task, "wifi_conn", 4096, params, 5, NULL);
    return ESP_BLE_PROTO_ASYNC;
}

static esp_err_t handle_wifi_query(cJSON *req, cJSON *res) {
    wifi_config_info_t configs[MAX_WIFI_CONFIGS];
    int count = 0;
    esp_err_t err = wifi_driver_get_saved_configs(configs, &count);
    
    create_response_base(res, CMD_WIFI_QUERY, "success");
    
    cJSON *saved_wifi = cJSON_CreateObject();
    if (err == ESP_OK && count > 0) {
        cJSON_AddNumberToObject(saved_wifi, "count", count);
        cJSON *list = cJSON_CreateArray();
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "ssid", configs[i].ssid);
            cJSON_AddItemToArray(list, item);
        }
        cJSON_AddItemToObject(saved_wifi, "list", list);
    } else {
        cJSON_AddNumberToObject(saved_wifi, "count", 0);
        cJSON_AddItemToObject(saved_wifi, "list", cJSON_CreateArray());
    }
    cJSON_AddItemToObject(res, "saved_wifi", saved_wifi);

    return ESP_OK;
}

static esp_err_t handle_wifi_modify(cJSON *req, cJSON *res) {
    cJSON *action = cJSON_GetObjectItem(req, "action");
    if (action && cJSON_IsString(action) && strcmp(action->valuestring, "delete") == 0) {
        wifi_driver_delete_config();
    }
    // 返回查询结果
    return handle_wifi_query(req, res);
}

static esp_err_t handle_reboot(cJSON *req, cJSON *res) {
    create_response_base(res, CMD_REBOOT, "success");
    cJSON_AddStringToObject(res, "msg", "rebooting in 3s");
    
    // 创建任务进行延时重启
    xTaskCreate(delayed_reboot_task, "reboot_task", 2048, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t ble_proto_parse_json(const uint8_t *data, size_t len, char *response_buffer, size_t response_len) {
    cJSON *req = cJSON_ParseWithLength((const char *)data, len);
    if (!req) {
        ESP_LOGE(TAG, "JSON 解析错误");
        return ESP_FAIL;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(req, "cmd");
    if (!cmd_item || !cJSON_IsNumber(cmd_item)) {
        cJSON_Delete(req);
        return ESP_FAIL;
    }

    int cmd = cmd_item->valueint;
    cJSON *res = cJSON_CreateObject();
    esp_err_t ret = ESP_OK;
    
    switch (cmd) {
        case CMD_CONNECT:
            ret = handle_connect(req, res);
            break;
        case CMD_WIFI_SCAN:
            ret = handle_wifi_scan(req, res);
            break;
        case CMD_WIFI_CONNECT:
            ret = handle_wifi_connect(req, res);
            break;
        case CMD_WIFI_QUERY:
            ret = handle_wifi_query(req, res);
            break;
        case CMD_WIFI_MODIFY:
            ret = handle_wifi_modify(req, res);
            break;
        case CMD_REBOOT:
            ret = handle_reboot(req, res);
            break;
        default:
            create_response_base(res, cmd, "fail");
            cJSON_AddStringToObject(res, "msg", "unknown command");
            break;
    }

    if (ret == ESP_OK) {
        cJSON_PrintPreallocated(res, response_buffer, response_len, false);
    } else if (ret == ESP_BLE_PROTO_ASYNC) {
        // 异步处理，此处不返回内容，任务会自行处理通知
    }

    cJSON_Delete(req);
    cJSON_Delete(res);

    return ret;
}

esp_err_t ble_proto_handle_ota(const uint8_t *data, size_t len) {
    // OTA 尚未实现
    return ESP_FAIL;
}
