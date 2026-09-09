# EgoLowBle 公共库使用说明

本文面向需要在应用中集成 `EgoLowBle` 公共库的开发者，说明如何使用该库完成 BLE 扫描、设备连接、Wi-Fi 配网、IP 查询和资源释放。

`EgoLowBle` 对外提供稳定的 C ABI，应用侧只需要依赖公共头文件和平台发布包，不需要直接调用 Windows、Linux、Android 或 iOS 的底层 BLE API。

## 1. 库能力概览

`EgoLowBle` 主要提供以下能力：

- 扫描附近 BLE 设备。
- 获取设备名称、地址、RSSI 和是否可连接。
- 按设备地址或设备名称连接 BLE 外设。
- 查询连接状态、MTU 和 GATT characteristic 列表。
- 读取和写入 characteristic。
- 等待 notification / indication。
- 使用固定协议 helper 完成 Wi-Fi 配网。
- 配网后查询设备 IP。
- 通过状态码和 `EgoLowBle_GetLastError()` 获取错误详情。

推荐应用侧将 BLE 流程封装在独立模块中，而不是在 UI 或业务页面中直接分散调用 `EgoLowBle_*`。

## 2. 集成和部署

Windows 预编译包通常包含：

```text
include/EgoLowBle/egolowble.h
lib/EgoLowBle.lib
bin/EgoLowBle.dll
```

编译时需要：

- 将 `include/` 加入头文件搜索路径。
- 链接 `lib/EgoLowBle.lib`。
- 在代码中包含 `EgoLowBle/egolowble.h`。

运行时需要：

- 将 `EgoLowBle.dll` 放到应用可执行文件同级目录，或放到系统能够搜索到的 DLL 路径。

最小运行布局示例：

```text
your_app.exe
EgoLowBle.dll
```

## 3. 推荐分层

建议应用侧按以下方式组织代码：

```text
UI / command layer
  -> provisioning workflow
    -> EgoLowBle wrapper
      -> EgoLowBle C API
```

各层职责：

- UI / command layer：收集用户输入，展示状态和日志，启动后台任务。
- provisioning workflow：编排扫描、连接、配网、IP 轮询和清理。
- EgoLowBle wrapper：封装 C API、错误转换、日志字段和资源生命周期。
- EgoLowBle C API：执行平台 BLE 操作和协议 helper。

这种分层可以避免 UI 线程直接阻塞，也方便在不同应用中复用配网流程。

## 4. 典型配网流程

推荐调用顺序：

```text
创建 BLE session
  -> 扫描 BLE 设备
  -> 选择目标设备
  -> 优先按地址连接，必要时按名称回退
  -> 校验连接状态、MTU、characteristic 列表
  -> 下发 Wi-Fi SSID 和密码
  -> 轮询查询设备 IP
  -> 断开 BLE 外设连接
  -> 使用 IP 进入后续业务流程
  -> 应用退出时销毁 BLE session
```

常用超时建议：

```text
BLE scan timeout: 3000-5000 ms
BLE connect timeout: 5000 ms
Wi-Fi provisioning timeout: 15000 ms
IP request timeout: 1000 ms
IP total wait timeout: 30000 ms
IP poll interval: 1000 ms
```

实际产品可以根据设备广播周期、网络环境和用户体验调整这些值。

## 5. 句柄生命周期

`EgoLowBle` 使用不透明句柄表示一次 BLE session：

```c
typedef void* EgoLowBleHandle;
```

创建 session：

```c
EgoLowBleHandle handle = NULL;
EgoLowBleStatus status = EgoLowBle_Create(&handle);
```

释放 session：

```c
EgoLowBle_Disconnect(handle);
EgoLowBle_Destroy(handle);
handle = NULL;
```

使用建议：

- 第一次执行扫描或配网前创建 handle。
- 同一业务模块可以复用一个 handle，减少重复初始化成本。
- 单次配网结束后应断开 BLE 外设连接。
- 应用或模块退出时再销毁 handle。
- 不要在同一个 handle 上并发执行多个 BLE 操作。

## 6. 扫描 BLE 设备

扫描接口：

```c
EgoLowBleStatus EgoLowBle_ScanDevices(
    EgoLowBleHandle handle,
    int scan_timeout_ms,
    EgoLowBleDeviceInfo* items,
    size_t* item_count);
```

该接口采用两阶段调用模式：

```c
size_t count = 0;
EgoLowBleStatus status = EgoLowBle_ScanDevices(handle, 5000, NULL, &count);
if (status != EGOLOWBLE_STATUS_OK || count == 0) {
    return;
}

EgoLowBleDeviceInfo* devices =
    (EgoLowBleDeviceInfo*)malloc(sizeof(EgoLowBleDeviceInfo) * count);

status = EgoLowBle_ScanDevices(handle, 5000, devices, &count);
```

扫描结果结构：

```c
typedef struct EgoLowBleDeviceInfo {
    char device_name[256];
    char device_address[64];
    int16_t rssi;
    uint8_t is_connectable;
} EgoLowBleDeviceInfo;
```

应用侧建议：

- 优先展示 `is_connectable != 0` 的设备。
- 如果目标设备有固定名称前缀，可以在应用侧过滤。
- 扫描结果按 RSSI 从高到低排序，方便用户选择。
- 多设备环境中应保留并展示设备地址，避免同名设备混淆。
- 如果第二阶段返回 `EGOLOWBLE_STATUS_BUFFER_TOO_SMALL`，按 `item_count` 扩容后重试。

## 7. 选择和连接设备

连接接口：

```c
EgoLowBleStatus EgoLowBle_ConnectByAddress(
    EgoLowBleHandle handle,
    const char* device_address,
    int scan_timeout_ms);

EgoLowBleStatus EgoLowBle_ConnectByName(
    EgoLowBleHandle handle,
    const char* device_name,
    int scan_timeout_ms);
```

推荐策略：

1. 用户或业务逻辑先从扫描列表中选择目标设备。
2. 如果扫描结果中有设备地址，优先调用 `EgoLowBle_ConnectByAddress()`。
3. 如果按地址连接失败，并且有设备名称，再尝试 `EgoLowBle_ConnectByName()`。
4. 连接前可以先调用 `EgoLowBle_Disconnect()`，确保旧连接状态被清理。

连接成功后建议做基础验证：

```c
int connected = 0;
uint16_t mtu = 0;
size_t characteristic_count = 0;

EgoLowBle_IsConnected(handle, &connected);
EgoLowBle_GetMtu(handle, &mtu);
EgoLowBle_ListCharacteristics(handle, NULL, &characteristic_count);
```

这些检查可以帮助确认：

- BLE 库认为设备仍处于连接状态。
- GATT 链路已经建立。
- 设备服务枚举正常。

## 8. Wi-Fi 配网

Wi-Fi 配网接口：

```c
EgoLowBleStatus EgoLowBle_ConfigureWifi(
    EgoLowBleHandle handle,
    const char* ssid,
    const char* password,
    int timeout_ms,
    EgoLowBleWifiConfigResponse* response);
```

响应结构：

```c
typedef struct EgoLowBleWifiConfigResponse {
    uint8_t result;
    char reason[65];
} EgoLowBleWifiConfigResponse;
```

调用示例：

```c
EgoLowBleWifiConfigResponse response = {0};
EgoLowBleStatus status = EgoLowBle_ConfigureWifi(
    handle,
    "MyWiFi",
    "MyPassword",
    15000,
    &response);

if (status != EGOLOWBLE_STATUS_OK) {
    // BLE 传输、超时或协议解析失败。
}
else if (response.result != 0) {
    // 设备拒绝配网或设备端配网失败，原因见 response.reason。
}
else {
    // 设备已接受 Wi-Fi 配置。
}
```

关键语义：

- `status == EGOLOWBLE_STATUS_OK` 只表示 BLE 通信和协议解析成功。
- `response.result == 0` 表示设备接受 Wi-Fi 配置。
- 设备接受 Wi-Fi 配置不代表已经连接路由器，也不代表已经拿到 IP。
- 日志中可以记录 SSID，但不要记录明文密码。

## 9. 查询设备 IP

配网请求成功后，需要继续通过 BLE 查询设备 IP。

接口：

```c
EgoLowBleStatus EgoLowBle_RequestIp(
    EgoLowBleHandle handle,
    int timeout_ms,
    EgoLowBleIpResponse* response);
```

响应结构：

```c
typedef enum EgoLowBleIpResult {
    EGOLOWBLE_IP_RESULT_SUCCESS = 0,
    EGOLOWBLE_IP_RESULT_NOT_CONFIGURED = 1,
    EGOLOWBLE_IP_RESULT_CONFIGURING = 2,
    EGOLOWBLE_IP_RESULT_CONFIGURE_FAILED = 3
} EgoLowBleIpResult;

typedef struct EgoLowBleIpResponse {
    uint8_t result;
    char ip[33];
    char reason[33];
} EgoLowBleIpResponse;
```

推荐轮询逻辑：

```c
for (int i = 0; i < 30; ++i) {
    EgoLowBleIpResponse response = {0};
    EgoLowBleStatus status = EgoLowBle_RequestIp(handle, 1000, &response);

    if (status != EGOLOWBLE_STATUS_OK) {
        break;
    }

    if (response.result == EGOLOWBLE_IP_RESULT_SUCCESS &&
        response.ip[0] != '\0') {
        printf("Device IP: %s\n", response.ip);
        break;
    }

    Sleep(1000);
}
```

成功条件：

```c
response.result == EGOLOWBLE_IP_RESULT_SUCCESS && response.ip[0] != '\0'
```

如果长时间无法获取 IP，通常需要排查：

- Wi-Fi SSID 或密码是否正确。
- 设备是否支持目标 Wi-Fi 频段和加密方式。
- 路由器 DHCP 是否给设备分配 IP。
- 设备与后续业务客户端是否处于可达网络。
- 设备是否持续返回 `CONFIGURING` 或 `CONFIGURE_FAILED`。

## 10. 配网结束后的 BLE 连接释放

配网流程结束后建议立即断开 BLE 外设连接：

```c
EgoLowBle_Disconnect(handle);
```

这样做有几个原因：

- 避免设备长期保持 BLE 连接。
- 释放系统 BLE 连接资源。
- 允许后续扫描或其他客户端重新连接设备。
- 配网后主要业务通常会切换到 Wi-Fi/IP 通道。

注意区分“断开连接”和“销毁 session”：

```text
EgoLowBle_Disconnect(handle)  只断开当前外设连接，handle 仍可复用
EgoLowBle_Destroy(handle)     销毁整个 BLE session，handle 不可再用
```

推荐资源边界：

- 单次配网成功或失败后：调用 `EgoLowBle_Disconnect()`。
- 应用或 BLE 模块退出时：调用 `EgoLowBle_Disconnect()` 后再调用 `EgoLowBle_Destroy()`。
- 如果后续还要继续扫描或再次配网，可以保留 handle，不必立刻 `Destroy`。

## 11. 固定配网协议说明

如果使用 `EgoLowBle_ConfigureWifi()` 和 `EgoLowBle_RequestIp()` helper，应用侧无需手动拼接 GATT payload。了解协议有助于联调设备端。

固定 GATT 通道：

```text
service UUID:        0000ffe0-0000-1000-8000-00805f9b34fb
characteristic UUID: 0000ff03-0000-1000-8000-00805f9b34fb
```

Wi-Fi 配网命令：

```text
"A001" + 32 字节 SSID 字段 + 32 字节 password 字段
```

IP 查询命令：

```text
"A002"
```

文本字段按 ASCII / UTF-8 字节写入，右侧使用 `0x00` 填充。SSID 和密码长度限制按字节计算，不按字符数计算。

## 12. 错误处理

所有 API 都返回 `EgoLowBleStatus`。失败时可以调用 `EgoLowBle_GetLastError()` 获取详细错误文本。

错误文本读取方式：

```c
size_t required = 0;
EgoLowBle_GetLastError(handle, NULL, 0, &required);

if (required > 0) {
    char* text = (char*)malloc(required);
    if (EgoLowBle_GetLastError(handle, text, required, &required) == EGOLOWBLE_STATUS_OK) {
        printf("BLE error: %s\n", text);
    }
    free(text);
}
```

常见状态含义：

```text
EGOLOWBLE_STATUS_INVALID_ARGUMENT      参数错误，例如空指针或字段过长
EGOLOWBLE_STATUS_ADAPTER_NOT_FOUND     没有找到蓝牙适配器
EGOLOWBLE_STATUS_DEVICE_NOT_FOUND      没有找到目标设备
EGOLOWBLE_STATUS_NOT_CONNECTED         当前 handle 未连接设备
EGOLOWBLE_STATUS_BUFFER_TOO_SMALL      输出数组或字符串缓冲区不足
EGOLOWBLE_STATUS_BLUETOOTH_UNAVAILABLE 蓝牙不可用
EGOLOWBLE_STATUS_OPERATION_FAILED      平台 BLE 或 GATT 操作失败
EGOLOWBLE_STATUS_TIMEOUT               操作超时
EGOLOWBLE_STATUS_PROTOCOL_ERROR        协议响应格式不符合预期
EGOLOWBLE_STATUS_REMOTE_ERROR          设备端返回错误
```

错误处理建议：

- 先判断 API 返回的 `EgoLowBleStatus`。
- 对 `EgoLowBle_ConfigureWifi()`，再判断 `response.result`。
- 对 `EgoLowBle_RequestIp()`，再判断 `response.result` 和 `response.ip`。
- 日志中不要输出 Wi-Fi 密码。
- 失败日志应包含 API 名称、状态码和 `GetLastError` 文本。

## 13. 线程和 UI 注意事项

BLE 扫描、连接、配网和 IP 查询都可能阻塞。应用侧建议：

- 在后台线程执行 `EgoLowBle_ScanDevices()`、`EgoLowBle_ConnectByAddress()`、`EgoLowBle_ConfigureWifi()` 等阻塞调用。
- UI 线程只负责展示状态、日志和进度。
- 同一个 handle 上不要并发执行多个 BLE 操作。
- 配网过程中避免同时启动其他会改变设备连接状态的流程。
- 所有阻塞调用都应设置合理 timeout。

## 14. 最小 C API 示例

下面示例展示公共库层面的最小调用顺序。实际产品中建议再封装错误处理、日志和线程调度。

```c
EgoLowBleHandle handle = NULL;
EgoLowBleStatus status = EgoLowBle_Create(&handle);
if (status != EGOLOWBLE_STATUS_OK) {
    return;
}

size_t count = 0;
status = EgoLowBle_ScanDevices(handle, 5000, NULL, &count);
if (status != EGOLOWBLE_STATUS_OK || count == 0) {
    goto cleanup;
}

EgoLowBleDeviceInfo* devices =
    (EgoLowBleDeviceInfo*)malloc(sizeof(EgoLowBleDeviceInfo) * count);
status = EgoLowBle_ScanDevices(handle, 5000, devices, &count);
if (status != EGOLOWBLE_STATUS_OK) {
    free(devices);
    goto cleanup;
}

const char* address = devices[0].device_address;
status = EgoLowBle_ConnectByAddress(handle, address, 5000);
free(devices);
if (status != EGOLOWBLE_STATUS_OK) {
    goto cleanup;
}

EgoLowBleWifiConfigResponse wifi = {0};
status = EgoLowBle_ConfigureWifi(handle, "MyWiFi", "MyPassword", 15000, &wifi);
if (status != EGOLOWBLE_STATUS_OK || wifi.result != 0) {
    goto cleanup;
}

for (int i = 0; i < 30; ++i) {
    EgoLowBleIpResponse ip = {0};
    status = EgoLowBle_RequestIp(handle, 1000, &ip);
    if (status == EGOLOWBLE_STATUS_OK &&
        ip.result == EGOLOWBLE_IP_RESULT_SUCCESS &&
        ip.ip[0] != '\0') {
        printf("Device IP: %s\n", ip.ip);
        break;
    }
    Sleep(1000);
}

cleanup:
EgoLowBle_Disconnect(handle);
EgoLowBle_Destroy(handle);
```

## 15. 集成要点

- 运行时部署 `EgoLowBle.dll`。
- 使用两阶段 `EgoLowBle_ScanDevices()` 获取扫描结果。
- 多设备环境中优先按 address 连接。
- 连接后建议查询 `IsConnected`、`GetMtu`、`ListCharacteristics` 做诊断确认。
- `EgoLowBle_ConfigureWifi()` 成功不代表已经拿到 IP。
- 必须通过 `EgoLowBle_RequestIp()` 轮询获取设备 IP。
- 单次配网结束后调用 `EgoLowBle_Disconnect()`。
- 应用退出时调用 `EgoLowBle_Destroy()`。
- BLE 阻塞操作不要放在 UI 线程执行。

## 16. 与最新 Ego C++ SDK 的联动

当前 OpenOrbbecSDK 的 Ego 示例已经把设备侧扩展能力集中到 `ob::DataAcquisitionDevice`（`include/libobsensor/hpp/DataAcquisitionDevice.hpp`）。

推荐把 BLE 配网与 SDK 数据链路拆成两个阶段：

1. BLE 阶段：通过 `EgoLowBle_ConfigureWifi()` + `EgoLowBle_RequestIp()` 获取设备 IP。
2. SDK 阶段：使用 `ob::Context::createNetDevice(ip, 8090)` 建立数据链路，并按需转换为 `DataAcquisitionDevice` 调用 Ego 扩展接口。

最小联动流程示例：

```cpp
// 假设已通过 EgoLowBle 获取到 ip 字符串
ob::Context ctx;
auto rawDevice = ctx.createNetDevice(ip.c_str(), 8090);

// 建议转换为 DataAcquisitionDevice 使用 Ego 扩展能力
auto egoDevice = ob::DataAcquisitionDevice::fromDevice(rawDevice);

// 示例：注册状态回调
egoDevice->setEgoStateCallback([](const OBEgoStateReport &state) {
    // state.work_state / state.state_flags / state.error_flags
});

// 示例：创建视频管线
ob::Pipeline pipe(egoDevice);
auto config = std::make_shared<ob::Config>();
config->enableVideoStream(OB_STREAM_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_H264);
pipe.start(config);
```

联动建议：

- BLE 层仅负责“发现设备、配网、拿 IP、断开 BLE”；
- 视频/音频/IMU 采集统一走 Orbbec SDK；
- 设备端能力（录制目录、固件日志导出、设备端音频播放、电池/状态回调）走 `DataAcquisitionDevice`；
- `fromDevice` 会转移底层句柄所有权，转换后不要继续使用旧的 `rawDevice` 实例。
