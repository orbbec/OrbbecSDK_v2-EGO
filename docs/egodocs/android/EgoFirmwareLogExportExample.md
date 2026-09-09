# EGO 固件日志导出示例

适用于 Android EGO SDK。

本文说明如何将 EGO 设备的固件日志导出到 Android 应用专属外部存储目录。示例使用 `Device.exportFirmwareLogs()` 异步导出，进度、成功和失败回调均在主线程执行，可直接更新界面。

## 导出目录

建议每次导出使用一个新的时间戳子目录，避免重复导出时同名日志文件互相覆盖：

```text
Android/data/<package-name>/files/firmware_logs/
└── 20260728_194812_345/
    ├── <firmware-log-file-1>
    └── <firmware-log-file-2>
```

`getExternalFilesDir()` 返回应用专属外部存储目录；通常不需要额外的存储读写权限。传入 `exportFirmwareLogs()` 的目录不存在时，SDK 会尝试创建它。

## 完整导出示例

下面代码假定已经成功打开 EGO 设备并获得 `Device device`。导出期间请保持设备连接；同一 `Device` 同时只能执行一个日志导出任务。

```java
import android.util.Log;

import com.orbbec.obsensor.Device;
import com.orbbec.obsensor.FirmwareLogExportCallback;
import com.orbbec.obsensor.FirmwareLogExportException;
import com.orbbec.obsensor.FirmwareLogExportProgress;
import com.orbbec.obsensor.FirmwareLogExportResult;
import com.orbbec.obsensor.FirmwareLogExportTask;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

// 例如在 Activity 中执行；device 为已打开的 EGO Device。
File firmwareLogsRoot = getExternalFilesDir("firmware_logs");
if (firmwareLogsRoot == null) {
    throw new IllegalStateException("App-specific external storage is unavailable");
}

String directoryName = new SimpleDateFormat("yyyyMMdd_HHmmss_SSS", Locale.US)
        .format(new Date());
File outputDirectory = new File(firmwareLogsRoot, directoryName);

FirmwareLogExportTask exportTask = device.exportFirmwareLogs(
        outputDirectory,
        new FirmwareLogExportCallback() {
            @Override
            public void onProgress(FirmwareLogExportProgress progress) {
                Log.i("EgoFwLog", "Exported " + progress.getCurrentFileName()
                        + " (" + progress.getCompletedFileCount()
                        + "/" + progress.getTotalFileCount()
                        + ", " + progress.getPercent() + "%)");
                // 可在此处更新进度条：progressBar.setProgress(progress.getPercent());
            }

            @Override
            public void onSuccess(FirmwareLogExportResult result) {
                Log.i("EgoFwLog", "Export completed: "
                        + result.getOutputDirectory().getAbsolutePath());
                for (File file : result.getExportedFiles()) {
                    Log.i("EgoFwLog", "Exported file: " + file.getAbsolutePath());
                }
            }

            @Override
            public void onFailure(FirmwareLogExportException exception) {
                Log.e("EgoFwLog", "Export failed: " + exception.getReason()
                        + ", " + exception.getMessage());
            }
        });
```

## 取消导出

保存 `exportFirmwareLogs()` 返回的任务对象，在用户取消操作或页面退出时调用 `cancel()`：

```java
if (exportTask != null && !exportTask.isDone()) {
    exportTask.cancel();
}
```

取消后会通过 `onFailure()` 回调返回，失败原因是 `CANCELLED`。设备断开或关闭时，导出也会结束，失败原因是 `DEVICE_CLOSED`。

## 注意事项

- 仅对支持固件日志导出的 EGO 设备调用该接口。
- 导出过程中不要重复调用 `exportFirmwareLogs()`；否则会返回 `ALREADY_RUNNING`。
- 请为每次导出使用新的目录。示例采用毫秒级时间戳，避免文件覆盖并便于按时间定位日志。
- 在 `onSuccess()` 中使用 `FirmwareLogExportResult` 获取实际输出目录和所有已导出的文件。
