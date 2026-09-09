# EgoLowBle 安卓蓝牙库使用说明

本文面向 `EgoLowBle-android-1.1.5` 发布包，说明 Android 应用如何集成发布包中的 `EgoLowBle-1.1.5.aar`，以及如何通过 AAR 内置的 `EgoLowBleNative` JNI 接口完成 BLE 扫描、连接、GATT 读写、Wi-Fi 配网、IP 查询和资源释放。

注意：`EgoLowBle-android-1.1.5` 的 AAR 不包含 `EgoLowBleClient`、`EgoLowBlePermissions`、`EgoLowBleScanResult`、`EgoLowBleOperationResult` 等高层 Kotlin 封装。业务代码需要直接管理 native handle，或在应用侧自行封装一层。

## 1. 库能力概览

`EgoLowBle-android-1.1.5` 的 Kotlin/JNI 层提供以下能力：

- 扫描附近符合过滤条件的 BLE 设备。
- 获取设备名称、地址、RSSI 和是否可连接。
- 按设备名称或设备地址连接 BLE 外设。
- 断开当前 BLE 连接。
- 获取连接后的 MTU。
- 读取 characteristic。
- 使用 ATT Write Request 或 ATT Write Command 写入 characteristic。
- 使用固定 FFE0/FF03 通道发送 Wi-Fi 配网命令。
- 使用固定 FFE0/FF03 通道查询设备 IP。
- 读取 native session 的最近一次错误文本。

1.1.5 的 Kotlin/JNI 层没有暴露以下 C API 能力：

- characteristic 枚举。
- 连接状态查询。
- notification / indication 等待。
- 通用协议通道配置和通用协议收发。
- `statusName()` 这类状态码转文本工具。

如业务需要这些能力，需要基于发布包中的 `include/EgoLowBle/egolowble.h` 自行扩展 JNI。

## 2. 发布包和 AAR 内容

发布包目录：

```text
release/EgoLowBle-android-1.1.5/
```

主要内容：

```text
aar/EgoLowBle-1.1.5.aar
include/EgoLowBle/egolowble.h
docs/ANDROID_GUIDE.md
examples/android-jni/
meta/VERSION.txt
```

`EgoLowBle-1.1.5.aar` 已包含：

```text
libEgoLowBle.so
libEgoLowBleJni.so
com.ego.egolowble.EgoLowBleNative
com.ego.egolowble.EgoLowBleScanDevice
com.ego.egolowble.EgoLowBleWifiConfigResponse
com.ego.egolowble.EgoLowBleIpResponse
org.simpleble.android.bridge.*
```

运行基线：

```text
minSdk:      24
Android ABI: arm64-v8a, armeabi-v7a
```

建议应用工程使用：

```text
compileSdk: 31 或更高，推荐 36
Java/Kotlin target: 17
```

注意：

- AAR 不包含 `x86` 和 `x86_64` 原生库，常规 x86 Android 模拟器会触发 `UnsatisfiedLinkError`。
- 联调建议使用 ARM 真机，或使用能够运行 ARM ABI 的环境。
- 发布包中的 `examples/android-jni/` 是 JNI 参考源码，不会被打进 AAR。
- 1.1.5 AAR 内的 `proguard.txt` 为空。如果应用开启 R8/ProGuard，建议应用侧显式添加 keep 规则，见本文第 4 节。

## 3. 集成 AAR

将发布包中的 AAR 放入应用模块：

```text
app/libs/EgoLowBle-1.1.5.aar
```

在应用模块的 `build.gradle.kts` 中添加依赖：

```kotlin
dependencies {
    implementation(files("libs/EgoLowBle-1.1.5.aar"))
}
```

建议的 Android 配置：

```kotlin
android {
    compileSdk = 36

    defaultConfig {
        minSdk = 24
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}
```

如果应用限制 ABI，至少保留 AAR 实际提供的一个 ABI：

```kotlin
android {
    defaultConfig {
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }
}
```

## 4. 混淆和压缩

1.1.5 AAR 内置的 `proguard.txt` 为空，仅包含说明注释。若应用开启 `minifyEnabled = true`，建议在应用侧添加：

```proguard
-keep class com.ego.egolowble.** { *; }
-keep class org.simpleble.android.bridge.** { *; }
-keepclasseswithmembernames class * {
    native <methods>;
}
```

如果应用重新打包或裁剪了 AAR 内容，应确认 APK/AAB 中仍同时包含：

```text
libEgoLowBle.so
libEgoLowBleJni.so
com.ego.egolowble.*
org.simpleble.android.bridge.*
```

## 5. Android 权限和系统开关

1.1.5 AAR manifest 实际声明：

```xml
<uses-feature
    android:name="android.hardware.bluetooth_le"
    android:required="false" />

<uses-permission android:name="android.permission.BLUETOOTH" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
<uses-permission
    android:name="android.permission.BLUETOOTH_SCAN"
    android:usesPermissionFlags="neverForLocation" />
```

1.1.5 AAR 不声明 `ACCESS_FINE_LOCATION`。如果应用需要支持 Android 11 及以下的 BLE 扫描，应用自己的 manifest 需要补充：

```xml
<uses-permission
    android:name="android.permission.ACCESS_FINE_LOCATION"
    android:maxSdkVersion="30" />
```

运行时权限规则：

```text
Android 12 及以上:
  请求 BLUETOOTH_SCAN
  请求 BLUETOOTH_CONNECT

Android 11 及以下:
  请求 ACCESS_FINE_LOCATION
  确认系统定位服务已开启
```

1.1.5 不提供 `EgoLowBlePermissions` helper，应用需要自行处理权限。例如：

```kotlin
private fun requiredBlePermissions(): Array<String> {
    return if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
        arrayOf(
            android.Manifest.permission.BLUETOOTH_SCAN,
            android.Manifest.permission.BLUETOOTH_CONNECT
        )
    } else {
        arrayOf(android.Manifest.permission.ACCESS_FINE_LOCATION)
    }
}
```

扫描前还应检查 BLE feature：

```kotlin
val hasBle = packageManager.hasSystemFeature(
    android.content.pm.PackageManager.FEATURE_BLUETOOTH_LE
)
```

扫描前还应检查系统蓝牙是否已开启。不要静默启用蓝牙，应通过系统界面让用户开启。

`BLUETOOTH_SCAN` 在 AAR 中带有 `neverForLocation`。如果应用确实使用 BLE 推导物理位置，或目标广播受该标记影响，应由应用侧根据 Android 权限政策覆盖 manifest 声明并申请相应位置权限。

## 6. 线程模型和生命周期

`EgoLowBleNative` 的 BLE 方法是同步阻塞调用，不应在主线程执行。建议使用 `Dispatchers.IO`、单线程 `ExecutorService` 或应用已有的后台任务框架。

同一个 native handle 建议只由一个串行任务使用。底层 C API 会在 session 内部串行化调用，但并发业务流程仍可能互相等待，并且可能改变连接状态。

标准生命周期：

```text
nativeCreate()
  -> scan/connect/read/write/configureWifi/requestIp
  -> nativeDisconnect()
  -> nativeDestroy()
```

`nativeCreate()` 返回 `0L` 表示创建 session 失败。所有后续调用都必须使用有效 handle。业务结束时必须调用 `nativeDestroy()`，不要依赖 GC 释放 native 资源。

普通 AAR 使用方不需要手动调用 `EgoLowBle_Android_InitializeJavaVM()`。AAR 内的 `libEgoLowBleJni.so` 会在 `JNI_OnLoad()` 中完成 Android JVM 初始化。

## 7. EgoLowBleNative API

1.1.5 AAR 暴露的 Kotlin 对象：

```kotlin
object EgoLowBleNative {
    const val STATUS_OK = 0
    const val STATUS_INVALID_ARGUMENT = 1
    const val STATUS_ADAPTER_NOT_FOUND = 2
    const val STATUS_DEVICE_NOT_FOUND = 3
    const val STATUS_NOT_CONNECTED = 4
    const val STATUS_BUFFER_TOO_SMALL = 5
    const val STATUS_BLUETOOTH_UNAVAILABLE = 6
    const val STATUS_OPERATION_FAILED = 7

    const val IP_RESULT_SUCCESS = 0
    const val IP_RESULT_NOT_CONFIGURED = 1
    const val IP_RESULT_CONFIGURING = 2
    const val IP_RESULT_CONFIGURE_FAILED = 3

    external fun nativeCreate(): Long
    external fun nativeDestroy(nativeHandle: Long)
    external fun nativeScanDevices(nativeHandle: Long, timeoutMs: Int): Array<EgoLowBleScanDevice>?
    external fun nativeConnectByName(nativeHandle: Long, deviceName: String, timeoutMs: Int): Int
    external fun nativeConnectByAddress(nativeHandle: Long, deviceAddress: String, timeoutMs: Int): Int
    external fun nativeDisconnect(nativeHandle: Long): Int
    external fun nativeGetMtu(nativeHandle: Long): Int
    external fun nativeRead(nativeHandle: Long, serviceUuid: String, characteristicUuid: String): ByteArray?
    external fun nativeWriteRequest(nativeHandle: Long, serviceUuid: String, characteristicUuid: String, payload: ByteArray): Int
    external fun nativeWriteCommand(nativeHandle: Long, serviceUuid: String, characteristicUuid: String, payload: ByteArray): Int
    external fun nativeConfigureWifi(nativeHandle: Long, ssid: String, password: String, timeoutMs: Int): EgoLowBleWifiConfigResponse?
    external fun nativeRequestIp(nativeHandle: Long, timeoutMs: Int): EgoLowBleIpResponse?
    external fun nativeGetLastError(nativeHandle: Long): String
}
```

数据类：

```kotlin
data class EgoLowBleScanDevice(
    val deviceName: String,
    val deviceAddress: String,
    val rssi: Int,
    val isConnectable: Boolean
)

data class EgoLowBleWifiConfigResponse(
    val result: Int,
    val reason: String
)

data class EgoLowBleIpResponse(
    val result: Int,
    val ip: String,
    val reason: String
)
```

## 8. 状态码和错误文本

Kotlin 层只声明了 `0..7` 的状态常量。发布包头文件 `egolowble.h` 还定义了：

```text
STATUS_TIMEOUT        = 8
STATUS_PROTOCOL_ERROR = 9
STATUS_REMOTE_ERROR   = 10
STATUS_NOT_CONFIGURED = 11
```

JNI 方法返回的是底层 C API 的整数状态，因此业务代码应能处理未在 Kotlin 对象中声明的数值。

应用侧可自定义状态文本：

```kotlin
private fun egoLowBleStatusName(status: Int): String = when (status) {
    EgoLowBleNative.STATUS_OK -> "OK"
    EgoLowBleNative.STATUS_INVALID_ARGUMENT -> "INVALID_ARGUMENT"
    EgoLowBleNative.STATUS_ADAPTER_NOT_FOUND -> "ADAPTER_NOT_FOUND"
    EgoLowBleNative.STATUS_DEVICE_NOT_FOUND -> "DEVICE_NOT_FOUND"
    EgoLowBleNative.STATUS_NOT_CONNECTED -> "NOT_CONNECTED"
    EgoLowBleNative.STATUS_BUFFER_TOO_SMALL -> "BUFFER_TOO_SMALL"
    EgoLowBleNative.STATUS_BLUETOOTH_UNAVAILABLE -> "BLUETOOTH_UNAVAILABLE"
    EgoLowBleNative.STATUS_OPERATION_FAILED -> "OPERATION_FAILED"
    8 -> "TIMEOUT"
    9 -> "PROTOCOL_ERROR"
    10 -> "REMOTE_ERROR"
    11 -> "NOT_CONFIGURED"
    else -> "UNKNOWN($status)"
}
```

错误文本读取规则：

- `nativeConnectByName()`、`nativeConnectByAddress()`、`nativeDisconnect()`、`nativeWriteRequest()`、`nativeWriteCommand()` 返回非 0 状态时，立即调用 `nativeGetLastError(handle)`。
- `nativeScanDevices()` 返回 `null` 表示失败，立即调用 `nativeGetLastError(handle)`。
- `nativeGetMtu()` 返回 `-1` 表示失败，立即调用 `nativeGetLastError(handle)`。
- `nativeRead()` 返回 `null` 表示失败，返回空 `ByteArray` 表示读成功但数据为空。
- `nativeConfigureWifi()` 和 `nativeRequestIp()` 返回 `null` 表示 BLE 传输、协议解析或超时失败，立即调用 `nativeGetLastError(handle)`。
- 后续调用可能覆盖错误文本，失败后应立即读取。

## 9. 扫描 BLE 设备

接口：

```kotlin
val devices: Array<EgoLowBleScanDevice>? =
    EgoLowBleNative.nativeScanDevices(handle, timeoutMs)
```

`timeoutMs <= 0` 时，底层使用默认扫描时间 `5000ms`。

Android 后端的扫描过滤规则：

- 启动扫描时优先使用固定 Service UUID 过滤：

```text
0000ffe0-0000-1000-8000-00805f9b34fb
```

- 如果系统拒绝带 UUID 的扫描入口，桥接层会回退到无过滤扫描。
- 最终返回列表仍会保留满足以下任一条件的设备：
  - 设备名称以大小写敏感的 `Ego` 开头。
  - 广播数据或 scan response 中包含固定 FFE0 Service UUID。

因此目标设备建议同时满足：

```text
广播名称以 Ego 开头
广播或 scan response 包含 0000ffe0-0000-1000-8000-00805f9b34fb
```

扫描示例：

```kotlin
val devices = EgoLowBleNative.nativeScanDevices(handle, 5000)
if (devices == null) {
    val error = EgoLowBleNative.nativeGetLastError(handle)
    error("BLE scan failed: $error")
}

val candidates = devices
    .filter { it.isConnectable }
    .sortedByDescending { it.rssi }
```

注意：

- 空数组表示扫描成功但没有找到符合过滤条件的设备。
- 地址应优先来自刚完成的扫描结果，不建议长期持久化未经验证的 Android BLE 地址。
- 扫描为空时优先检查权限、定位服务、蓝牙开关、广播内容和 `neverForLocation` 影响。

## 10. 连接和断开

按地址连接：

```kotlin
val status = EgoLowBleNative.nativeConnectByAddress(
    handle,
    selected.deviceAddress,
    5000
)
if (status != EgoLowBleNative.STATUS_OK) {
    val error = EgoLowBleNative.nativeGetLastError(handle)
    error("connect failed: ${egoLowBleStatusName(status)} $error")
}
```

按名称连接：

```kotlin
val status = EgoLowBleNative.nativeConnectByName(
    handle,
    "Ego-Device",
    5000
)
```

连接规则：

- 连接接口会扫描并选择匹配条件下 RSSI 最强的可连接设备。
- `timeoutMs` 是查找目标设备时使用的扫描时间，不是严格的底层 GATT 连接总超时。
- 同一 session 会缓存最近一次非空扫描结果约 30 秒，连接时会优先复用该缓存。
- 同一 handle 上如果已有连接，连接新设备前会先断开旧连接。

断开连接：

```kotlin
val status = EgoLowBleNative.nativeDisconnect(handle)
if (status != EgoLowBleNative.STATUS_OK) {
    val error = EgoLowBleNative.nativeGetLastError(handle)
    error("disconnect failed: ${egoLowBleStatusName(status)} $error")
}
```

## 11. 获取 MTU

接口：

```kotlin
val mtu = EgoLowBleNative.nativeGetMtu(handle)
if (mtu <= 0) {
    val error = EgoLowBleNative.nativeGetLastError(handle)
    error("get MTU failed: $error")
}
```

建议在连接成功后调用 `nativeGetMtu()` 验证 GATT 链路可用。

## 12. GATT 读写

读取 characteristic：

```kotlin
val data = EgoLowBleNative.nativeRead(
    handle,
    "0000ffe0-0000-1000-8000-00805f9b34fb",
    "0000ff03-0000-1000-8000-00805f9b34fb"
)
if (data == null) {
    val error = EgoLowBleNative.nativeGetLastError(handle)
    error("read failed: $error")
}
```

使用 Write Request 写入：

```kotlin
val status = EgoLowBleNative.nativeWriteRequest(
    handle,
    serviceUuid,
    characteristicUuid,
    payload
)
```

使用 Write Command 写入：

```kotlin
val status = EgoLowBleNative.nativeWriteCommand(
    handle,
    serviceUuid,
    characteristicUuid,
    payload
)
```

写入失败处理：

```kotlin
if (status != EgoLowBleNative.STATUS_OK) {
    val error = EgoLowBleNative.nativeGetLastError(handle)
    error("write failed: ${egoLowBleStatusName(status)} $error")
}
```

UUID 应使用完整规范格式。写入长度受协商 MTU 和目标 characteristic 属性限制。

## 13. Wi-Fi 配网

接口：

```kotlin
val response = EgoLowBleNative.nativeConfigureWifi(
    handle,
    ssid,
    password,
    10000
)
```

失败处理：

```kotlin
if (response == null) {
    val error = EgoLowBleNative.nativeGetLastError(handle)
    error("configure Wi-Fi transport failed: $error")
}

if (response.result != 0) {
    error("device Wi-Fi configure failed: ${response.reason}")
}
```

固定通道：

```text
service UUID:        0000ffe0-0000-1000-8000-00805f9b34fb
characteristic UUID: 0000ff03-0000-1000-8000-00805f9b34fb
```

请求格式：

```text
"A001" + 32-byte SSID + 32-byte password
```

响应格式：

```text
"A001" + ASCII result byte + 64-byte reason text
```

其中 `result` 字段必须是 ASCII 字符：

```text
'0' 表示成功
'1' 表示失败
```

JNI 返回给 Kotlin 后，`response.result` 会被转换为整数 `0` 或 `1`。设备端不要把该字段实现为二进制 `0x00` 或 `0x01`。

限制和建议：

- SSID 不能为空。
- SSID 最大 32 字节。
- password 最大 32 字节，空字符串可用于开放网络。
- 字段长度按编码后的字节数计算，不按 Kotlin 字符数计算。
- 为避免设备端编码差异，优先使用设备协议明确支持的 ASCII 或 UTF-8 内容。
- 日志中不要输出明文 Wi-Fi 密码。
- 配网响应成功只表示设备接受并处理了配网请求，不等于设备已经获取 IP。

## 14. 查询设备 IP

接口：

```kotlin
val response = EgoLowBleNative.nativeRequestIp(handle, 10000)
```

失败处理：

```kotlin
if (response == null) {
    val error = EgoLowBleNative.nativeGetLastError(handle)
    error("request IP transport failed: $error")
}
```

设备结果码：

```text
IP_RESULT_SUCCESS          = 0
IP_RESULT_NOT_CONFIGURED   = 1
IP_RESULT_CONFIGURING      = 2
IP_RESULT_CONFIGURE_FAILED = 3
```

固定请求格式：

```text
request:  "A002"
response: "A002" + ASCII result byte + 32-byte text
```

`result` 字段必须是 ASCII 字符：

```text
'0' success
'1' not configured
'2' configuring
'3' configure failed
```

JNI 返回给 Kotlin 后，`response.result` 会被转换为整数 `0..3`。设备端不要把该字段实现为二进制 `0x00..0x03`。

成功条件：

```kotlin
response.result == EgoLowBleNative.IP_RESULT_SUCCESS &&
    response.ip.isNotBlank()
```

轮询示例：

```kotlin
var deviceIp: String? = null

for (attempt in 1..30) {
    val response = EgoLowBleNative.nativeRequestIp(handle, 1000)
        ?: error("request IP failed: ${EgoLowBleNative.nativeGetLastError(handle)}")

    when (response.result) {
        EgoLowBleNative.IP_RESULT_SUCCESS -> {
            if (response.ip.isNotBlank()) {
                deviceIp = response.ip
                break
            }
        }
        EgoLowBleNative.IP_RESULT_NOT_CONFIGURED -> {
            error("device Wi-Fi is not configured: ${response.reason}")
        }
        EgoLowBleNative.IP_RESULT_CONFIGURING -> {
            // Continue polling.
        }
        EgoLowBleNative.IP_RESULT_CONFIGURE_FAILED -> {
            error("device Wi-Fi configure failed: ${response.reason}")
        }
        else -> {
            error("unknown IP result: ${response.result}, reason=${response.reason}")
        }
    }

    if (attempt < 30) {
        kotlinx.coroutines.delay(1000)
    }
}
```

## 15. 完整配网流程示例

下面示例展示应用侧如何自行封装 1.1.5 的 native handle。该类不是 AAR 自带类。

```kotlin
class EgoLowBle115Session : java.io.Closeable {
    private var handle: Long = EgoLowBleNative.nativeCreate()

    init {
        check(handle != 0L) { "Failed to create EgoLowBle native session." }
    }

    suspend fun provision(ssid: String, password: String): String =
        kotlinx.coroutines.withContext(kotlinx.coroutines.Dispatchers.IO) {
            try {
                val devices = EgoLowBleNative.nativeScanDevices(handle, 5000)
                    ?: error("scan failed: ${EgoLowBleNative.nativeGetLastError(handle)}")

                val target = devices
                    .filter { it.isConnectable }
                    .filter { it.deviceName.startsWith("Ego") }
                    .maxByOrNull { it.rssi }
                    ?: error("no connectable Ego BLE device found")

                val connectStatus = EgoLowBleNative.nativeConnectByAddress(
                    handle,
                    target.deviceAddress,
                    5000
                )
                check(connectStatus == EgoLowBleNative.STATUS_OK) {
                    "connect failed: ${egoLowBleStatusName(connectStatus)} " +
                        EgoLowBleNative.nativeGetLastError(handle)
                }

                val mtu = EgoLowBleNative.nativeGetMtu(handle)
                check(mtu > 0) {
                    "get MTU failed: ${EgoLowBleNative.nativeGetLastError(handle)}"
                }

                val wifi = EgoLowBleNative.nativeConfigureWifi(handle, ssid, password, 15000)
                    ?: error("configure Wi-Fi failed: ${EgoLowBleNative.nativeGetLastError(handle)}")
                check(wifi.result == 0) {
                    "device rejected Wi-Fi config: ${wifi.reason}"
                }

                for (attempt in 1..30) {
                    val ip = EgoLowBleNative.nativeRequestIp(handle, 1000)
                        ?: error("request IP failed: ${EgoLowBleNative.nativeGetLastError(handle)}")

                    when (ip.result) {
                        EgoLowBleNative.IP_RESULT_SUCCESS -> {
                            if (ip.ip.isNotBlank()) {
                                return@withContext ip.ip
                            }
                        }
                        EgoLowBleNative.IP_RESULT_NOT_CONFIGURED -> {
                            error("device Wi-Fi is not configured: ${ip.reason}")
                        }
                        EgoLowBleNative.IP_RESULT_CONFIGURE_FAILED -> {
                            error("device Wi-Fi configure failed: ${ip.reason}")
                        }
                    }

                    if (attempt < 30) {
                        kotlinx.coroutines.delay(1000)
                    }
                }

                error("waiting for device IP timed out")
            } finally {
                EgoLowBleNative.nativeDisconnect(handle)
            }
        }

    override fun close() {
        val current = handle
        handle = 0L
        if (current != 0L) {
            EgoLowBleNative.nativeDestroy(current)
        }
    }
}
```

## 16. 直接使用 C API

发布包提供 `include/EgoLowBle/egolowble.h`。纯原生工程或需要扩展 JNI 时，可以直接使用 C API。

C API 比 1.1.5 Kotlin/JNI 层更完整，包含：

- `EgoLowBle_IsConnected`
- `EgoLowBle_ListCharacteristics`
- `EgoLowBle_ProtocolConfigureChannel`
- `EgoLowBle_ProtocolTransceive`
- `EgoLowBle_WaitForNotification`
- `EgoLowBle_WriteRequestAndWaitForNotification`
- `EgoLowBle_WriteCommandAndWaitForNotification`
- `EgoLowBle_WriteRequestAndPollRead`

在 Android 自定义 JNI 中使用 C API 时，需要在 `JNI_OnLoad()` 中调用：

```cpp
EgoLowBle_Android_InitializeJavaVM(vm);
```

AAR 自带的 `libEgoLowBleJni.so` 已经做了这一步，普通 Kotlin 调用方不需要重复处理。

## 17. 常见问题排查

### 17.1 UnsatisfiedLinkError

检查：

- APK/AAB 是否包含 `libEgoLowBle.so` 和 `libEgoLowBleJni.so`。
- 应用 ABI 是否包含 `arm64-v8a` 或 `armeabi-v7a`。
- 是否在 x86/x86_64 模拟器上运行。
- R8/打包配置是否裁剪了 `com.ego.egolowble.*` 或 `org.simpleble.android.bridge.*`。

### 17.2 扫描结果为空

检查：

- 系统蓝牙是否开启。
- Android 12+ 是否已授予 `BLUETOOTH_SCAN` 和 `BLUETOOTH_CONNECT`。
- Android 11 及以下是否已声明并授予 `ACCESS_FINE_LOCATION`。
- Android 11 及以下系统定位服务是否开启。
- 设备是否正在发送 BLE 广播。
- 设备名称是否以大小写敏感的 `Ego` 开头。
- 广播或 scan response 是否包含固定 FFE0 Service UUID。
- `neverForLocation` 是否影响了目标广播。
- 扫描窗口是否覆盖设备广播周期。

### 17.3 能扫描但无法连接

检查：

- 目标设备是否仍在广播。
- 目标设备是否已经被其他主机连接。
- 是否使用刚完成扫描返回的地址。
- 设备地址是否因 RPA 发生变化。
- 扫描结果中的 `isConnectable` 是否为 `true`。
- 目标设备是否提供可访问的 GATT service。

### 17.4 Wi-Fi 配网失败或超时

检查：

- FFE0 service 和 FF03 characteristic 是否存在。
- FF03 是否支持 Write Request 和读取。
- 设备是否返回固定长度 A001 响应。
- A001 响应 result 字段是否为 ASCII `'0'` 或 `'1'`。
- SSID 和 password 是否按字节计算超过 32 字节。
- 设备是否支持目标路由器频段和认证方式。

### 17.5 IP 一直处于 configuring

检查：

- 不要只判断 JNI 返回是否为非空，还要检查 `response.result`。
- A002 响应 result 字段是否为 ASCII `'0'..'3'`。
- 设备是否成功连接路由器。
- DHCP 是否成功。
- 单次 `nativeRequestIp()` 超时和整体等待超时是否分开设置。
- 不要在设备仍需通过 BLE 返回状态时提前断开 BLE。

## 18. 集成要点

- 本文只描述 `EgoLowBle-android-1.1.5` 发布 AAR 的实际能力。
- 1.1.5 AAR 不包含 `EgoLowBleClient` 和 `EgoLowBlePermissions`。
- 应用需要自行管理 `nativeCreate()` 返回的 handle，并最终调用 `nativeDestroy()`。
- BLE 调用是同步阻塞调用，应放在后台线程执行。
- Android 11 及以下的 `ACCESS_FINE_LOCATION` 需要应用侧声明和请求。
- AAR 仅提供 `arm64-v8a` 和 `armeabi-v7a`。
- 扫描结果会按 Android 后端规则过滤，目标设备建议广播 `Ego` 前缀和 FFE0 Service UUID。
- 配网和 IP 查询的 result 字段在设备协议中是 ASCII 数字字符，JNI 返回后才是整数。
- 失败后立即读取 `nativeGetLastError(handle)`。
- 开启混淆时建议应用侧添加 keep 规则。
