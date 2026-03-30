#include "config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_mac.h"
#include "nvs.h"

static const char *NVS_NAMESPACE = "ble_prov";
static const char *NVS_KEY_DEVICE_CFG = "dev_cfg";
static ble_prov_device_config_t s_runtime_defaults;

static void ble_prov_copy_if_not_empty(char *dst, size_t dst_size, const char *src)
{
    if (dst == NULL || dst_size == 0 || src == NULL || src[0] == 0) return;
    strlcpy(dst, src, dst_size);
}

static void ble_prov_config_set_defaults(ble_prov_device_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    strlcpy(cfg->sys_name, BLE_PROV_DEFAULT_SYS_NAME, sizeof(cfg->sys_name));
    strlcpy(cfg->mac, "XX:XX:XX:XX:XX:XX", sizeof(cfg->mac));
    strlcpy(cfg->fw_ver, BLE_PROV_DEFAULT_FW_VER, sizeof(cfg->fw_ver));
    strlcpy(cfg->hw_ver, BLE_PROV_DEFAULT_HW_VER, sizeof(cfg->hw_ver));
    strlcpy(cfg->proto_ver, BLE_PROV_DEFAULT_PROTO_VER, sizeof(cfg->proto_ver));

    strlcpy(cfg->sw_name, BLE_PROV_DEFAULT_SW_NAME, sizeof(cfg->sw_name));
    strlcpy(cfg->sw_ver, BLE_PROV_DEFAULT_SW_VER, sizeof(cfg->sw_ver));
    strlcpy(cfg->sw_desc, BLE_PROV_DEFAULT_SW_DESC, sizeof(cfg->sw_desc));
    strlcpy(cfg->sw_date, BLE_PROV_DEFAULT_SW_DATE, sizeof(cfg->sw_date));

    strlcpy(cfg->state_dev, BLE_PROV_DEFAULT_STATE_DEV, sizeof(cfg->state_dev));
    strlcpy(cfg->state_server, BLE_PROV_DEFAULT_STATE_SERVER, sizeof(cfg->state_server));
}

static void ble_prov_config_apply_runtime_defaults(ble_prov_device_config_t *cfg)
{
    ble_prov_copy_if_not_empty(cfg->sys_name, sizeof(cfg->sys_name), s_runtime_defaults.sys_name);
    ble_prov_copy_if_not_empty(cfg->mac, sizeof(cfg->mac), s_runtime_defaults.mac);
    ble_prov_copy_if_not_empty(cfg->fw_ver, sizeof(cfg->fw_ver), s_runtime_defaults.fw_ver);
    ble_prov_copy_if_not_empty(cfg->hw_ver, sizeof(cfg->hw_ver), s_runtime_defaults.hw_ver);
    ble_prov_copy_if_not_empty(cfg->proto_ver, sizeof(cfg->proto_ver), s_runtime_defaults.proto_ver);
    ble_prov_copy_if_not_empty(cfg->sw_name, sizeof(cfg->sw_name), s_runtime_defaults.sw_name);
    ble_prov_copy_if_not_empty(cfg->sw_ver, sizeof(cfg->sw_ver), s_runtime_defaults.sw_ver);
    ble_prov_copy_if_not_empty(cfg->sw_desc, sizeof(cfg->sw_desc), s_runtime_defaults.sw_desc);
    ble_prov_copy_if_not_empty(cfg->sw_date, sizeof(cfg->sw_date), s_runtime_defaults.sw_date);
    ble_prov_copy_if_not_empty(cfg->state_dev, sizeof(cfg->state_dev), s_runtime_defaults.state_dev);
    ble_prov_copy_if_not_empty(cfg->state_server, sizeof(cfg->state_server), s_runtime_defaults.state_server);
}

static bool ble_prov_mac_is_placeholder(const char *mac)
{
    if (mac == NULL || mac[0] == 0) return true;
    return strcmp(mac, "XX:XX:XX:XX:XX:XX") == 0;
}

static void ble_prov_fill_mac_if_needed(ble_prov_device_config_t *cfg)
{
    if (!ble_prov_mac_is_placeholder(cfg->mac)) return;

    uint8_t mac_bytes[6] = {0};
    if (esp_read_mac(mac_bytes, ESP_MAC_WIFI_STA) != ESP_OK) return;

    snprintf(cfg->mac, sizeof(cfg->mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_bytes[0], mac_bytes[1], mac_bytes[2], mac_bytes[3], mac_bytes[4], mac_bytes[5]);
}

esp_err_t ble_prov_config_get(ble_prov_device_config_t *out_cfg)
{
    if (out_cfg == NULL) return ESP_ERR_INVALID_ARG;

    ble_prov_config_set_defaults(out_cfg);
    ble_prov_config_apply_runtime_defaults(out_cfg);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ble_prov_fill_mac_if_needed(out_cfg);
        return ESP_OK;
    }

    ble_prov_device_config_t stored = {0};
    size_t required_size = sizeof(stored);
    err = nvs_get_blob(handle, NVS_KEY_DEVICE_CFG, &stored, &required_size);
    nvs_close(handle);

    if (err == ESP_OK && required_size == sizeof(stored)) {
        *out_cfg = stored;
    }

    ble_prov_fill_mac_if_needed(out_cfg);
    return ESP_OK;
}

esp_err_t ble_prov_config_set_runtime_defaults(const ble_prov_device_config_t *cfg)
{
    memset(&s_runtime_defaults, 0, sizeof(s_runtime_defaults));
    if (cfg != NULL) {
        s_runtime_defaults = *cfg;
        s_runtime_defaults.sys_name[sizeof(s_runtime_defaults.sys_name) - 1] = 0;
        s_runtime_defaults.mac[sizeof(s_runtime_defaults.mac) - 1] = 0;
        s_runtime_defaults.fw_ver[sizeof(s_runtime_defaults.fw_ver) - 1] = 0;
        s_runtime_defaults.hw_ver[sizeof(s_runtime_defaults.hw_ver) - 1] = 0;
        s_runtime_defaults.proto_ver[sizeof(s_runtime_defaults.proto_ver) - 1] = 0;
        s_runtime_defaults.sw_name[sizeof(s_runtime_defaults.sw_name) - 1] = 0;
        s_runtime_defaults.sw_ver[sizeof(s_runtime_defaults.sw_ver) - 1] = 0;
        s_runtime_defaults.sw_desc[sizeof(s_runtime_defaults.sw_desc) - 1] = 0;
        s_runtime_defaults.sw_date[sizeof(s_runtime_defaults.sw_date) - 1] = 0;
        s_runtime_defaults.state_dev[sizeof(s_runtime_defaults.state_dev) - 1] = 0;
        s_runtime_defaults.state_server[sizeof(s_runtime_defaults.state_server) - 1] = 0;
    }
    return ESP_OK;
}

esp_err_t ble_prov_config_set(const ble_prov_device_config_t *cfg)
{
    if (cfg == NULL) return ESP_ERR_INVALID_ARG;

    ble_prov_device_config_t to_store = *cfg;
    to_store.sys_name[sizeof(to_store.sys_name) - 1] = 0;
    to_store.mac[sizeof(to_store.mac) - 1] = 0;
    to_store.fw_ver[sizeof(to_store.fw_ver) - 1] = 0;
    to_store.hw_ver[sizeof(to_store.hw_ver) - 1] = 0;
    to_store.proto_ver[sizeof(to_store.proto_ver) - 1] = 0;
    to_store.sw_name[sizeof(to_store.sw_name) - 1] = 0;
    to_store.sw_ver[sizeof(to_store.sw_ver) - 1] = 0;
    to_store.sw_desc[sizeof(to_store.sw_desc) - 1] = 0;
    to_store.sw_date[sizeof(to_store.sw_date) - 1] = 0;
    to_store.state_dev[sizeof(to_store.state_dev) - 1] = 0;
    to_store.state_server[sizeof(to_store.state_server) - 1] = 0;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(handle, NVS_KEY_DEVICE_CFG, &to_store, sizeof(to_store));
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}
