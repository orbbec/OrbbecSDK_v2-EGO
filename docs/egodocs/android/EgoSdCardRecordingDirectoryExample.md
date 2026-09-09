# EGO 自定义目录 SD 卡录制示例

本文说明如何在 Android 应用中，为 EGO 设备设置固件端 SD 卡录制目录，并启动和停止固件录制。

对应的可运行示例位于：

`OrbbecSdkJavaExamples > Ego-FW-Recording-Directory`

## 使用流程

1. 确认设备支持目录属性和录制控制属性。
2. 在未录制状态下设置 SD 卡录制目录。
3. 将录制目标设置为 SD 卡。
4. 发送开始录制动作。
5. 结束时保持录制目标为 SD 卡并发送停止录制动作。

> 录制动作的实际行为取决于录制目标：目标为 SD 卡时，开始/停止动作控制固件录制；目标为主机时，同一动作只会播放设备语音提示。因此开始录制前必须设置 SD 卡目标。

## 目录格式

`setFwRecordingDirectory()` 的参数是设备 SD 卡内的 UTF-8 Linux 风格目录，不是 Android 手机本地文件路径。

```text
正确：raw/session_001
正确：/all_raw_data/20260731/session_001/
错误：C:\recordings\session_001
错误：raw\session_001
错误：raw//session_001
错误：raw/../session_001
```

目录规则：

- 路径分隔符必须为 `/`，不支持反斜杠 `\` 或 Windows 盘符路径；
- 可传入前导或尾部 `/`，设备读回时会移除；
- 路径不能为空，不能包含空路径段、`.`、`..`、控制字符或非法 UTF-8；
- 整个路径最长 480 个 UTF-8 字节，单个路径段最长 255 个 UTF-8 字节；
- 固件正在录制时不能修改目录。

## 完整代码示例

以下代码假定已经成功获取到 `Device device`。建议在后台线程中执行设备 I/O，并在 UI 线程中更新界面。

```java
import android.util.Log;

import com.orbbec.obsensor.Device;
import com.orbbec.obsensor.property.DeviceProperty;
import com.orbbec.obsensor.types.PermissionType;
import com.orbbec.obsensor.types.RecordingAction;
import com.orbbec.obsensor.types.RecordingDestination;

private static final String TAG = "EgoFwRecording";

private static void requireWritable(Device device, DeviceProperty property) {
    if (!device.isPropertySupported(property, PermissionType.OB_PERMISSION_WRITE)) {
        throw new UnsupportedOperationException(property + " is not writable on this device");
    }
}

// 1. 在开始录制前设置目录。目录必须是设备 SD 卡内的 Linux 风格路径。
String directory = "raw/session_001";
requireWritable(device, DeviceProperty.OB_STRUCT_FW_RECORDING_DIRECTORY);
device.setFwRecordingDirectory(directory);

// 可选：读回设备规范化后的目录。前导或尾部 '/' 会在读回时移除。
if (device.isPropertySupported(DeviceProperty.OB_STRUCT_FW_RECORDING_DIRECTORY,
        PermissionType.OB_PERMISSION_READ)) {
    String currentDirectory = device.getFwRecordingDirectory();
    Log.i(TAG, "Firmware recording directory: " + currentDirectory);
}

// 2. 开始录制前，必须将动作目标切换为 SD 卡。
requireWritable(device, DeviceProperty.OB_PROP_RECORDING_DESTINATION_INT);
requireWritable(device, DeviceProperty.OB_PROP_RECORDING_ACTION_INT);
device.setPropertyValueI(DeviceProperty.OB_PROP_RECORDING_DESTINATION_INT,
        RecordingDestination.OB_RECORDING_DESTINATION_SD_CARD.value());
device.executeRecordingAction(RecordingAction.OB_RECORDING_ACTION_START);

// 3. 停止录制。再次设置 SD 卡目标可避免目标被其他代码修改后，STOP 只播放语音提示。
device.setPropertyValueI(DeviceProperty.OB_PROP_RECORDING_DESTINATION_INT,
        RecordingDestination.OB_RECORDING_DESTINATION_SD_CARD.value());
device.executeRecordingAction(RecordingAction.OB_RECORDING_ACTION_STOP);
```

## 异常处理

以上接口在以下情况会抛出 `OBException`：

- 设备或当前固件不支持相应属性；
- 目录格式不合法；
- 固件录制仍处于进行状态时尝试修改目录；
- SD 卡或设备拒绝目录设置、开始录制或停止录制请求；
- 设备在操作过程中断开。

调用前应始终使用 `isPropertySupported()` 检查读写权限，不要仅根据 SDK 版本判断功能是否可用。

## 示例页面说明

示例应用中的 `Ego-FW-Recording-Directory` 页面提供以下操作：

- **Read directory**：读取当前固件录制目录；
- **Set directory**：设置输入框中的目录，并在支持读取时回读规范化结果；
- **Start recording**：设置 SD 卡目标后请求开始固件录制；
- **Stop recording**：设置 SD 卡目标后请求停止固件录制。
