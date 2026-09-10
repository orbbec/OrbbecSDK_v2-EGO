# 1. stream infrared_left

基于 `1.stream.infrared`(ob_infrared)的变体示例:默认**只开启 Left IR 单流**,可通过命令行参数切换开启 Right IR,便于单独验证单个红外相机的帧率/带宽。

## 用法

```bash
ob_infrared_left            # 只开 Left IR(默认)
ob_infrared_left -r         # 只开 Right IR
ob_infrared_left --both     # 同时开 Left IR + Right IR
ob_infrared_left -h         # 查看帮助
```

参数说明:

| 参数 | 说明 |
| --- | --- |
| `-h`, `--help` | 显示帮助信息并退出 |
| `-r`, `--right` | 只开启 Right IR 流(默认是只开 Left IR) |
| `-b`, `--both` | 同时开启 Left IR 与 Right IR 流 |

## 说明

- 未传参时行为与单路 IR 设备一致:仅启用 `OB_SENSOR_IR_LEFT`,便于单独观察左目红外流的实际帧率(对比多流场景下的吞吐)。
- 传 `-r/--right` 时仅启用 `OB_SENSOR_IR_RIGHT`,用于单独验证右目。
- 传 `-b/--both` 时同时启用左右目,行为与 `ob_infrared` 一致。
