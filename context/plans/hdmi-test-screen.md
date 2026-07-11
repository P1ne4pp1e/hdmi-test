# Plan：直接 HDMI 测试画面

关联规格：[../kits/hdmi-test-screen.md](../kits/hdmi-test-screen.md)

1. 编写纯内存测试画面模块和单元测试，先验证 ARGB8888 像素与图案布局（R3，Gate 2）。
2. 编写 KMS 封装：DRM card 探测、connector/mode/CRTC 选择、dumb buffer、模式设置和状态恢复（R1、R2，Gate 1、5、6）。
3. 以 Makefile 连接 `libdrm`，提供 `make`、`make test` 及命令行帮助（R4，Gate 1、2、5）。
4. 在目标设备执行 HDMI 冒烟测试；当前没有 `/dev/dri` 时，将此条件作为明确的外部环境阻塞记录（R2、R3，Gate 5、6）。
