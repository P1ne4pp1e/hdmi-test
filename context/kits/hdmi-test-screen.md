# Kit：直接 HDMI 测试画面

## 目的

验证 Linux 小电脑能否在不启动 X11、Wayland 或桌面环境时，由单一 C++ 进程直接通过 DRM/KMS 向已连接的 HDMI 显示器输出画面。

## 要求

### R1：明确的运行时诊断

程序必须自动寻找具有显示 connector 的 DRM card；找不到时必须说明未找到 `/dev/dri/card*`，并以非零状态退出。程序还必须能列出 card 与 connector 状态，辅助硬件诊断。

**验收标准**

- [ ] 在没有 DRM card 的机器运行程序时，错误输出包含 `/dev/dri/card*`。**Gate 5**
- [ ] 程序返回非零退出码。**Gate 5**
- [ ] `--probe` 列出每个 DRM card 的 connector、连接状态和 mode 数量。**Gate 5**

### R2：直接 KMS 输出

程序必须选择一个已连接且有显示模式的 connector，使用其首选模式（无首选时第一个模式），创建 dumb buffer，并以 KMS 配置显示输出。

**验收标准**

- [ ] 不链接 X11、Wayland、Qt 或 SDL。**Gate 1**
- [ ] 在有可用 HDMI/DRM 环境的设备上，启动后 HDMI 显示测试画面。**Gate 5/6**
- [ ] 按 Ctrl-C 后恢复启动前的 CRTC 状态。**Gate 6**

### R3：测试画面

画面须包含深色背景、顶部标题栏、三种状态色块、分辨率信息区域和 RGB 色条；所有绘制都写入 ARGB8888 buffer。

**验收标准**

- [ ] 单元测试验证 ARGB 像素打包。**Gate 2**
- [ ] 单元测试验证色条顺序与状态色块绘制。**Gate 2**
- [ ] 在 HDMI 上人工确认文字和色块可见。**Gate 6**

### R4：可重复构建与执行

项目使用 Makefile 和系统 `g++`/`pkg-config libdrm` 构建，不要求 CMake 或 Qt。

**验收标准**

- [ ] `make` 成功生成可执行文件。**Gate 1**
- [ ] `make test` 成功运行无硬件依赖的单元测试。**Gate 2**
- [ ] `--help` 说明设备参数和退出方式。**Gate 5**

### R5：性能与调试可观测性

测试画面必须显示输出刷新率、应用帧率、帧序号、系统 CPU 使用率、GPU 使用率和内存使用量。目标 HDMI 模式为 60 Hz 级别（当前硬件实际模式为 59.97 Hz）；指标采样不得依赖高频外部进程。

**验收标准**

- [ ] CPU 利用率、内存和 GPU load 的解析逻辑有无硬件依赖的单元测试。**Gate 2**
- [ ] 测试画面显示目标 60 FPS、当前输出刷新率和实时指标。**Gate 5/6**
- [ ] 系统指标最多每秒读取一次；渲染路径不启动 `tegrastats` 等常驻采样子进程。**Gate 4**
- [ ] 动态压力背景必须由 EGL/OpenGL ES shader 生成，不允许 CPU 每帧逐像素生成全屏背景。**Gate 4**
- [ ] HUD 纹理必须实际在 fragment shader 中合成；空白 HUD 像素不得遮挡动态背景。**Gate 4**
- [ ] 帧提交由 Wayland frame callback / EGL swap interval 驱动，不得在完整渲染之后额外固定 sleep 16.67ms。**Gate 4**
- [ ] 连续 60 秒实测平均 FPS ≥ 59.0，且 p99 frame time ≤ 20ms。**Gate 4**
