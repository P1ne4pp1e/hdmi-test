## Stage 1: 规格与环境基线
**Goal**: 明确首个 HDMI 测试画面的可验证目标，并记录当前图形设备环境。
**Success Criteria**: 需求、验收标准和构建方式已写入项目；运行时会对缺失 DRM 设备给出明确诊断。
**Tests**: `make test` 覆盖测试画面数据；无 `/dev/dri` 时程序返回可读错误。
**Status**: Complete

## Stage 2: 直接 KMS 测试画面
**Goal**: 验证在 Jetson 当前显示驱动栈中可用的无桌面直出路径，并输出测试画面。
**Success Criteria**: 选定的后端能使用当前 HDMI 输出、设置目标模式并显示画面；退出时恢复或由显示服务接管状态。
**Tests**: 单元测试验证画面布局与像素格式；在目标设备执行人工 HDMI 冒烟测试。
**Status**: In Progress

## Stage 3: 当前显示栈的可见测试画面
**Goal**: 经当前 NVIDIA X 显示栈在已验证的 HDMI 屏上显示 C++ 全屏测试画面。
**Success Criteria**: 原生 X11 测试程序在 `:1` 的 800×480 屏幕上全屏显示；可由服务模板在图形会话后启动。
**Tests**: `make x11-kiosk`、`xwininfo -root -tree`、人工核对 HDMI 屏。
**Status**: Complete

## Stage 4: 无桌面 NVIDIA 后端验证
**Goal**: 选择并验证 JetPack NVIDIA 支持的 kiosk/直接输出后端，替代临时 X11 测试路径。
**Success Criteria**: 不依赖 GNOME 桌面仍可输出测试画面，并记录性能与启动行为。
**Tests**: 停止图形桌面后的实机启动、重启后自启动、HDMI 人工审查。
**Status**: Not Started
