# BLE 配网组件

这是一个用于 ESP32-S3 的 BLE (Bluetooth Low Energy) 配网组件，基于 ESP-IDF 开发。它允许用户通过蓝牙连接配置设备的 WiFi 网络，并支持简单的指令交互和状态查询。

## 功能特性

*   **双模式配网**：优先尝试连接已保存的 WiFi，失败后自动进入 BLE 配网模式。
*   **WiFi 连接管理**：
    *   自动重连机制（每轮 5 次，共 3 轮）。
    *   支持 WiFi 凭据的 NVS 存储、查询和删除。
*   **BLE 交互**：
    *   提供标准的 GATT Server 服务。
    *   基于 JSON 的指令协议（连接、扫描、配网、重启等）。
    *   支持状态通知（Notify）。
    *   可配置 WiFi 连接后是否保持 BLE 开启。
*   **扩展性**：
    *   提供 Python 上位机测试工具。
    *   预留重启前的 Hook 函数，方便二次开发。
    *   支持注册自定义协议处理回调（外部扩展自己的通信协议）。
    *   BLE 连接建立后自动触发一次 WiFi 扫描，并通过 Notify 上报扫描结果。
    *   Connect 响应中的 `sys_info / sw_info / state` 字段支持外部传入默认值，并可保存到 NVS 持久化。

## 目录结构

```
ble_provisioning/
├── include/
│   ├── ble_provisioning.h   # 对外公共接口
│   ├── config.h             # 设备信息配置（默认值 + NVS）
│   └── wifi_driver.h        # WiFi 驱动对外接口
├── src/
│   ├── ble_provisioning.c   # 核心配网逻辑
│   ├── ble_gatts_module.c   # BLE GATT Server 实现
│   ├── ble_gatts_module.h   # BLE 模块内部头文件
│   ├── ble_proto_parser.c   # 协议解析与指令处理
│   ├── ble_proto_parser.h   # 协议解析器头文件
│   ├── config.c             # 设备信息配置（NVS 读写实现）
│   └── wifi_driver.c        # WiFi 驱动封装
├── python_tool/             # Python 上位机测试工具
│   ├── ble_test.py          # 测试脚本
│   ├── requirements.txt     # 依赖库
│   └── README.md            # 上位机使用说明
├── CMakeLists.txt           # 构建脚本
├── PROTOCOL.md              # 通信协议文档
└── README.md                # 组件说明文档
```

## 作为依赖引入

### 方式 A：从 Git 仓库（推荐用于开发）

在项目 `main/idf_component.yml`（或任意需要依赖它的组件清单）中添加：

```yaml
dependencies:
  ble_provisioning:
    git: "https://github.com/dxbsw/ble_provisioning.git"
    version: "v1.0.5"
```

### 方式 B：从组件中心（发布后使用）

```yaml
dependencies:
  dxbsw/ble_provisioning:
    version: "^1.0.5"
```

## 依赖 (Dependencies)

本组件依赖以下 ESP-IDF 组件（构建系统会自动处理）：
*   `nvs_flash`: 用于存储 WiFi 凭据。
*   `bt`: 蓝牙协议栈 (Bluedroid)。
*   `esp_wifi`: WiFi 驱动。
*   `json`: cJSON 库，用于协议解析。

## 配置与使用

### 1. 必需的 SDK 配置 (`sdkconfig`)

在使用本组件前，必须在 `sdkconfig` 中开启蓝牙支持。请运行 `idf.py menuconfig` 并确保以下选项已启用：

*   **Component config -> Bluetooth**:
    *   [x] `Bluetooth`
    *   [x] `Bluedroid Options` -> `Include BLE 4.2 features` (或者确保 `CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y`)
        *   *注意：本组件使用了传统的 BLE 广播 API，因此必须开启 BLE 4.2 特性支持。*
*   **Partition Table**:
    *   建议使用自定义分区表，确保 App 分区足够大（蓝牙协议栈占用较大空间）。
    *   建议 `factory` 分区至少 2MB，或使用 `partitions.csv` 配置更大的 `ota_0` 分区。

### 2. 代码集成

在您的 `main` 函数中初始化组件：

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ble_provisioning.h"
#include "wifi_driver.h"

void app_main(void)
{
    // 定义配置
    ble_prov_config_t config = {
        .device_name = "ESP32-S3-TEXT", // 自定义蓝牙广播名称
        .device_info = {
            .sys_name = "ESP32-S3-DEVICE",
            .fw_ver = "1.0.1",
            .hw_ver = "1.0.0",
            .proto_ver = "1.0.0",
            .sw_name = "ESP32-S3-APP",
            .sw_ver = "1.0.1",
            .sw_desc = "BLE Provisioning",
            .sw_date = __DATE__,
            .state_dev = "normal",
            .state_server = "connected",
        },
    };

    // 初始化 BLE 配网组件
    // 这会自动初始化 NVS、WiFi，并根据情况启动连接或配网
    esp_err_t err = ble_provisioning_init(&config, true);
    
    if (err != ESP_OK) {
        // 错误处理
    }

    // 等待 wifi 连接完成（可选）
    while (!wifi_driver_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

`init_nvs` 参数说明：
*   传 `true`：由组件内部调用 `nvs_flash_init()`（适合大多数工程）。
*   传 `false`：组件不初始化 NVS，要求外部已完成 NVS 初始化（适合你在 app_main 里统一初始化的工程）。

### 3. 自定义配置

#### 3.1 WiFi 连接后保持 BLE

在 `include/ble_provisioning.h` 中，可以通过修改宏定义来决定 WiFi 连接成功后是否保持蓝牙开启：

```c
// 1: 保持开启（默认），允许后续通过蓝牙控制
// 0: 关闭蓝牙以省电
#define BLE_KEEP_ALIVE_AFTER_WIFI_CONNECTED 1
```

#### 3.2 重启钩子 (Hook)

组件在执行重启指令前预留了 Hook 函数。您可以在自己的代码（如 `main.c`）中重新实现该函数，以执行自定义的清理工作（如保存数据、关闭外设）：

```c
// 在您的代码中覆盖此函数
void pre_restart_hook(void) {
    ESP_LOGI("APP", "设备即将重启，正在保存数据...");
    // save_user_data();
    // disable_peripherals();
}
```

#### 3.3 外部传入设备信息默认值

`ble_prov_config_t` 新增了 `device_info` 字段，用于配置 BLE Connect 响应中的 `sys_info / sw_info / state` 内容。  
这些值会在 `ble_provisioning_init()` 时作为运行时默认值注入；如果某个字段留空，组件会继续回退到 `include/config.h` 中定义的默认值。

对应字段定义见 [config.h](file:///d:/Project/sundries/BLE_TEST/components/ble_provisioning/include/config.h#L46-L60)：

```c
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
```

示例：

```c
ble_prov_config_t config = {
    .device_name = "ESP32-S3-TEXT",
    .device_info = {
        .sys_name = "ESP32-S3-DEVICE",
        .fw_ver = "1.0.1",
        .hw_ver = "1.0.0",
        .proto_ver = "1.0.0",
        .sw_name = "ESP32-S3-APP",
        .sw_ver = "1.0.1",
        .sw_desc = "BLE Provisioning",
        .sw_date = __DATE__,
        .state_dev = "normal",
        .state_server = "connected",
    },
};

ble_provisioning_init(&config, true);
```

补充说明：
*   `mac` 可不传。若未设置或仍为占位符，组件会自动读取芯片真实 MAC 填充。
*   外部传入的是“默认值”，当 NVS 中已经保存过配置时，读取结果会优先使用 NVS 中的值。
*   字符串长度需满足结构体字段限制，超长内容会被截断。

#### 3.4 保存设备信息到 NVS

如果应用层希望把新的设备信息持久化到 NVS，可直接调用 `ble_prov_config_set()`。保存位置为：

*   namespace: `ble_prov`
*   key: `dev_cfg`

示例：

```c
#include "config.h"

void save_device_info(void)
{
    ble_prov_device_config_t cfg = {
        .sys_name = "ESP32-S3-DEVICE",
        .fw_ver = "1.0.2",
        .hw_ver = "1.0.0",
        .proto_ver = "1.0.0",
        .sw_name = "ESP32-S3-APP",
        .sw_ver = "1.0.2",
        .sw_desc = "BLE Provisioning",
        .sw_date = __DATE__,
        .state_dev = "normal",
        .state_server = "connected",
    };

    ESP_ERROR_CHECK(ble_prov_config_set(&cfg));
}
```

如需读取当前生效配置，可调用 `ble_prov_config_get()`：

```c
ble_prov_device_config_t current_cfg;
ESP_ERROR_CHECK(ble_prov_config_get(&current_cfg));
```

## 新功能使用示例

### 1) BLE 连接后自动上报 WiFi 扫描结果

设备 BLE 连接建立后，会自动触发一次 WiFi 扫描，并通过 Notify 上报（格式与 `CMD_WIFI_SCAN` 的响应一致，详见 `PROTOCOL.md`）。

如需手动触发，也可以在应用层调用：

```c
ble_provisioning_scan_and_notify();
```

### 2) 自定义协议钩子：JSON Echo（手机发什么，设备回什么）

你可以在外部注册自定义协议回调，拦截 Command 写入数据并生成自定义响应。下面示例约定 `cmd=9000` 为测试命令：手机发送 JSON，设备通过 Notify 回显 `data` 字段。

```c
#include <string.h>
#include "ble_provisioning.h"
#include "cJSON.h"

static esp_err_t my_json_echo(void *user_ctx,
                             const uint8_t *data,
                             size_t len,
                             char *response,
                             size_t response_len)
{
    cJSON *req = cJSON_ParseWithLength((const char *)data, len);
    if (req == NULL) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    cJSON *cmd_item = cJSON_GetObjectItem(req, "cmd");
    if (cmd_item == NULL || !cJSON_IsNumber(cmd_item) || cmd_item->valueint != 9000) {
        cJSON_Delete(req);
        return ESP_ERR_NOT_SUPPORTED;
    }

    cJSON *res = cJSON_CreateObject();
    cJSON_AddNumberToObject(res, "cmd", 9000);
    cJSON_AddStringToObject(res, "status", "success");

    cJSON *data_item = cJSON_GetObjectItem(req, "data");
    if (data_item != NULL) {
        cJSON_AddItemToObject(res, "echo", cJSON_Duplicate(data_item, true));
    }

    bool ok = cJSON_PrintPreallocated(res, response, response_len, false);
    cJSON_Delete(req);
    cJSON_Delete(res);
    if (!ok) {
        snprintf(response, response_len, "{\"cmd\":9000,\"status\":\"fail\",\"msg\":\"resp too long\"}");
    }
    return ESP_OK;
}

void app_main(void)
{
    ble_prov_config_t config = {.device_name = "ESP32-S3-TEXT"};
    ble_provisioning_init(&config, true);
    ble_provisioning_set_custom_handler(my_json_echo, NULL);
}
```

手机端测试：
*   连接设备后，找到 **Write/Command** 特征值写入：

```json
{"cmd":9000,"data":"hello"}
```

*   打开 **Notify/Status** 特征值通知，接收类似回包：

```json
{"cmd":9000,"status":"success","echo":"hello"}
```

说明：
*   自定义回调优先级高于内置 JSON 协议解析；当回调返回 `ESP_ERR_NOT_SUPPORTED` 等非 `ESP_OK` 时，会回落到内置协议处理。
*   如需异步处理（例如创建任务、分包发送），回调返回 `BLE_PROV_PROTO_ASYNC`，并在异步流程中自行调用 `ble_provisioning_send_notify()` 上报。

## 更新记录

### v1.0.5

*   文档更新：补充 `ble_prov_config_t.device_info` 的外部传参说明，并补充 `ble_prov_config_set()` / `ble_prov_config_get()` 的 NVS 持久化示例。

### v1.0.4

*   新增：自定义协议处理钩子（`ble_provisioning_set_custom_handler` / `ble_provisioning_send_notify`），便于外部扩展自定义通信协议。
*   新增：BLE 连接建立后自动触发一次 WiFi 扫描，并通过 Notify 上报扫描结果。
*   新增：设备信息配置（`sys_info / sw_info / state`）支持通过 `ble_prov_config_t.device_info` 外部传入默认值，并可通过 `ble_prov_config_set()` 保存到 NVS（`include/config.h` / `src/config.c`）。
*   变更：`ble_provisioning_init()` 增加 `init_nvs` 参数，便于工程统一初始化 NVS。

## 注意事项

1.  **UUID 匹配**：
    *   上位机（Python 脚本或手机 App）使用的 UUID 必须与固件中 `ble_gatts_module.c` 定义的 UUID 一致。
    *   默认 UUID：
        *   Service: `00000001-0000-1000-8000-00805F9B34FB`
        *   Write: `00000002-0000-1000-8000-00805F9B34FB`
        *   Notify: `00000003-0000-1000-8000-00805F9B34FB`

2.  **句柄数量 (`GATTS_NUM_HANDLE_TEST_A`)**：
    *   在 `ble_gatts_module.c` 中，宏 `GATTS_NUM_HANDLE_TEST_A` 定义了服务的最大句柄数。
    *   当前定义为 **8**，足以支持当前的 Service + 2 Characteristics + 1 Descriptor。
    *   如果您添加了新的特征值，务必增加此数值，否则新特征值将无法添加。

3.  **分区表大小**：
    *   启用蓝牙和 WiFi 栈后，固件体积会显著增加（通常 >1MB）。
    *   请确保分区表中的 APP 分区足够大（建议至少 1.5MB 或 2MB），否则会出现 `app partition is too small` 编译错误。

4.  **MTU 限制**：
    *   默认 BLE MTU 较小（23 字节）。虽然组件设置了本地 MTU 为 500，但实际传输大小取决于手机/上位机的协商结果。
    *   发送长 JSON 响应时，建议上位机先请求增大 MTU，或在应用层进行分包处理（当前组件通过 Notify 直接发送，如果超过 MTU 可能会被截断）。

## 测试工具

组件附带了一个 Python 上位机脚本，位于 `python_tool/` 目录下。它可以模拟手机 App 进行全功能的配网和控制测试。详细使用说明请参考该目录下的 `README.md`。
