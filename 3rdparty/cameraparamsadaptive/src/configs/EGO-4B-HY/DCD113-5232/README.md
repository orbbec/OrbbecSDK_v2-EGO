# DCD113-5232

| 项 | 值 |
|---|---|
| 产品名称 | EGO-4B-HY |
| 产品型号 | DCD113-5232 |
| 产品 PID | 0x1211 |
| 项目名称 | TN2615 |
| 相机数量 | 4（**2 路 rgb2616 + 2 路 rgb2617**，两种 sensor 分辨率混装） |
| cam_0（左相机）/ cam_1（右相机） | 模组 `rgb2616`，sensor 可用区 **2592x1944**（唯一开放分辨率 = 标定分辨率） |
| cam_2（右外侧相机）/ cam_3（左外侧相机） | 模组 `rgb2617`，sensor 可用区 **1920x1200**（唯一开放分辨率 = 标定分辨率） |
| 开放分辨率（逐 cam） | cam_0/cam_1：`2592x1944_native`；cam_2/cam_3：`1920x1200_native`（每路**仅一个**模式，整幅无裁剪） |
| raw 读出朝向 | **四路均为 mirror=true、flip=true**（配置阶段已知，直接写进 config 的 `readout_orientation`；与 EGO-4B / DCD111-523 同一编码约定） |
| `lock` | 全部 **true**（每路只有一个整幅模式，裁剪恒为 0、无可移动空间） |
| config 语义指纹 | `d0e3da3da28fdd61`（含 pid / 逐 cam module / sensor / 读出朝向 / lock；与标定无关） |

> 配置文件名约定：本目录下 `DCD113-5232_convert_config.json`（JSONC，允许 `//` 注释；
> 注释不参与语义指纹——指纹由 `parseConvertJson()` 得到的字段计算，已验证带注释与
> 去注释两种写法的指纹完全一致）。
> 多产品固件按 **PID + 各 cam 模组型号**（`validateConfigIdentity()`，错误码 64/65；或
> `selectConfigByDevice()`）选择本文件，并用 `validateConfigSensor()` **逐 cam** 核对 sensor
> 分辨率（错误码 60——本产品两种 sensor 尺寸混装，必须逐台上报，不能用一个整机尺寸）。
> 产品/型号登记表见 `工程结构与说明.md` §13。

## 标定文件（**合成标定数据，仅工程验证，不可用于量产**）

> 本型号规格已确认，但**真标定文件尚未产出**；以下两份是**合成数据**，只为让标定/运行
> 链路可验证。**产品定型前请用真实标定整体替换**。

- `params_cam.yaml`：**合成标定数据（仅用于工程验证，不是真实标定，不可用于量产）**（文件头已注明）
  ——4 路**整幅**标定，标定分辨率 = 各自 sensor 可用区：
  cam_0/cam_1 为 `2592x1944`（fx/fy = 685.08803331 / 684.73968627，cx/cy = 1297.0 / 973.0），
  cam_2/cam_3 为 `1920x1200`（fx/fy = 507.47261727 / 507.21458242 与 507.88196102 / 507.75250165）。
  fx/fy 由同一光学族的 1920x1200 参考值按宽度比 ×1.35 放大，主点取图像中心附近、逐台略有不同。
  外参/温度/时间约定**完全沿用** `configs/EGO-4B/DCD111-523/params_cam.yaml`（cam_1 水平基线约 119 mm、
  cam_2/cam_3 相对 cam_0 垂直偏移 60 mm），未发明新的符号约定。
- `params_imu.yaml`：**合成标定数据（仅用于工程验证，不可用于量产）**——`cam0` 块的
  `resolution` / `intrinsics` 取自本型号 `params_cam.yaml` 的 cam_0（`resolution: [2592, 1944]`、
  `intrinsics: [1297.0, 973.0, 685.08803331, 684.73968627]`，顺序 cx/cy/fx/fy）；`imu0` 与
  `T_cam_imu` 沿用模板产品 `configs/EGO-4B/DCD111-523/params_imu.yaml` 的实测值、未改动。

## 验证结果

四个示例（calib / firmware / basic / sdk）+ 全量 config 矩阵均通过；生成产物
`convert_params.json` / `output_params_cam.yaml` / `output_params_imu.yaml` 均为偶数字节（`hasEvenByteSize()`）。
config 语义指纹 `d0e3da3da28fdd61` 可由 `example_calib` 反复复现。
