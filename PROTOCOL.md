# BLE 通信协议文档

## 1. 概述 (Overview)

本文档描述了 ESP32-S3 BLE 组件与上位机（手机 APP 或其他 BLE 中心设备）之间的通信协议。
协议分为两部分：
1.  **控制指令**：使用 JSON 格式，用于设备配置、状态查询等。
2.  **OTA 升级**：使用自定义二进制帧格式，确保大数据传输的高效与可靠。

**注意**：BLE 单包数据受限于 MTU (Maximum Transmission Unit)。默认 MTU 为 23 字节（有效负载 20 字节）。建议上位机连接后请求修改 MTU 为 256 或 512 字节，以提高传输效率。

## 2. GATT 服务定义 (GATT Service Definition)

本设备作为 GATT Server，提供以下服务：

### 2.1 主服务 (Main Service)

*   **Service UUID**: `00000001-0000-1000-8000-00805F9B34FB` (示例，需根据实际定义修改)

### 2.2 特征值 (Characteristics)

| 特征名称 (Name) | UUID | 属性 (Properties) | 描述 (Description) |
| :--- | :--- | :--- | :--- |
| **Command Write** | `00000002-0000-1000-8000-00805F9B34FB` | Write / Write No Response | 用于上位机下发指令 (JSON/OTA数据) |
| **Status Notify** | `00000003-0000-1000-8000-00805F9B34FB` | Notify / Read | 用于设备上报状态或回复指令 |

> **注意**: 上述 UUID 为示例值，开发时请生成并替换为自定义的 UUID。

## 3. 数据帧格式 (Data Frame Format)

### 3.1 控制指令 (JSON)

常规交互使用 JSON 格式字符串。
为减少数据量和解析复杂度，JSON 键名 (Key) 统一使用英文，值 (Value) 可使用 UTF-8 编码的中文。

**示例**:
```json
{
    "cmd": 1,
    "vendor": "XRGEEK",
    "data": { ... }
}
```

### 3.2 OTA 升级数据帧 (Binary)

OTA 升级时，为提高效率，使用二进制格式传输固件数据。

**帧结构**:

| 偏移 | 字段 | 长度 (Bytes) | 描述 |
| :--- | :--- | :--- | :--- |
| 0 | **Head** | 2 | 固定帧头 `0x55 0xA5` |
| 2 | **Cmd** | 1 | 命令字，OTA 数据固定为 `0xF0` |
| 3 | **Seq** | 2 | 包序号 (0-65535)，用于检测丢包/乱序 (大端序) |
| 5 | **Len** | 2 | 数据长度 (Data 字段的长度，大端序) |
| 7 | **Data** | N | 固件数据内容 |
| 7+N | **Sum** | 1 | 校验和 (Head+Cmd+Seq+Len+Data 的累加和，取低8位) |

**校验算法**:
```c
uint8_t calculate_checksum(const uint8_t *data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}
```

## 4. 指令定义 (Command Definitions)

**Cmd ID 分配**:

| Cmd ID | 定义 | 说明 |
| :--- | :--- | :--- |
| `0x01` | Connect | 连接设备与获取信息 |
| `0x02` | WiFi Scan | 扫描 WiFi |
| `0x03` | WiFi Connect | 连接 WiFi |
| `0x04` | WiFi Query | 查询已保存 WiFi |
| `0x05` | WiFi Modify | 修改/删除 WiFi |
| `0x06` | Reboot | 重启设备 |
| `0x07` | Update Check | 检查更新 |
| `0x08` | Update Start | 开始更新 |
| `0xF0` | OTA Data | OTA 固件数据包 (Binary) |

### 4.1 连接设备 (Connect)

**APP -> 设备**:
```json
{
    "cmd": 1,
    "vendor": "XRGEEK"
}
```

**设备 -> APP**:
```json
{
    "cmd": 1,
    "status": "success",
    "sys_info": {
        "name": "ESP32-S3-TEXT",
        "mac": "XX:XX:XX:XX:XX:XX",
        "fw_ver": "1.0.0",
        "hw_ver": "1.0.0",
        "proto_ver": "1.0.0"
    },
    "sw_info": {
        "name": "ESP32-S3-TEXT",
        "ver": "1.0.0",
        "desc": "ESP32-S3 Module",
        "date": "2023-08-01"
    },
    "state": {
        "wifi": "connected",    // connected, disconnected
        "dev": "normal",        // normal, error
        "server": "connected"
    }
}
```

### 4.2 WiFi 扫描 (WiFi Scan)

**APP -> 设备**:
```json
{
    "cmd": 2,
    "vendor": "XRGEEK"
}
```

**设备 -> APP**:
```json
{
    "cmd": 2,
    "status": "success",
    "scan_res": {
        "count": 1,
        "list": [
            {
                "ssid": "wifi1",
                "rssi": -70
            }
        ]
    }
}
```
*(注：如果 WiFi 列表过长导致 JSON 超过 MTU，建议仅返回前 N 个或分多次 Notify 发送)*

### 4.3 WiFi 连接 (WiFi Connect)

**APP -> 设备**:
```json
{
    "cmd": 3,
    "vendor": "XRGEEK",
    "wifi": {
        "ssid": "wifi1",
        "pwd": "123456" // 建议传输时加密或编码
    }
}
```

**设备 -> APP**:
```json
{
    "cmd": 3,
    "status": "success", // success, fail
    "msg": "connected"
}
```

### 4.4 查询已保存 WiFi (WiFi Query)

**APP -> 设备**:
```json
{
    "cmd": 4,
    "vendor": "XRGEEK"
}
```

**设备 -> APP**:
```json
{
    "cmd": 4,
    "status": "success",
    "saved_wifi": {
        "count": 1,
        "list": [
            { "ssid": "wifi1" }
        ]
    }
}
```

### 4.5 修改已保存 WiFi (WiFi Modify)

**APP -> 设备**:
```json
{
    "cmd": 5,
    "vendor": "XRGEEK",
    "action": "delete", // delete
    "list": [
        { "ssid": "wifi1" }
    ]
}
```

**设备 -> APP**:
返回查询结果 (Cmd 0x04)。

### 4.6 重启设备 (Reboot)

**APP -> 设备**:
```json
{
    "cmd": 6,
    "vendor": "XRGEEK"
}
```

**设备 -> APP**:
```json
{
    "cmd": 6,
    "status": "success",
    "msg": "rebooting"
}
```

### 4.7 检查更新 (Update Check)

**APP -> 设备**:
```json
{
    "cmd": 7,
    "vendor": "XRGEEK"
}
```

**设备 -> APP**:
```json
{
    "cmd": 7,
    "status": "success",
    "has_update": true // true, false
}
```

### 4.8 开始更新 (Update Start)

**APP -> 设备**:
```json
{
    "cmd": 8,
    "vendor": "XRGEEK",
    "url": "https://example.com/ota.bin", // 可选，如果设备自行下载
    "size": 102400 // 可选，固件大小
}
```

**设备 -> APP**:
```json
{
    "cmd": 8,
    "status": "ready" // ready: 准备好接收数据
}
```

### 4.9 OTA 数据传输 (OTA Data)

**APP -> 设备**:
发送二进制帧 (Cmd `0xF0`)。

**优化策略**:
为了提高传输效率和校验效率，强烈建议采用以下策略：
1.  **增大 MTU**: 上位机连接后应请求增大 MTU（512 字节）。这样单帧有效载荷可达 240+ 字节，头部开销占比从 ~30% 降至 <3%。
2.  **批量传输 (Burst Write)**: 利用 BLE 的 `Write Without Response` 特性，上位机连续发送 N 个数据包（例如 16 个包或 4KB 数据），无需每包等待回复。
3.  **累计确认**: 设备仅在收到 N 个包后，或检测到丢包/错误时，才发送一次确认帧。

**累计确认模式 (示例)**:
上位机连续发送 Seq 10~25 的数据包。
设备收到 Seq 25 后（或达到设定的窗口大小），回复确认：
```json
{
    "cmd": 240, // 0xF0
    "seq": 25,  // 确认已连续接收到 Seq 25 (隐含 25 之前的数据都正确)
    "status": 0 // 0: OK, 1: Error (需重传), 2: Complete
}
```
若中间发生丢包（例如收到 10, 11, 13），设备应立即报错或请求重传：
```json
{
    "cmd": 240,
    "seq": 11,  // 期望收到 12，但在 12 处中断
    "status": 1 // Error/Retry
}
```

## 5. 状态码与常量 (Constants)

### 5.1 屏幕方向
*   1: 正常 (Normal)
*   2: 90度
*   3: 180度
*   4: 270度

### 5.2 屏幕镜像
*   1: 无 (None)
*   2: 水平 (Horizontal)
*   3: 垂直 (Vertical)
*   4: 水平+垂直 (Both)

### 5.3 错误码
*   0: 成功 (Success)
*   1: 失败 (Fail)
*   2: 参数错误 (Invalid Arg)
*   3: 校验错误 (Checksum Error)
*   4: 内存不足 (No Mem)
