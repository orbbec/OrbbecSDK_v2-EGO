# EGO 状态、录制结果与设备端录制（Android Java）

EGO 设备通过 HID 上报工作状态、状态/错误标志、剩余存储空间，以及最近一次设备端录制时长。Android Java SDK 提供三项彼此职责明确的能力：

| 能力 | Java 接口 | 用途 |
| --- | --- | --- |
| 状态监听 | `setEgoStateCallback(...)` | 持续观察设备状态机，如 `RECORDING`、`SAVING`、`RECORD_SAVED` 和故障标志。 |
| 录制结果监听 | `setEgoRecordingResultCallback(...)` | 在一次录制结束后获得该次结果：状态快照、录制目录和最终分片目录名。此为业务侧获取录制结果的首选接口。 |
| 最近分片查询 | `getFwRecordingPartList()` | 主动读取设备保存的“最近一次”分片列表；用于补查或诊断，不替代结果回调。 |

本文对应 `OrbbecSdkJavaExamples > Ego-FW-Recording-Directory`。该页面在发送 START 前注册状态与录制结果回调，分别展示状态、结果回调和手动查询结果，并输出对应日志。

## 1. 状态回调

状态回调用于处理录制过程和设备健康状态。`RECORD_SAVED` 表示设备端文件已保存成功；`STOPPING_RECORD` 与 `SAVING` 仍是过程状态。

```java
import com.orbbec.obsensor.Device;
import com.orbbec.obsensor.types.EgoStateCallbackThread;
import com.orbbec.obsensor.types.EgoStateReport;
import com.orbbec.obsensor.types.EgoWorkState;

private void startStateMonitor(Device device) {
    device.setEgoStateCallback(this::onEgoState,
            EgoStateCallbackThread.SDK_WORKER);
}

private void onEgoState(EgoStateReport state) {
    if (state.getWorkState() == EgoWorkState.RECORD_SAVED) {
        if (state.isRecordingDurationValid()) {
            long durationMs = state.getRecordingDurationMs();
            // 显示或记录本次录制时长。
        }
        // 文件已保存成功。
    }
}
```

仅更新 Android UI 时可选 `EgoStateCallbackThread.MAIN`。回调不在底层 HID 接收线程直接执行；耗时操作仍应交给应用自身的工作线程。

### `EgoStateReport` 关键字段

| 方法 | 含义 |
| --- | --- |
| `getProtocolVersion()` | HID 状态报告协议版本。 |
| `getSequence()` | 状态报告序列号，可用于诊断状态是否持续到达。 |
| `getWorkState()` / `getRawWorkState()` | 类型化工作状态 / 固件原始状态值。应记录未知原始值。 |
| `getStateFlags()` / `hasStateFlag(...)` | 当前设备状态标志集合。 |
| `getErrorFlags()` / `hasErrorFlag(...)` | 当前错误标志集合。 |
| `getStorageFreeBytes()` | 剩余存储空间，单位字节。 |
| `getRecordingDurationMs()` | 最近一次设备端录制时长，单位毫秒。 |
| `isRecordingDurationValid()` | 时长是否有效；不能以时长是否为 0 替代此判断。 |

工作状态 `10` 为 `EgoWorkState.RECORD_SAVED`。状态和错误标志均为位集合，应使用 `hasStateFlag(...)` / `hasErrorFlag(...)` 判断，不能用相等比较判断单个标志。

## 2. 录制结果回调（推荐）

`EgoRecordingResultCallback` 在设备报告可获取录制结果时被调用。`EgoRecordingResult` 是不可变对象，包含：

- `getState()`：触发结果的 `EgoStateReport` 快照；
- `getRecordingDirectory()` 与 `isDirectoryValid()`：录制目录及其有效性；
- `getPartNames()` 与 `isPartListValid()`：最终分片目录名列表及其有效性。

```java
import com.orbbec.obsensor.Device;
import com.orbbec.obsensor.EgoRecordingResultCallback;
import com.orbbec.obsensor.types.EgoRecordingResult;
import com.orbbec.obsensor.types.EgoRecordingResultCallbackThread;

private void startRecordingResultMonitor(Device device) {
    device.setEgoRecordingResultCallback(this::onRecordingResult,
            EgoRecordingResultCallbackThread.MAIN);
}

private void onRecordingResult(EgoRecordingResult result) {
    if (result.isDirectoryValid()) {
        String directory = result.getRecordingDirectory();
        // 显示本次录制目录。
    }
    if (result.isPartListValid()) {
        for (String partName : result.getPartNames()) {
            // 显示本次最终分片目录名。
        }
    }
}
```

传入 `null` 回调等价于清理；也可显式调用 `clearEgoRecordingResultCallback()`。`Device.close()` 会清理状态回调和录制结果回调。设备重新连接后，必须对新的 `Device` 实例重新注册。

### 终态与网络读取

录制结果回调由状态报告触发。SDK 在收到“录制时长有效”且状态为 `SAVING`、`RECORD_SAVED`、`FAULT`，或带有 `RECORD_ABORTED` 错误标志的报告时，为该序列号创建一次结果任务并去重。

为填充结果中的目录和分片列表，SDK 会在其结果工作线程中读取：

1. `OB_STRUCT_FW_RECORDING_DIRECTORY`；
2. `OB_RAW_DATA_FW_RECORDING_PART_LIST`。

网络设备上这两次内部属性读取会走 TCP 控制通道。因此，X 键或软件 STOP 触发保存终态后，可能紧接着看到这两次 TCP 控制请求；这属于录制结果回调的正常数据获取流程。若读取失败，回调仍会返回状态快照，但相应的 `directoryValid` 或 `partListValid` 为 `false`。这不表示录制本身必然失败：应以 `result.getState().getWorkState()`、错误标志和有效标志共同判断，不能把空字符串或空列表直接当作成功结果。

## 3. 最近分片列表查询（辅助接口）

`getFwRecordingPartList()` 是同步主动查询，只读取设备当前保存的“最近一次”分片列表。它不等待下一次录制结束，不注册回调，也不参与回调结果的确认逻辑。

```java
import java.util.List;

List<String> partNames = device.getFwRecordingPartList();
for (String partName : partNames) {
    // 用于用户主动刷新、补查或诊断。
}
```

调用前应确认设备支持只读属性 `OB_RAW_DATA_FW_RECORDING_PART_LIST`。此接口会产生一次额外的设备属性读取；不要在状态回调中轮询调用。已经拿到 `EgoRecordingResult` 时，通常也不应立刻再次调用该接口；只有业务确实需要重新读取“最近一次”结果时才主动查询。

## 4. 软件录制流程

录制目标必须为 SD 卡；目标为 `HOST` 时，START/STOP 是设备语音提示控制，不控制 SD 卡录制。

```java
import com.orbbec.obsensor.property.DeviceProperty;
import com.orbbec.obsensor.types.RecordingAction;
import com.orbbec.obsensor.types.RecordingDestination;

device.setPropertyValueI(DeviceProperty.OB_PROP_RECORDING_DESTINATION_INT,
        RecordingDestination.OB_RECORDING_DESTINATION_SD_CARD.value());
device.executeRecordingAction(RecordingAction.OB_RECORDING_ACTION_START);

// 停止时同样先选择 SD 卡目标。
device.setPropertyValueI(DeviceProperty.OB_PROP_RECORDING_DESTINATION_INT,
        RecordingDestination.OB_RECORDING_DESTINATION_SD_CARD.value());
device.executeRecordingAction(RecordingAction.OB_RECORDING_ACTION_STOP);
```

推荐顺序：

1. 检查录制相关属性及 TF 卡状态；
2. 注册状态回调和录制结果回调；
3. 设置 `OB_PROP_RECORDING_DESTINATION_INT = SD_CARD`；
4. 发送 START；
5. 通过状态回调观察录制过程，通过录制结果回调获取最终文件结果；
6. 有补查需求时，再由用户主动调用 `getFwRecordingPartList()`。

不再使用设备时，应先停止业务逻辑，再清理两个回调：

```java
device.clearEgoRecordingResultCallback();
device.clearEgoStateCallback();
device.close();
```

## 5. Sample 中的 UI 与日志

`EgoFwRecordingDirectoryActivity` 将三类信息分区显示：

| UI 区域 | 内容 | 日志标签 |
| --- | --- | --- |
| EGO State | 完整状态报告、标志、存储空间、录制时长和 `RECORD_SAVED` 提示 | `EGO state report` |
| Recording result callback test | 录制结果回调的状态、目录、分片及有效标志 | `EGO recording-result callback` |
| Latest part-list query test | 点击 `Read latest recording part list` 后的主动查询结果 | `EGO recording part-list query` |

录制结果回调与主动分片查询分别展示、分别记录日志；后者不是前者的回退或验证步骤。
