# DCD111-523

| 项 | 值 |
|---|---|
| 产品名称 | EGO-4B |
| 产品型号 | DCD111-523 |
| 产品 PID | 0x1204 |
| 相机数量 | 4（各路同模组） |
| 开放分辨率 | 1920x1200、1920x1080、1600x1200 |
| sensor（**可用区**尺寸） | 1920x1200 |
| 模组型号 | rgb2612 |
| raw 读出朝向 | mirror=true、flip=true（配置阶段已知，直接写进 config；固件按 pid/模组/sensor 校验身份即可，无需校验朝向） |
| `lock` | 全部 **true**（裁剪值锁定为对称值；后续由算法组改 config 放开） |
| config 语义指纹 | `e12db117d2585857`（含 pid / module / 读出朝向 / lock；与标定无关） |

> 多产品固件按 **PID + 各 cam 模组型号**（`validateConfigIdentity()`，错误码 64/65；或 `selectConfigByDevice()`）选择，并用 `validateConfigSensor()` 核对 sensor 分辨率（错误码 60）。

## 标定文件（**两代标定都必须支持**）

- `params_cam.yaml`：**1600x1200 居中子窗口标定**（原始数据来自早期批次 `TN2608-A-250201157`，该目录已并入本产品目录）—— 前期已出货相机的标定方式（库内部按 `anchorCalibrationToSensor()` 把主点锚定进 sensor 帧，cx += 160）
- `params_cam_1920x1200.yaml`：同机型的 **1920x1200 全幅标定** —— 后续统一采用
- 本目录的 `params_cam.yaml` / `params_cam_1920x1200.yaml` 与 `configs/EGO-4B/DCD111-923` **内容共用是有意复用（by design，非复制粘贴失误）**：两型号 sensor 可用区相同，算法组确认工程验证期有意共用同一份标定数据。
- 两代标定**生成的 `convert_params.json` 完全一致**（config 全量 `lock=true`，裁剪值不依赖标定）
- 两代标定**算出的输出内参逐位一致**：实测 cam_0 1920x1080 `cx=968.0127 / cy=540.17045`（子窗口 +160 锚定后 = 全幅值）
- 验证：`example_calib` / `example_firmware` / `example_sdk` × 两代标定，全部通过
