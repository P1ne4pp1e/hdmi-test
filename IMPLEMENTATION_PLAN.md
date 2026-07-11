## Stage 1: 规格与环境基线
**Goal**: 明确首个 HDMI 测试画面的可验证目标，并记录当前图形设备环境。
**Success Criteria**: 需求、验收标准和构建方式已写入项目；运行时会对缺失 DRM 设备给出明确诊断。
**Tests**: `make test` 覆盖测试画面数据；无 `/dev/dri` 时程序返回可读错误。
**Status**: Complete

## Stage 2: 直接 KMS 测试画面
**Goal**: 构建不依赖桌面环境的 C++ libdrm 程序，直接向 HDMI 输出测试画面。
**Success Criteria**: 程序选择已连接显示器、设置首选模式、绘制画面并在退出时恢复原显示状态。
**Tests**: 单元测试验证画面布局与像素格式；在目标设备执行人工 HDMI 冒烟测试。
**Status**: Complete

## Stage 3: 验证入口与交接
**Goal**: 提供可重复的构建、运行、诊断和验证说明。
**Success Criteria**: README 说明依赖、命令、成功标志和已知环境阻塞；计划文档在完成后删除。
**Tests**: `make test`、`make`、`./build/hdmi_test --help`。
**Status**: In Progress
