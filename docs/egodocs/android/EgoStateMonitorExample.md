# EGO 状态、录制结果与设备端录制（Android Java）

EGO 设备通过 HID 上报工作状态、状态/错误标志、剩余存储空间，以及最近一次设备端录制时长。Android Java SDK 提供以下彼此职责明确的能力：

| 能力 | Java 接口 | 用途 |
| --- | --- | --- |
| 状态监听 | `setEgoStateCallback(...)` | 持续观察设备状态机，如 `RECORDING`、`SAVING`、`RECORD_SAVED` 和故障标志。 |
| 当前状态查询 | `getEgoState()` | 主动读取一次当前状态；适用于初始展示、用户刷新或诊断，不替代持续监听。 |
| 录制结果监听 | `setEgoRecordingResultCallback(...)` | 在一次录制结束后获得该次结果：状态快照、录制目录和最终分片目录名。此为业务侧获取录制结果的首选接口。 |
| 最近分片查询 | `getFwRecordingPartList()` | 主动读取设备保存的“最近一次”分片列表；用于补查或诊断，不替代结果回调。 |
| 指定目录会话查询 | `getRecordingSessionResult(String)` | 查询指定录制目录的文件是否存在、录制时长（秒）和当前/最终会话状态。 |

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

### 主动读取当前状态

`getEgoState()` 同步向设备读取一次 `EgoStateReport`。它不注册回调，适合页面初始展示、用户主动刷新或诊断；
不能替代 `setEgoStateCallback(...)` 对录制过程的持续监听。应在应用工作线程调用，设备不支持状态服务或查询失败时会抛出 `OBException`。

```java
EgoStateReport state = device.getEgoState();
// 使用 state.getWorkState()、getStorageFreeBytes() 等字段更新页面或记录日志。
```

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

## 3. 主动录制结果查询（辅助接口）

`getFwRecordingPartList()` 是同步主动查询，只读取设备当前保存的“最近一次”分片列表。它不等待下一次录制结束，不注册回调，也不参与回调结果的确认逻辑。

```java
import java.util.List;

List<String> partNames = device.getFwRecordingPartList();
for (String partName : partNames) {
    // 用于用户主动刷新、补查或诊断。
}
```

调用前应确认设备支持只读属性 `OB_RAW_DATA_FW_RECORDING_PART_LIST`。此接口会产生一次额外的设备属性读取；不要在状态回调中轮询调用。已经拿到 `EgoRecordingResult` 时，通常也不应立刻再次调用该接口；只有业务确实需要重新读取“最近一次”结果时才主动查询。

### 指定目录的录制会话结果

`getRecordingSessionResult(String recordingTaskDirectory)` 按录制开始时设置的设备端目录查询该会话。
返回的 `RecordingSessionResult` 包含 `isFileExists()`、`getRecordingDuration()`（秒）、
`getRecordingStatus()` 和 `getRawRecordingStatus()`；状态可能为 `NOT_STARTED`、`RECORDING`、`STOPPED`、
`ABNORMAL` 或 `NOT_FOUND`。

| `RecordingSessionStatus` | raw 值 | 含义 |
| --- | ---: | --- |
| `NOT_STARTED` | 0 | 会话已创建但尚未开始录制。 |
| `RECORDING` | 1 | 正在录制、停止处理中或正在保存最终文件。 |
| `STOPPED` | 2 | 录制已正常完成。 |
| `ABNORMAL` | 3 | 录制被中断或异常终止。 |
| `NOT_FOUND` | 4 | 未找到该录制目录对应的会话。 |

| `RecordingSessionResult` 方法 | 含义 |
| --- | --- |
| `isFileExists()` | 查询时设备端是否存在对应录制文件；不能单独用它判断录制是否正常完成。 |
| `getRecordingDuration()` | 录制时长，单位为秒；不要与 `EgoStateReport.getRecordingDurationMs()` 的毫秒单位混用。 |
| `getRecordingStatus()` | 类型化会话状态；`STOPPED` 表示正常完成，`ABNORMAL` 表示异常终止。 |
| `getRawRecordingStatus()` | 状态原始整数值，用于日志和协议联调。 |

目录必须使用设置录制目录时的最终值（推荐使用设备回读的规范化目录），不能传 Android 本地路径。目录不能为空，
应为有效 UTF-8，最长 `480` 字节，单段最长 `255` 字节；不允许空路径段、`.` / `..`、反斜杠、控制字符或内嵌空字符。
此接口是同步查询，不注册回调也不等待录制完成，必须在应用工作线程调用。当前核心 SDK 仅在 Wrist / Wrist 网络设备且固件版本
大于 `10102` 时支持此能力；其他设备或固件、通信失败或路径格式错误会抛出 `OBException`。`null`、空字符串或内嵌
`\0` 在 Java 侧会抛出 `IllegalArgumentException`。

```java
import com.orbbec.obsensor.types.RecordingSessionResult;

RecordingSessionResult result = device.getRecordingSessionResult(recordingDirectory);
// result.getRecordingDuration() 的单位为秒。
// result.getRecordingStatus() 为当前或最终会话状态。
```

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
6. 有补查需求时，再由用户主动调用 `getFwRecordingPartList()` 或按录制目录调用
   `getRecordingSessionResult(recordingDirectory)`。

不再使用设备时，应先停止业务逻辑，再清理两个回调：

```java
device.clearEgoRecordingResultCallback();
device.clearEgoStateCallback();
device.close();
```

## 5. Sample 中的 UI 与日志

`EgoFwRecordingDirectoryActivity` 将五类信息分区显示：

| UI 区域 | 内容 | 日志标签 |
| --- | --- | --- |
| EGO State | 完整状态报告、标志、存储空间、录制时长和 `RECORD_SAVED` 提示 | `EGO state report` |
| Recording result callback test | 录制结果回调的状态、目录、分片及有效标志 | `EGO recording-result callback` |
| Latest part-list query test | 点击 `Read latest recording part list` 后的主动查询结果 | `EGO recording part-list query` |
| Synchronous device query test / Get EGO state | 点击后主动读取并展示一次完整 `EgoStateReport` | `EGO state query` |
| Synchronous device query test / Get session result | 使用页面当前录制目录查询文件、时长和会话状态 | `Recording session result query` |

录制结果回调与主动分片查询分别展示、分别记录日志；后者不是前者的回退或验证步骤。
