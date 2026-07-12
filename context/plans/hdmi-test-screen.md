# Plan：直接 HDMI 测试画面

关联规格：[../kits/hdmi-test-screen.md](../kits/hdmi-test-screen.md)

1. 编写纯内存测试画面模块和单元测试，先验证 ARGB8888 像素与图案布局（R3，Gate 2）。
2. 编写 KMS 封装：DRM card 探测、connector/mode/CRTC 选择、dumb buffer、模式设置和状态恢复（R1、R2，Gate 1、5、6）。自动选择时跳过没有 connector 的 card，并提供 `--probe` 硬件清单。
3. 以 Makefile 连接 `libdrm`，提供 `make`、`make test` 及命令行帮助（R4，Gate 1、2、5）。
4. 在目标设备执行 HDMI 冒烟测试；当前没有 `/dev/dri` 时，将此条件作为明确的外部环境阻塞记录（R2、R3，Gate 5、6）。
5. 实现 `/proc` 与 Jetson GPU sysfs 的低频系统指标采样，显示 60 FPS 目标、输出刷新率和资源信息（R5，Gate 2、4、5）。
6. 将动态背景迁移到 EGL/OpenGL ES fragment shader，并以 Wayland frame callback/EGL swap interval 驱动 60Hz 提交；保留低频 CPU 字体 HUD 更新（R5，Gate 4、5）。
