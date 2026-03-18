#include "ble_gatts_module.h"
#include "ble_proto_parser.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include <string.h>

#define GATTS_TAG "BLE_GATTS"

// 定义 UUID
// 服务 UUID
#define SERVICE_UUID        {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}
// 命令特征值 UUID (Write)
#define CHAR_CMD_UUID       {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00}
// 状态特征值 UUID (Notify)
#define CHAR_STATUS_UUID    {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00}

#define GATTS_NUM_HANDLE_TEST_A     8  // 服务句柄数量
#define PROFILE_NUM                 1  // Profile 数量
#define PROFILE_A_APP_ID            0  // Profile A 的 App ID

static uint16_t s_conn_id = 0xffff;
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_char_cmd_handle = 0;
static uint16_t s_char_status_handle = 0;
static char s_device_name[32] = "ESP32-BLE-PROV";

static uint8_t service_uuid128[16] = SERVICE_UUID;
static uint8_t char_cmd_uuid128[16] = CHAR_CMD_UUID;
static uint8_t char_status_uuid128[16] = CHAR_STATUS_UUID;

// 广播参数配置
static esp_ble_adv_params_t test_adv_params = {
    .adv_int_min        = 0x20, // 最小广播间隔
    .adv_int_max        = 0x40, // 最大广播间隔
    .adv_type           = ADV_TYPE_IND, // 通用广播
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

// Profile 实例结构体
static struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
} gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gatts_cb = gatts_profile_a_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

// GAP 事件处理函数
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        // 广播数据设置完成后，开始广播
        esp_ble_gap_start_advertising(&test_adv_params);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        // 广播启动完成事件
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(GATTS_TAG, "广播启动失败");
        }
        break;
    default:
        break;
    }
}

// GATT Profile A 事件处理函数
static void gatts_profile_a_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "注册 APP 事件, status %d, app_id %d", param->reg.status, param->reg.app_id);
        gl_profile_tab[PROFILE_A_APP_ID].service_id.is_primary = true;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.inst_id = 0x00;
        gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.len = ESP_UUID_LEN_128;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].service_id.id.uuid.uuid.uuid128, service_uuid128, 16);

        // 设置设备名称
        esp_ble_gap_set_device_name(s_device_name);
        // 配置广播数据
        esp_ble_gap_config_adv_data(&(esp_ble_adv_data_t){
            .set_scan_rsp = false,
            .include_name = true,
            .include_txpower = true,
            .min_interval = 0x0006,
            .max_interval = 0x0010,
            .appearance = 0x00,
            .manufacturer_len = 0,
            .p_manufacturer_data =  NULL,
            .service_data_len = 0,
            .p_service_data = NULL,
            .service_uuid_len = 16,
            .p_service_uuid = service_uuid128,
            .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
        });
        // 创建服务
        esp_ble_gatts_create_service(gatts_if, &gl_profile_tab[PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE_TEST_A);
        break;
    case ESP_GATTS_READ_EVT: {
        ESP_LOGI(GATTS_TAG, "GATT 读取事件");
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 1;
        rsp.attr_value.value[0] = 0x00;
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGI(GATTS_TAG, "GATT 写入事件, 长度 %d", param->write.len);
        if (!param->write.is_prep) {
            if (param->write.handle == s_char_cmd_handle) {
                // 解析命令
                char response[512]; // 响应缓冲区
                esp_err_t err = ble_proto_parse_json(param->write.value, param->write.len, response, sizeof(response));
                
                if (err == ESP_OK) {
                    // 立即通过 Notify 发送响应
                    ble_gatts_module_send_notify((uint8_t*)response, strlen(response));
                }
                // 如果是 ESP_BLE_PROTO_ASYNC，则在此不做任何操作（任务会处理通知）
            }
        }
        if (param->write.need_rsp){
            esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        }
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(GATTS_TAG, "MTU 交换事件, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "创建服务事件, 状态 %d, 服务句柄 %d", param->create.status, param->create.service_handle);
        gl_profile_tab[PROFILE_A_APP_ID].service_handle = param->create.service_handle;
        
        // 添加命令特征值 (Write)
        esp_bt_uuid_t uuid_cmd = {
            .len = ESP_UUID_LEN_128,
        };
        memcpy(uuid_cmd.uuid.uuid128, char_cmd_uuid128, 16);
        
        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &uuid_cmd,
                               ESP_GATT_PERM_WRITE,
                               ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_WRITE_NR, 
                               NULL, NULL);

        // 添加状态特征值 (Notify)
        esp_bt_uuid_t uuid_status = {
            .len = ESP_UUID_LEN_128,
        };
        memcpy(uuid_status.uuid.uuid128, char_status_uuid128, 16);

        esp_ble_gatts_add_char(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &uuid_status,
                               ESP_GATT_PERM_READ,
                               ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_READ, 
                               NULL, NULL);
        
        // 启动服务
        esp_ble_gatts_start_service(gl_profile_tab[PROFILE_A_APP_ID].service_handle);
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        ESP_LOGI(GATTS_TAG, "添加特征值事件, 属性句柄 %d", param->add_char.attr_handle);
        
        if (memcmp(param->add_char.char_uuid.uuid.uuid128, char_cmd_uuid128, 16) == 0) {
            s_char_cmd_handle = param->add_char.attr_handle;
        } else if (memcmp(param->add_char.char_uuid.uuid.uuid128, char_status_uuid128, 16) == 0) {
            s_char_status_handle = param->add_char.attr_handle;
            // 为 Notify 添加 CCCD 描述符
            esp_bt_uuid_t cccd_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
            };
            esp_ble_gatts_add_char_descr(gl_profile_tab[PROFILE_A_APP_ID].service_handle, &cccd_uuid,
                                         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                         NULL, NULL);
        }
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT: {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        conn_params.latency = 0;
        conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
        ESP_LOGI(GATTS_TAG, "设备连接事件, conn_id %d", param->connect.conn_id);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = param->connect.conn_id;
        s_conn_id = param->connect.conn_id;
        s_gatts_if = gatts_if;
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "设备断开连接事件");
        s_conn_id = 0xffff;
        esp_ble_gap_start_advertising(&test_adv_params);
        break;
    case ESP_GATTS_CONF_EVT:
        if (param->conf.status != ESP_GATT_OK){
            esp_log_buffer_hex(GATTS_TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* 如果是注册事件，存储每个 profile 的 gatts_if */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            return;
        }
    }

    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE，未指定 gatts_if，需调用所有 profile 回调 */
                    gatts_if == gl_profile_tab[idx].gatts_if) {
                if (gl_profile_tab[idx].gatts_cb) {
                    gl_profile_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}

esp_err_t ble_gatts_module_init(const char *device_name)
{
    if (device_name) {
        snprintf(s_device_name, sizeof(s_device_name), "%s", device_name);
    }
    
    // 释放经典蓝牙控制器内存
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        return ret;
    }
    
    ret = esp_bluedroid_init();
    if (ret) {
        return ret;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        return ret;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret){
        return ret;
    }
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret){
        return ret;
    }
    ret = esp_ble_gatts_app_register(PROFILE_A_APP_ID);
    if (ret){
        return ret;
    }
    
    // 设置本地 MTU
    esp_ble_gatt_set_local_mtu(500);
    
    return ESP_OK;
}

esp_err_t ble_gatts_module_deinit(void)
{
    return ESP_OK;
}

esp_err_t ble_gatts_module_start_adv(void)
{
    esp_ble_gap_start_advertising(&test_adv_params);
    return ESP_OK;
}

esp_err_t ble_gatts_module_stop_adv(void)
{
    esp_ble_gap_stop_advertising();
    return ESP_OK;
}

esp_err_t ble_gatts_module_send_notify(const uint8_t *data, size_t len)
{
    if (s_conn_id == 0xffff || s_gatts_if == ESP_GATT_IF_NONE || s_char_status_handle == 0) {
        ESP_LOGW(GATTS_TAG, "无法发送通知：无连接或句柄无效");
        return ESP_FAIL;
    }
    
    return esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_char_status_handle,
                                      len, (uint8_t*)data, false);
}
