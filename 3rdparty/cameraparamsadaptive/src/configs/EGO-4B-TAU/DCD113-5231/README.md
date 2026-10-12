# DCD113-5231

| 项 | 值 |
|---|---|
| 产品名称 | EGO-4B-TAU |
| 产品型号 | DCD113-5231 |
| 产品 PID | 0x1208 |
| 相机数量 | 4（各路同模组） |
| 开放分辨率 | 1920x1200、1920x1080、1600x1200 |
| sensor（**可用区**尺寸） | 1920x1200 |
| 模组型号 | rgb2612 |
| raw 读出朝向 | mirror=true、flip=true（配置阶段已知，直接写进 config；固件按 pid/模组/sensor 校验身份即可，无需校验朝向） |
| `lock` | 全部 **true**（裁剪值锁定为对称值；后续由算法组改 config 放开） |
| config 语义指纹 | `079dce5816ba24b3`（含 pid / module / 读出朝向 / lock；与标定无关） |

> 多产品固件按 **PID + 各 cam 模组型号**（`validateConfigIdentity()`，错误码 64/65；或 `selectConfigByDevice()`）选择，并用 `validateConfigSensor()` 核对 sensor 分辨率（错误码 60）。
## 标定文件

- `params_cam.yaml`：**合成标定数据（仅用于工程验证，不是真实标定）**（文件头已注明）——4 路整幅 1920×1200，cam_0 `cx=968.0 / cy=606.0`。
- `params_imu.yaml`：cam0 块已与本型号自身标定**对齐**——`resolution` / `intrinsics` 取自本型号 `params_cam.yaml` 的 cam_0（`resolution: [1920, 1200]`、`intrinsics: [968.0, 606.0, 507.47261727, 507.21458242]`，顺序 cx/cy/fx/fy），不再是 `configs/EGO-4B/DCD111-523/params_imu.yaml` 的字节相同副本（该借用副本是 1600×1200 子窗口内参 `808.01 / 600.17`）；`imu0` 与 `T_cam_imu` 沿用原实测值、未改动（文件头已注明）。工程验证期仍由 SDK 侧 `reoutputImuParams()` 按目标分辨率重输出（demo 分辨率 1920x1080 裁剪模式 ⇒ `cx=968.0 / cy=546.0`）。
- 验证：三个示例（calib / basic / sdk）全部通过；config 语义指纹 `079dce5816ba24b3`。
