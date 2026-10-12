# DCD351-211

| 项 | 值 |
|---|---|
| 产品名称 | Wrist-1 |
| 产品型号 | DCD351-211 |
| 产品 PID | 0x1203 |
| 项目名称 | TN2608-B |
| 相机数量 | 1（单相机，腕部相机） |
| 开放分辨率 | **1600x1200（唯一）**——它等于整幅，无裁剪余量 |
| sensor（**可用区**尺寸，写入 config 的 `sensor_width/height`） | **1600x1200**（固件交付帧；模组 `rgb2612` 的可用区为 1920x1200，出流前已固定左右各裁 160） |
| 模组型号（config 里每 cam 的 `module`，小写） | `rgb2612` |
| raw 读出朝向 | **无镜像、无翻转**（`readout_orientation` = `{mirror: false, flip: false}`，配置阶段已知，直接写进 config） |
| `lock` | **true**（唯一模式为整幅，裁剪恒为 0、无可移动空间） |
| config 语义指纹 | `514c92aa9e445c75`（含 pid / module / sensor / 读出朝向 / lock；与标定无关） |
| 状态 | **规格已确认**；config 与标定文件齐备（标定为合成数据，见下） |

> 配置文件名约定：本目录下 `DCD351-211_convert_config.json`（JSONC，允许 `//` 注释；
> 注释不参与语义指纹——指纹由 `parseConvertJson()` 得到的字段计算，已验证带注释与
> 去注释两种写法的指纹完全一致）。
> 多产品固件按 **PID + 各 cam 模组型号**（`validateConfigIdentity()`，错误码 64/65；或
> `selectConfigByDevice()`）选择本文件，并用 `validateConfigSensor()` 核对 sensor 分辨率
> （错误码 60；本型号必须上报 **1600x1200**，上报 1920x1200 会被判为 config/硬件不符）。
> 产品/型号登记表见 `工程结构与说明.md` §13。

## 标定文件（**合成标定数据，仅工程验证，不可用于量产**）

> 本型号规格已确认，但**真标定文件尚未产出**；以下两份是**合成数据**，只为让标定/运行
> 链路可验证。**产品定型前请用真实标定整体替换**。

- `params_cam.yaml`：**合成标定数据（仅用于工程验证，不是真实标定，不可用于量产）**（文件头已注明）
  ——单相机 cam_0，**整幅 1600x1200** 标定（= 固件交付帧 = 唯一开放分辨率 ⇒
  `anchorCalibrationToSensor()` 是空操作）。主点取图像中心附近（cx/cy = 801.0 / 596.0），
  fx/fy = 506.41138124 / 506.53864725，畸变模型 `KB`（与前代 `rgb2612` 同族系数）。
  外参/温度/时间约定**完全沿用** `configs/EGO-2/DCD111-811/params_cam.yaml`（同为 rgb2612 /
  1600x1200 / 无镜像翻转），未发明新的符号约定。
- `params_imu.yaml`：**合成标定数据（仅用于工程验证，不可用于量产）**——`cam0` 块的
  `resolution` / `intrinsics` 取自本型号 `params_cam.yaml` 的 cam_0（`resolution: [1600, 1200]`、
  `intrinsics: [801.0, 596.0, 506.41138124, 506.53864725]`，顺序 cx/cy/fx/fy）；`imu0` 与
  `T_cam_imu` 沿用模板产品 `configs/EGO-4B/DCD111-523/params_imu.yaml` 的实测值、未改动。

## 验证结果

四个示例（calib / firmware / basic / sdk）+ 全量 config 矩阵均通过；生成产物
`convert_params.json` / `output_params_cam.yaml` / `output_params_imu.yaml` 均为偶数字节（`hasEvenByteSize()`）。
config 语义指纹 `514c92aa9e445c75` 可由 `example_calib` 反复复现。
