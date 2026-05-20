import asyncio
import json
from bleak import BleakScanner, BleakClient

# 定义 UUID
# 服务 UUID，需与设备端保持一致
SERVICE_UUID = "00000001-0000-1000-8000-00805F9B34FB"
# 命令特征值 UUID (Write)，用于发送 JSON 指令
CHAR_CMD_UUID = "00000002-0000-1000-8000-00805F9B34FB"
# 状态特征值 UUID (Notify)，用于接收设备响应
CHAR_STATUS_UUID = "00000003-0000-1000-8000-00805F9B34FB"

# 目标设备名称
DEVICE_NAME = "ESP32-S3-TEXT"

client = None
disconnect_event = asyncio.Event()


def disconnected_handler(bleak_client):
    """
    处理设备断开事件
    """
    disconnect_event.set()
    print("\n[状态] BLE 已断开。若设备已成功连上 WiFi，可能是固件按配置自动关闭了 BLE。")

def notification_handler(sender, data):
    """
    处理来自设备的通知消息
    """
    print(f"\n[通知] {sender}: {data}")
    try:
        resp = json.loads(data.decode('utf-8'))
        print(f"[JSON] {json.dumps(resp, indent=2, ensure_ascii=False)}")
    except Exception as e:
        print(f"[错误] JSON 解析失败: {e}")


async def get_services_compat(bleak_client):
    """
    兼容不同 bleak 版本的服务发现接口
    """
    get_services = getattr(bleak_client, "get_services", None)
    if callable(get_services):
        services = await get_services()
        if services is not None:
            return services
    return bleak_client.services

async def scan_and_connect():
    """
    扫描并连接到指定名称的 BLE 设备
    """
    global client
    print("正在扫描设备...")
    # 扫描设备
    device = await BleakScanner.find_device_by_filter(
        lambda d, ad: d.name and DEVICE_NAME in d.name
    )
    
    if not device:
        print(f"未找到设备 {DEVICE_NAME}。")
        return False

    print(f"找到设备: {device.name} ({device.address})")
    disconnect_event.clear()
    client = BleakClient(device, disconnected_callback=disconnected_handler)
    # 连接设备
    await client.connect()
    print("已连接!")

    services = await get_services_compat(client)
    print("发现的服务:")
    for service in services:
        print(f"[服务] {service.uuid}")
        for char in service.characteristics:
            print(f"  [特征值] {char.uuid} ({','.join(char.properties)})")

    # 开启通知订阅
    await client.start_notify(CHAR_STATUS_UUID, notification_handler)
    print("[状态] 已开启 Notify，设备可能会自动上报一次 WiFi 扫描结果。")
    return True

async def send_command(cmd_id, data=None):
    """
    发送 JSON 命令到设备
    """
    if not client or not client.is_connected:
        print("未连接。")
        return

    payload = {
        "cmd": cmd_id,
        "vendor": "XRGEEK"
    }
    if data:
        payload.update(data)
    
    json_str = json.dumps(payload)
    print(f"[发送] {json_str}")
    # 写入特征值
    await client.write_gatt_char(CHAR_CMD_UUID, json_str.encode('utf-8'), response=True)


async def wait_after_command(seconds=1.0):
    """
    等待命令异步响应或设备断开
    """
    try:
        await asyncio.wait_for(disconnect_event.wait(), timeout=seconds)
    except asyncio.TimeoutError:
        pass

async def main():
    if not await scan_and_connect():
        return

    while True:
        if not client or not client.is_connected:
            print("设备当前未连接，程序结束。")
            break

        print("\n--- 菜单 ---")
        print("1. 连接 (获取信息)")
        print("2. WiFi 扫描")
        print("3. WiFi 连接")
        print("4. WiFi 查询")
        print("5. WiFi 删除")
        print("6. 重启")
        print("q. 退出")
        
        choice = input("请选择: ")
        
        if choice == '1':
            await send_command(1)
        elif choice == '2':
            await send_command(2)
        elif choice == '3':
            ssid = input("请输入 SSID: ")
            pwd = input("请输入密码: ")
            await send_command(3, {"wifi": {"ssid": ssid, "pwd": pwd}})
        elif choice == '4':
            await send_command(4)
        elif choice == '5':
            ssid = input("请输入要删除的 SSID: ")
            await send_command(5, {"action": "delete", "list": [{"ssid": ssid}]})
        elif choice == '6':
            await send_command(6)
        elif choice == 'q':
            break
        else:
            print("无效选择，请重新输入。")
            continue
        
        await wait_after_command(2.0)

    if client:
        if client.is_connected:
            await client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
