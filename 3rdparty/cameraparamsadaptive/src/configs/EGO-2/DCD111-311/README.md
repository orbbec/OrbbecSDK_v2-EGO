# DCD111-311

| 项 | 值 |
|---|---|
| 产品名称 | EGO-2 |
| 产品型号 | DCD111-311 |
| 产品 PID | 0x1201 |
| 相机数量 | 2（各路同模组） |
| 开放分辨率 | 1600x1300、1600x1200 |
| sensor（**可用区**尺寸） | 1600x1300 |
| 模组型号 | rgb2610 |
| raw 读出朝向 | mirror=true、flip=true（配置阶段已知，直接写进 config；固件按 pid/模组/sensor 校验身份即可，无需校验朝向） |
| `lock` | 全部 **true**（裁剪值锁定为对称值；后续由算法组改 config 放开） |
| config 语义指纹 | `e13eda9d36428f80`（含 pid / module / 读出朝向 / lock；与标定无关） |

> 多产品固件按 **PID + 各 cam 模组型号**（`validateConfigIdentity()`，错误码 64/65；或 `selectConfigByDevice()`）选择，并用 `validateConfigSensor()` 核对 sensor 分辨率（错误码 60）。

## 标定文件

- `params_cam.yaml` / `params_imu.yaml`：来自早期批次 `TN2608-A-250200875`（该早期目录已并入本产品目录，标定文件已随本目录维护）（2 路、**1600x1300 全幅标定**，与本品 sensor 可用区一致）
- 验证：`example_calib` / `example_firmware` / `example_sdk` 全部通过；SDK 输出 cam_0 1600x1200 cx=806.68799 cy=592.90626
