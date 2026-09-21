# EGO SD 卡文件服务接口说明（Android Java）

本文档面向使用 Orbbec Android SDK 的应用开发者，介绍 EGO 网络设备的 SD 卡文件管理、文件下载和上传接口。

## 您可以完成什么

| 目标 | 使用接口 |
| --- | --- |
| 查看某个文件或目录是否存在、大小和类型 | `getSdcardFileInfo` |
| 浏览 SD 卡根目录或递归浏览某个目录 | `getSdcardFileList` |
| 将一个设备端文件下载到 Android 本地 | `downloadSdcardFile(..., LOCAL, ...)` |
| 将一个设备端文件上传到 HTTP/COS | `downloadSdcardFile(..., URL, ...)` |
| 取消当前设备上的文件传输 | `cancelSdcardFileOperation` |
| 删除文件或递归删除目录 | `deleteSdcardFile` |

## 1. 适用范围

- 适用于支持 EGO SD 卡文件服务的网络设备。
- 应用需先通过 `OBContext.createNetDevice(ip, port)` 创建并保持 `Device` 对象。
- SD 卡文件服务的路径均为**设备端 SD 卡相对路径**，使用 `/` 作为路径分隔符。
- 固件、设备能力或设备当前状态不支持某项操作时，SDK 会抛出 `OBException`。

## 2. 准备工作

### 2.1 创建网络设备

```java
Device device = context.createNetDevice("192.168.1.10", 8090);
```

操作完成前请勿调用 `device.close()`。异步传输时，必须等到收到结束状态回调后再关闭设备。

### 2.2 Android 权限和本地目录

下载到 Android 本地存储时，应用应具备写入目标目录的权限。示例使用：

```text
/storage/emulated/0/Orbbec/Downloads
```

Android 11（API 30）及以上，如使用共享外部存储根目录，需要根据应用的存储方案申请相应权限；示例工程使用 `MANAGE_EXTERNAL_STORAGE`。

## 2.3 最快开始：下载一个文件

下面的流程适合首次验证。请将 IP、端口和设备端文件路径替换为实际值。

```java
Device device = context.createNetDevice("192.168.1.10", 8090);

SdcardFileInfo info = device.getSdcardFileInfo("EGO_AZEL/record.mp4");
if (!info.exists() || info.isDirectory()) {
    throw new IllegalStateException("The selected device path is not a file");
}

device.downloadSdcardFile(
        "EGO_AZEL/record.mp4",
        SdcardDownloadDestination.LOCAL,
        "/storage/emulated/0/Orbbec/Downloads/EGO_AZEL/record.mp4",
        progress -> {
            // This callback is not on the Android main thread.
            Log.i("EGO", progress.getState() + " " + progress.getPercent());
        },
        true);
```

收到 `DONE`、`FAILED...` 或 `CANCELLED` 后，当前文件传输才算结束。异步传输结束前请保持 `Device` 有效。

## 3. SD 卡文件信息查询

### `getSdcardFileInfo`

```java
SdcardFileInfo getSdcardFileInfo(String path)
```

查询设备 SD 卡上某个文件或目录是否存在，以及其基本属性。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `String` | 设备端 SD 卡相对路径，例如 `EGO_AZEL/record.mp4`。不能为空，不能包含 `..`。 |

返回 `SdcardFileInfo`：

| 方法 | 返回值 | 说明 |
| --- | --- | --- |
| `exists()` | `boolean` | 路径是否存在。 |
| `isDirectory()` | `boolean` | 是否为目录。 |
| `getSize()` | `long` | 文件大小，单位为字节；目录通常为 `0`。 |

示例：

```java
SdcardFileInfo info = device.getSdcardFileInfo("EGO_AZEL/record.mp4");
if (info.exists() && !info.isDirectory()) {
    Log.i(TAG, "file size=" + info.getSize());
}
```

## 4. SD 卡目录列表

### `getSdcardFileList`

```java
List<SdcardFileEntry> getSdcardFileList(String path, boolean recursive)
```

列出指定目录内容。传入空路径可列出 SD 卡根目录。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `String` | 要列出的设备端目录相对路径；传入空字符串表示根目录。不能为 `null`。 |
| `recursive` | `boolean` | `false`：仅列出当前目录直接子项；`true`：递归列出当前目录及其所有子目录中的条目。 |

返回 `List<SdcardFileEntry>`。每个条目包含：

| 方法 | 返回值 | 说明 |
| --- | --- | --- |
| `getName()` | `String` | 设备端文件或目录名称/路径。递归列表中应作为设备端路径使用。 |
| `isDirectory()` | `boolean` | 是否为目录。 |
| `getSize()` | `long` | 文件大小，单位为字节；目录为 `0`。 |

示例：

```java
List<SdcardFileEntry> entries = device.getSdcardFileList("EGO_AZEL", true);
for (SdcardFileEntry entry : entries) {
    Log.i(TAG, (entry.isDirectory() ? "DIR  " : "FILE ") + entry.getName());
}
```

## 5. 文件下载和上传

### `downloadSdcardFile`

```java
void downloadSdcardFile(
        String devicePath,
        SdcardDownloadDestination destinationType,
        String destination,
        SdcardDownloadProgressCallback callback,
        boolean async)
```

该接口一次传输一个设备端文件。目录传输应由应用先递归列出文件，再对每个文件分别调用本接口。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `devicePath` | `String` | 设备端 SD 卡文件相对路径。必须指向存在的文件，不能是目录，不能包含 `..`。 |
| `destinationType` | `SdcardDownloadDestination` | 传输目的地类型，见下表。 |
| `destination` | `String` | 目的地。下载到本地时为完整本地文件路径；上传时为设备可访问的完整 HTTP 预签名 URL。不能为空。 |
| `callback` | `SdcardDownloadProgressCallback` | 进度和结束状态回调。可以为 `null`；建议传入以处理结果。 |
| `async` | `boolean` | `false`：阻塞当前调用线程直到结束；`true`：立即返回，在后台执行，并通过回调通知结果。 |

#### 目的地类型

| 枚举值 | 说明 |
| --- | --- |
| `SdcardDownloadDestination.LOCAL` | 将设备端文件下载到 Android 本地文件。 |
| `SdcardDownloadDestination.URL` | 由设备将文件上传到远端 HTTP URL。当前设备端上传使用 `http://` URL。 |

### 5.1 下载到本地

```java
String localPath = "/storage/emulated/0/Orbbec/Downloads/EGO_AZEL/record.mp4";
device.downloadSdcardFile(
        "EGO_AZEL/record.mp4",
        SdcardDownloadDestination.LOCAL,
        localPath,
        this::onTransferProgress,
        true);
```

调用前请确保本地父目录存在且可写。SDK 可在支持的场景下继续未完成的本地下载；应用不应在传输过程中移动或删除目标文件。

### 5.2 上传到 HTTP/COS

```java
String presignedPutUrl = createPresignedUrlFor("EGO_AZEL/record.mp4");
device.downloadSdcardFile(
        "EGO_AZEL/record.mp4",
        SdcardDownloadDestination.URL,
        presignedPutUrl,
        this::onTransferProgress,
        true);
```

上传 URL 必须是设备能够访问的完整 `http://` 地址。对于腾讯 COS 等对象存储，应为每个文件生成独立的 `PUT` 预签名 URL；不要让多个文件共用同一个对象 URL，否则后上传的文件可能覆盖先上传的对象。

示例工程中的 `TencentCosPresignUrlGenerator` 仅演示 COS V5 预签名 URL 的生成方式。生产环境不建议将长期 SecretKey 硬编码在 Android APK 中，应通过业务服务端签发短期 URL。

### 5.3 目录下载或目录上传

SDK 的传输接口以“单文件”为单位。目录操作的推荐流程如下：

1. 调用 `getSdcardFileList(directory, true)` 获取递归列表；
2. 过滤 `isDirectory() == false` 的文件项；
3. 对每个文件分别调用 `downloadSdcardFile`；
4. 本地下载时，按设备端相对路径创建本地子目录；上传时，为每个文件生成独立预签名 URL。

建议顺序传输，或根据设备能力限制并发数。设备端可能限制同时进行的上传任务数量。

## 6. 传输进度与结果

### `SdcardDownloadProgressCallback`

```java
public interface SdcardDownloadProgressCallback {
    void onProgress(SdcardDownloadProgress progress);
}
```

回调可能发生在 native 工作线程。需要更新 Android UI 时，必须切换到主线程，例如使用 `runOnUiThread`。

`SdcardDownloadProgress` 包含：

| 方法 | 返回值 | 说明 |
| --- | --- | --- |
| `getState()` | `SdcardDownloadState` | 当前传输状态。 |
| `getFileName()` | `String` | 当前处理的文件名。 |
| `getPercent()` | `int` | 进度百分比 `0` 到 `100`；`-1` 表示设备暂时无法提供可计算的百分比。 |
| `getMd5Base64()` | `String` | 完成时设备返回的 Base64 MD5；未提供时可能为 `null` 或空字符串。 |

状态枚举：

| 状态 | 含义 |
| --- | --- |
| `TRANSFERRING` | 传输中。 |
| `DONE` | 传输成功完成。 |
| `CANCELLED` | 已取消。 |
| `FAILED` | 通用失败。 |
| `FAILED_NOT_FOUND` | 设备端文件不存在。 |
| `FAILED_TIMEOUT` | 超时。 |
| `FAILED_NETWORK` | 网络连接或传输错误。 |
| `FAILED_SERVER` | 服务端错误。 |
| `FAILED_BUSY` | 设备端忙，例如上传任务槽位已满。 |
| `FAILED_CAMERA_STREAMING` | 设备当前采集/串流状态不允许执行该操作。 |

除 `TRANSFERRING` 外，其余状态均为**终态**。每次传输都应以终态回调作为完成、失败或取消的唯一判定依据。不要仅凭 `getPercent() == 100` 判断成功。

回调示例：

```java
private void onTransferProgress(SdcardDownloadProgress progress) {
    String percent = progress.getPercent() >= 0
            ? progress.getPercent() + "%" : "calculating";
    Log.i(TAG, progress.getState() + " " + progress.getFileName() + " " + percent);
}
```

## 7. 取消传输

### `cancelSdcardFileOperation`

```java
void cancelSdcardFileOperation()
```

取消当前 `Device` 的 SD 卡文件服务操作。该操作会影响该设备上正在进行的下载或上传任务。

```java
device.cancelSdcardFileOperation();
```

取消请求发出后，请继续等待回调中的终态（通常为 `CANCELLED`）再更新业务状态或关闭设备。

## 8. 删除文件或目录

### `deleteSdcardFile`

```java
void deleteSdcardFile(String path)
```

删除设备端 SD 卡的文件或目录。

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| `path` | `String` | 要删除的设备端 SD 卡相对路径。不能为空，不能包含 `..`。 |

若 `path` 指向目录，设备端会递归删除其全部子目录和文件，操作不可恢复。建议在应用 UI 中先显示确认对话框，并可先调用 `getSdcardFileInfo` 确认目标存在及类型。

```java
SdcardFileInfo info = device.getSdcardFileInfo("EGO_AZEL");
if (info.exists()) {
    device.deleteSdcardFile("EGO_AZEL");
}
```

## 9. 常见注意事项

1. 不要将 `Device` 跨生命周期提前关闭；异步传输结束前关闭会导致回调中断。
2. 传输回调不在 Android 主线程，UI 更新必须切回主线程。
3. `-1` 进度不是失败，表示总大小或设备端上传进度暂不可用。
4. 删除目录不可恢复，应进行二次确认。
5. 上传对象存储时，生产环境应由服务端生成短期预签名 URL，避免在移动端保存长期访问密钥。
6. 设备端路径与 Android 本地路径不能混用；传给 SDK 的 `devicePath` 始终应为设备 SD 卡相对路径。

## 10. 推荐接入流程

1. 连接设备后先调用 `getSdcardFileList("", true)` 或指定目录，得到用户可选择的文件和目录。
2. 用户选择文件时，先调用 `getSdcardFileInfo` 确认类型；文件可直接传输，目录应先递归列出并逐文件处理。
3. 本地下载前创建目标父目录；对象存储上传前为每个文件准备独立、未过期的 HTTP 预签名 URL。
4. 始终注册 `SdcardDownloadProgressCallback`，将回调切换至 UI 线程后显示进度和终态。
5. 用户取消时调用 `cancelSdcardFileOperation`，并等待终态回调；删除目录前必须进行二次确认。

示例工程中的 `EgoSdcardFileServiceActivity` 展示了以上完整流程，可作为应用集成参考。
