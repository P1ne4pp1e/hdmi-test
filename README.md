# hdmi-test

用于验证无桌面 Linux 环境下，C++ 程序能否直接通过 DRM/KMS 输出 HDMI 测试画面。

## 当前测试程序

`hdmi_test` 不依赖 X11、Wayland、Qt、SDL 或普通应用窗口。它使用 `libdrm`：

1. 自动发现 `/dev/dri/card*`；
2. 优先选择已连接的 HDMI 输出与首选分辨率；
3. 创建 ARGB8888 dumb buffer；
4. 直接设置 KMS CRTC 并显示测试图；
5. 退出时恢复原显示状态。

测试画面包含深色背景、标题、网格、三种状态色块和 RGB 色条，用于确认基本颜色、分辨率与显示稳定性。

## 依赖

- Linux DRM/KMS 驱动，且系统必须存在 `/dev/dri/cardN`；
- `g++`（支持 C++20）；
- `make`；
- `pkg-config` 与 `libdrm` 开发文件。

## 构建与验证

```bash
make test
make
./build/hdmi_test --help
./build/hdmi_test
```

如果需要指定显卡设备：

```bash
./build/hdmi_test --device /dev/dri/card0
```

显示测试画面后按 `Ctrl-C` 退出。程序会尝试恢复启动前的显示状态。

## 当前硬件阻塞

当前开发环境尚未出现 `/dev/dri/card*`，因此已验证构建、图案绘制和缺失设备诊断，但还不能完成 HDMI 实机点亮。详情见 [实施记录](context/impl/hdmi-test-screen.md)。
