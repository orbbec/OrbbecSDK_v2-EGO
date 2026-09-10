# Ego 扬声器播放（EgoSpeakerPlayer）接口说明

`EgoSpeakerPlayer` 用于把 PCM 音频数据播放到 **EgoDevice 的物理扬声器**，可与 Orbbec SDK 的相机、麦克风采集同时运行。

- 源码：`app/src/main/java/com/orbbec/orbbecsdkexamples/utils/EgoSpeakerPlayer.java`
- 独立类，仅依赖 `android.content.Context`，可整类拷贝到工程使用。

---

## 1. 快速开始

调用流程：查询设备音频输出状态 → 按需由用户确认开启 → `start()` → `write()`（可反复）→ `stop()`。

### 1.1 开启设备音频输出

新版 Ego 固件可通过 `OB_PROP_DEVICE_AUDIO_OUTPUT_ENABLE_BOOL` 控制设备端音频输出。该能力因设备和固件而异，不能假定一定可用；所有属性访问都应与其他 `Device` 调用在同一设备工作线程中串行执行。

先同时检查读、写权限。任一权限不支持时，**不做任何设置**，直接继续原有的播放器流程，以兼容旧设备和旧固件。读写都支持时读取当前值：已开启则无需处理；未开启时由用户确认后写入并回读确认，设置立即生效，无需重启设备。

```java
import com.orbbec.obsensor.Device;
import com.orbbec.obsensor.property.DeviceProperty;
import com.orbbec.obsensor.types.PermissionType;

boolean isDeviceAudioOutputEnabled(Device device) {
    DeviceProperty property = DeviceProperty.OB_PROP_DEVICE_AUDIO_OUTPUT_ENABLE_BOOL;
    boolean canRead = device.isPropertySupported(property, PermissionType.OB_PERMISSION_READ);
    boolean canWrite = device.isPropertySupported(property, PermissionType.OB_PERMISSION_WRITE);
    if (!canRead || !canWrite) {
        return true; // 不支持控制属性：保持原有播放流程，不做任何设备操作。
    }
    return device.getPropertyValueB(property);
}

// 用户在 UI 中确认“开启”后，在设备工作线程调用。
void enableDeviceAudioOutput(Device device) {
    DeviceProperty property = DeviceProperty.OB_PROP_DEVICE_AUDIO_OUTPUT_ENABLE_BOOL;
    device.setPropertyValueB(property, true);
    if (!device.getPropertyValueB(property)) {
        throw new IllegalStateException("Audio output enable read-back is false");
    }
}
```

```java
// 1) 获取到 SDK Device 后，在设备工作线程读取状态。
//    false 表示属性受支持但尚未开启：在 UI 提示用户确认，确认后调用
//    enableDeviceAudioOutput()，成功后可立即继续播放。
if (!isDeviceAudioOutputEnabled(device)) {
    return;
}

// 2) 创建（默认 16000 Hz / 单声道 / 16-bit，与设备原生格式一致）
EgoSpeakerPlayer player = new EgoSpeakerPlayer(context);

// 3) 启动。必须在 write() 之前调用；建议放在工作线程执行
if (!player.start()) {
    // 没找到 Ego / USB 输出设备，或音轨创建失败
    return;
}

// 4) 持续送 PCM（非阻塞，可在任意线程反复调用）
player.write(pcmChunk, pcmChunk.length);

// 5) 结束时释放
player.stop();
```

---

## 2. 构造函数

```java
// 默认格式：16000 Hz / 单声道 / 16-bit（推荐，与设备原生格式匹配）
public EgoSpeakerPlayer(Context context)

// 指定格式：音源不是 16 kHz 时用它，系统会自动重采样到设备 16 kHz
public EgoSpeakerPlayer(Context context, int sampleRate, int channels, int bitsPerSample)
```

| 参数 | 说明 | 约束 |
| --- | --- | --- |
| `context` | 任意 Context | 非空 |
| `sampleRate` | 音源采样率（Hz） | 必须与送入 `write()` 的 PCM 采样率一致 |
| `channels` | 声道数 | 仅支持 `1`（单声道）或 `2`（立体声） |
| `bitsPerSample` | 位深 | **仅支持 16** |

常量：`DEFAULT_SAMPLE_RATE`(16000)、`DEFAULT_CHANNELS`(1)、`DEFAULT_BITS_PER_SAMPLE`(16)。

> 播放 48 kHz 音源：用 `new EgoSpeakerPlayer(ctx, 48000, 1, 16)` 构造并送 48 kHz PCM，系统会自动降采样。**不要**按 16 kHz 构造却送 48 kHz 数据，否则会变调。

---

## 3. 接口列表

### `boolean start()`
定位 Ego 扬声器输出并起播。返回 `true` 成功；返回 `false` 表示未找到 Ego / USB 输出设备或音轨创建失败（无声音）。幂等。建议放在工作线程调用。

### `void write(byte[] pcm, int length)`
### `void write(byte[] pcm, int offset, int length)`
送一段 PCM 到播放队列。**非阻塞、线程安全**，可任意线程反复调用；队列满时丢弃该块而不阻塞。offset 重载可从大缓冲切片送入，避免每块 new 子数组。送入的必须是**裸 PCM**（格式见第 4 节），不能直接送 WAV / MP3 / AAC。

### `void playTone(int freqHz, int durationMs)`
自检用：合成正弦纯音播放，快速确认扬声器是否出声。须在 `start()` 之后调用。
```java
player.playTone(440, 2000);  // 播 2 秒 440Hz 纯音
```

### `void stop()`
停止播放并释放。幂等，可重复调用。

### `boolean isPlaying()`
当前是否处于播放状态。

### `String getRoutedDeviceName()`
上一次 `start()` 实际路由到的 USB 输出设备产品名，用于 UI / 日志。

---

## 4. PCM 格式要求（送入 `write()` 的数据）

| 项 | 要求 |
| --- | --- |
| **编码** | 16-bit **有符号** PCM，**小端序**（低字节在前）。不接受 8-bit / 24-bit / 32-bit / float。 |
| **采样率** | 必须等于构造播放器时的 `sampleRate`（默认 16000）。不一致会变调播放。 |
| **声道** | 必须等于构造时的 `channels`（默认单声道）。立体声按 L,R,L,R… 交织。 |
| **length** | 整帧数 —— `channels * 2` 字节的整数倍（单声道 16-bit 为 2）。非整帧的尾部残帧会被截断。 |

音源来源由业务决定：本地文件（去掉 WAV 头 / 解码后送裸 PCM）、TTS 合成、网络实时流等，均把 PCM 送 `write()` 即可。

---

## 5. 示例一：直接传入 PCM（最常用）

如果手上已经是裸 PCM（采集回来的、网络下发的、自己合成的），**直接送 `write()`，无需任何解析**。

### 5.1 一次性播放整段 PCM

```java
byte[] pcm = /* 16-bit 有符号小端、16000Hz、单声道的裸 PCM */;

EgoSpeakerPlayer player = new EgoSpeakerPlayer(context);  // 16000/mono/16-bit
if (!player.start()) {                                    // 建议放工作线程
    Log.w(TAG, "未找到 Ego 扬声器输出设备");
    return;
}

// 大段数据建议分块送，避免一次占用过多队列缓冲
final int CHUNK = 4096;  // 帧对齐：单声道16-bit须为2的倍数，4096满足
for (int off = 0; off < pcm.length && player.isPlaying(); off += CHUNK) {
    int n = Math.min(CHUNK, pcm.length - off);
    player.write(pcm, off, n);   // offset 重载，零额外拷贝
}

// 等待队列播完再停，否则截断尾音。时长 = 字节数 / (采样率×声道×2)
long durationMs = (long) pcm.length * 1000 / (16000 * 1 * 2);
try { Thread.sleep(durationMs + 400); } catch (InterruptedException ignored) {}
player.stop();
```

### 5.2 实时流式播放（边收边播）

音源是持续产生的 PCM（如网络语音、实时合成）时，`start()` 一次，PCM 每到一块就 `write()` 一块，`write()` 非阻塞不会卡采集线程；结束时再 `stop()`。把播放器作为成员字段持有即可：

```java
public class VoiceSession {

    private final EgoSpeakerPlayer mPlayer;

    public VoiceSession(Context context) {
        mPlayer = new EgoSpeakerPlayer(context);  // 16000/mono/16-bit
    }

    /** 会话开始：起播（建议放工作线程执行）。 */
    public boolean start() {
        return mPlayer.start();
    }

    /** 数据回调 / 接收线程里，每收到一块 PCM 就送。 */
    public void onPcmReceived(byte[] pcm, int length) {
        mPlayer.write(pcm, length);   // 非阻塞；队列满会自动丢最新块，不阻塞调用方
    }

    /** 会话结束：停止并释放。 */
    public void stop() {
        mPlayer.stop();
    }
}
```

> `write()` 的 `pcm` 缓冲在调用返回后即可复用/覆盖——内部会把数据拷进自己的池化缓冲。

---

## 6. 示例二：播放一个 WAV 文件

WAV 是容器格式，`write()` 只收裸 PCM，所以要先解析出 `fmt`（采样率/声道/位深）和 `data` 区，再用 data 区的采样率构造播放器、把 data 字节送 `write()`。

### 6.1 解析 WAV（取出格式与 PCM data 区）

```java
/** 极简 WAV 解析：定位 fmt/data chunk，仅支持 16-bit PCM。 */
static final class Wav {
    byte[] bytes;
    int sampleRate, channels, bitsPerSample, byteRate;
    int dataOffset, dataLength;

    static Wav parse(byte[] b) throws IOException {
        if (b.length < 44
                || b[0] != 'R' || b[1] != 'I' || b[2] != 'F' || b[3] != 'F'
                || b[8] != 'W' || b[9] != 'A' || b[10] != 'V' || b[11] != 'E') {
            throw new IOException("不是合法的 RIFF/WAVE 文件");
        }
        Wav w = new Wav();
        w.bytes = b;
        boolean haveFmt = false, haveData = false;
        int p = 12;  // data 不一定在偏移 44，可能前面有 fact/LIST 等 chunk
        while (p + 8 <= b.length) {
            int size = le32(b, p + 4);
            int body = p + 8;
            if (b[p] == 'f' && b[p+1] == 'm' && b[p+2] == 't' && b[p+3] == ' ') {
                w.channels      = le16(b, body + 2);
                w.sampleRate    = le32(b, body + 4);
                w.byteRate      = le32(b, body + 8);
                w.bitsPerSample = le16(b, body + 14);
                haveFmt = true;
            } else if (b[p] == 'd' && b[p+1] == 'a' && b[p+2] == 't' && b[p+3] == 'a') {
                w.dataOffset = body;
                w.dataLength = Math.min(size, b.length - body);
                haveData = true;
            }
            if (haveFmt && haveData) break;
            p = body + size + (size & 1);  // chunk 按偶数字节对齐
        }
        if (!haveFmt || !haveData) throw new IOException("缺少 fmt/data chunk");
        if (w.bitsPerSample != 16) throw new IOException("仅支持 16-bit PCM，实际 " + w.bitsPerSample);
        if (w.byteRate <= 0) w.byteRate = w.sampleRate * w.channels * w.bitsPerSample / 8;
        return w;
    }

    private static int le16(byte[] b, int i) {
        return (b[i] & 0xFF) | ((b[i+1] & 0xFF) << 8);
    }
    private static int le32(byte[] b, int i) {
        return (b[i] & 0xFF) | ((b[i+1] & 0xFF) << 8)
                | ((b[i+2] & 0xFF) << 16) | ((b[i+3] & 0xFF) << 24);
    }
}
```

> MP3 / AAC 等压缩格式没有这么简单：需用 `MediaExtractor` + `MediaCodec` 解码成 16-bit PCM 后再送 `write()`。

### 6.2 用解析结果播放

```java
byte[] wavBytes = /* 读入整个 WAV 文件 */;
Wav wav = Wav.parse(wavBytes);

// 用 data 区的真实采样率/声道构造，让系统自动重采样到设备 16 kHz
EgoSpeakerPlayer player =
        new EgoSpeakerPlayer(context, wav.sampleRate, wav.channels, wav.bitsPerSample);
if (!player.start()) {
    Log.w(TAG, "未找到 Ego 扬声器输出设备");
    return;
}

// 只送 data 区的裸 PCM（用 offset 重载，零额外拷贝）
int off = wav.dataOffset;
int end = wav.dataOffset + wav.dataLength;
final int CHUNK = 4096;
while (off < end && player.isPlaying()) {
    int n = Math.min(CHUNK, end - off);
    player.write(wav.bytes, off, n);
    off += n;
}

// 等待队列播完（约 = 音频时长）再停止，否则会截断尾音
long durationMs = (long) wav.dataLength * 1000 / wav.byteRate;
try { Thread.sleep(durationMs + 400); } catch (InterruptedException ignored) {}
player.stop();
```

---

## 7. 注意事项

1. **必须先 `start()` 再 `write()`**；`start()` 返回 `false` 时不要继续送数据。
2. **格式对齐**：采样率 / 声道 / 位深要与构造参数一致，否则变调或起播失败。
3. **不要直接送容器格式**（WAV/MP3/AAC），先解成裸 PCM。
4. **停止时机**：流式播放要等队列基本播完再 `stop()`，否则尾音被截断。
5. **音量**：`start()` 内部已把音轨增益拉满；实际响度还受系统媒体音量影响，声音偏小时先把系统媒体音量调到最大。
6. **设备音频输出使能**：先检查 `OB_PROP_DEVICE_AUDIO_OUTPUT_ENABLE_BOOL` 是否同时支持读、写；任一不支持时不做任何设备操作。支持时先读取状态，已开启不重复设置；未开启须经用户确认后设置并回读确认。设置立即生效，无需重启设备。
