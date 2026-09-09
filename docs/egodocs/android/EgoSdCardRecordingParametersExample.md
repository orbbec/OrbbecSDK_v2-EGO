# EGO SD Card Recording Parameter Configuration Example

## 设置录制 Profile 和文件切片时间

```java
Sensor leftSensor = device.getSensor(SensorType.COLOR_LEFT);
Sensor rightSensor = device.getSensor(SensorType.COLOR_RIGHT);
Sensor accelSensor = device.getSensor(SensorType.ACCEL);
Sensor gyroSensor = device.getSensor(SensorType.GYRO);

try (StreamProfileList leftProfiles = leftSensor.getStreamProfileList();
     StreamProfileList rightProfiles = rightSensor.getStreamProfileList();
     StreamProfileList accelProfiles = accelSensor.getStreamProfileList();
     StreamProfileList gyroProfiles = gyroSensor.getStreamProfileList();
     VideoStreamProfile left = leftProfiles.getProfile(0).as(StreamType.COLOR_LEFT);
     VideoStreamProfile right = rightProfiles.getProfile(0).as(StreamType.COLOR_RIGHT);
     AccelStreamProfile accel = accelProfiles.getProfile(0).as(StreamType.ACCEL);
     GyroStreamProfile gyro = gyroProfiles.getProfile(0).as(StreamType.GYRO)) {

    device.setFwRecordProfileList(
            FwRecordStreamProfile.from(left),
            FwRecordStreamProfile.from(right),
            FwRecordStreamProfile.from(accel),
            FwRecordStreamProfile.from(gyro));

    DeviceProperty splitDuration =
            DeviceProperty.OB_PROP_FW_RECORDING_FILE_SPLIT_DURATION_MIN_INT;
    if (device.isPropertySupported(splitDuration, PermissionType.OB_PERMISSION_WRITE)) {
        int durationMinutes = 10;
        int min = device.getMinRangeI(splitDuration);
        int max = device.getMaxRangeI(splitDuration);
        if (durationMinutes < min || durationMinutes > max) {
            throw new IllegalArgumentException(
                    "split duration out of range: [" + min + ", " + max + "]");
        }
        device.setPropertyValueI(splitDuration, durationMinutes);
    }
}
```

`FwRecordStreamProfile.from(VideoStreamProfile)` 同时支持 Color、Depth、IR、左右 Color、左右 IR、Raw Phase 和 Confidence 视频流。

## 回读配置

```java
List<FwRecordStreamProfile> profiles = device.getFwRecordProfileList();
for (FwRecordStreamProfile profile : profiles) {
    if (profile instanceof FwRecordVideoStreamProfile) {
        FwRecordVideoStreamProfile video = (FwRecordVideoStreamProfile) profile;
        Log.i(TAG, video.getSensorType() + ": "
                + video.getWidth() + "x" + video.getHeight()
                + "@" + video.getFps()
                + ", format=" + video.getFormat());
    }
    else if (profile instanceof FwRecordAccelStreamProfile) {
        FwRecordAccelStreamProfile accel = (FwRecordAccelStreamProfile) profile;
        Log.i(TAG, "ACCEL: range=" + accel.getFullScaleRange()
                + ", rate=" + accel.getSampleRate());
    }
    else if (profile instanceof FwRecordGyroStreamProfile) {
        FwRecordGyroStreamProfile gyro = (FwRecordGyroStreamProfile) profile;
        Log.i(TAG, "GYRO: range=" + gyro.getFullScaleRange()
                + ", rate=" + gyro.getSampleRate());
    }
}

DeviceProperty splitDuration =
        DeviceProperty.OB_PROP_FW_RECORDING_FILE_SPLIT_DURATION_MIN_INT;
if (device.isPropertySupported(splitDuration, PermissionType.OB_PERMISSION_READ)) {
    int durationMinutes = device.getPropertyValueI(splitDuration);
    Log.i(TAG, "split duration: " + durationMinutes + " min");
}
```

不支持固件端录制配置的设备会由底层 SDK 返回 `OBException`。切片时间属性应先使用 `isPropertySupported()` 检查读写权限。
