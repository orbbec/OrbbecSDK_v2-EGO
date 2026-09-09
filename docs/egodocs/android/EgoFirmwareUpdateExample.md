# EGO 固件升级使用说明（Android Java）

## 功能概述

`EgoFirmwareUpdateActivity` 演示 EGO 设备的完整固件升级流程：校验固件 BIN、将正常 EGO 切换到恢复模式、等待 EgoBoot 重新枚举，再调用升级接口写入固件。

升级页的操作顺序如下：

1. 选择 EGO 固件 BIN。
2. 连接正常 EGO 设备后，进入恢复模式。
3. 等待 EgoBoot 设备重新枚举并完成 USB 授权。
4. 开始固件升级。

## 固件文件目录与权限

请将 EGO 固件文件（`.bin`）放入以下目录：

```text
/storage/emulated/0/Orbbec
```

首次点击 **Select EGO Firmware BIN** 时，示例会打开 Android 的“所有文件访问权限”页面。请为 `OrbbecSdkJavaExamples` 开启该权限；授权完成后，页面会自动列出 `Orbbec` 目录内的 `.bin` 文件。

集成该示例时，应用 Manifest 需要声明以下权限：

```xml
<uses-permission android:name="android.permission.MANAGE_EXTERNAL_STORAGE" />
```

Android 11 及以上系统对公共存储目录实行分区存储限制。未授予该权限时，即使 BIN 文件已存在，应用也无法直接枚举该目录。

## 固件升级操作步骤

### 1. 选择并校验 BIN

打开 `EgoFirmwareUpdateActivity` 后，初始状态只有 **Select EGO Firmware BIN** 可以操作。

选择文件后，示例会在后台执行 BIN 校验。校验通过后会显示 payload 和传输数据长度；校验失败时会显示原因，不能进入恢复模式或开始升级。

校验由 `EgoFirmwareBinValidator` 完成，校验规则与 SDK C++ EGO BIN 校验逻辑一致，包括：

- 固件尾部 MD5；
- 固件脚本格式；
- 分段下载地址与长度范围；
- 总下载长度。

### 2. 连接正常 EGO 并进入恢复模式

连接正常 EGO 设备。正常设备的 USB 标识为：

```text
VID:PID = 2BC5:1201 或 2BC5:1203
```

当正常 EGO 已连接且 BIN 校验通过时，**Enter EGO Recovery Mode** 可用。示例会写入：

```java
device.setPropertyValueB(
        DeviceProperty.OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL, true);
```

该命令会使设备立即重启。因此命令调用后可能因 USB 断开而抛出异常；这属于预期行为，不能按普通命令失败处理。示例会释放旧设备句柄并保持等待 EgoBoot 重连的状态。

### 3. 等待 EgoBoot 重连并重新授权

设备进入恢复模式后，VID/PID 会改变为 EgoBoot：

| 阶段 | VID:PID |
| --- | --- |
| EgoBoot ROM | `1B20:0300` |
| EgoBoot U-Boot | `1B20:0301` |

Android 会把 EgoBoot 当作新的 USB 设备。请在系统 USB 授权弹窗中允许应用访问该设备。重新枚举成功后，示例通过设备变更回调获取新的 `Device` 实例，并将页面状态切换为 `BOOT_READY`。

> 不要复用正常 EGO 的 `Device` 实例调用升级接口。必须等待 EgoBoot 枚举后，使用新获取的 EgoBoot `Device`。

### 4. 开始升级

EgoBoot 已就绪且 BIN 已校验通过时，点击 **Start Firmware Update**。示例调用：

```java
bootDevice.upgrade(firmwarePath, (state, percent, message) -> {
    // 根据 state、percent 和 message 更新界面。
});
```

升级期间不要断开 USB、关闭应用或让设备断电。升级完成后，示例会清除已选择的 BIN 和校验结果，并回到初始状态；此时只有 **Select EGO Firmware BIN** 可操作，以便开始下一次升级。

## 关键状态

| 页面状态 | 含义 | 可操作按钮 |
| --- | --- | --- |
| `WAITING_EGO` | 等待正常 EGO，或一次升级已完成 | 选择 BIN |
| `EGO_READY` | 已发现正常 EGO | 选择 BIN；BIN 校验通过后可进入恢复模式 |
| `WAITING_BOOT` | 已发起恢复模式重启，等待重连 | 无 |
| `BOOT_READY` | 已发现并打开 EgoBoot | 选择 BIN；BIN 校验通过后可开始升级 |
| `UPDATING` | 固件正在写入 | 无 |

## 常见问题

### 在 `/storage/emulated/0/Orbbec` 中已有 BIN，但页面未列出

请确认已在系统设置中为 `OrbbecSdkJavaExamples` 开启“所有文件访问权限”。授权后重新进入升级页或再次点击 **Select EGO Firmware BIN**。

### 进入恢复模式后显示异常

正常 EGO 在收到 `OB_PROP_BOOT_INTO_RECOVERY_MODE_BOOL` 后会立即断开并重启，导致调用路径报告 USB/JNI 异常。这是预期的设备切换过程；请等待 `1B20:0300` 或 `1B20:0301` 枚举，并在出现授权提示时允许访问。

### EgoBoot 已显示在系统 USB 列表中，但升级按钮不可用

请依次确认：

- 已对该新 USB 设备授权；
- 页面显示 EgoBoot 已就绪；
- 已重新选择并校验通过 BIN；
- 升级期间未发生设备断连。
