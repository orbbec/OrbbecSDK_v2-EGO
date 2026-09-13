# EGODepth depth work mode (Default <-> Factory Calib) test

This sample validates the EGODepth depth work mode switching feature:

- `Default` mode: the device exposes Depth but **no** IR_LEFT / IR_RIGHT.
- `Factory Calib` mode: the device exposes IR_LEFT / IR_RIGHT but **no** Depth.

The sample:

1. finds the EGODepth device;
2. checks the initial mode is `Default` and the sensor set matches;
3. verifies a Depth stream can start and deliver frames in `Default`;
4. switches to `Factory Calib` and reopens the device;
5. checks the sensor set matches and verifies an IR_LEFT stream;
6. switches back to `Default` and reopens the device.

> Requires firmware that implements `OB_RAW_DATA_DEPTH_ALG_MODE_LIST` and
> `OB_STRUCT_CURRENT_DEPTH_ALG_MODE` for EGODepth.

Press any key to exit when done; the exit code is 0 when all checks pass.
