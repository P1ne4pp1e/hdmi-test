# Kit：直接 HDMI 测试画面

## 目的

验证 Linux 小电脑能否在不启动 X11、Wayland 或桌面环境时，由单一 C++ 进程直接通过 DRM/KMS 向已连接的 HDMI 显示器输出画面。

## 要求

### R1：明确的运行时诊断

程序必须自动寻找 DRM card 设备；找不到时必须说明未找到 `/dev/dri/card*`，并以非零状态退出。

**验收标准**

- [ ] 在没有 DRM card 的机器运行程序时，错误输出包含 `/dev/dri/card*`。**Gate 5**
- [ ] 程序返回非零退出码。**Gate 5**

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
