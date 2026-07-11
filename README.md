# hdmi-test

用于验证无桌面 Linux 环境下，C++ 程序能否直接通过 DRM/KMS 输出 HDMI 测试画面。

## 当前测试程序

`hdmi_test` 是第一个通用 `libdrm`/KMS 探测与测试图原型，不依赖 X11、Wayland、Qt、SDL 或普通应用窗口。它会：

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
./build/hdmi_test --probe
./build/hdmi_test
```

如果需要指定显卡设备：

```bash
./build/hdmi_test --device /dev/dri/card0
```

显示测试画面后按 `Ctrl-C` 退出。程序会尝试恢复启动前的显示状态。

## 当前 Jetson 的可见测试画面

当前 JetPack 的 HDMI 输出由 NVIDIA 专有 X 驱动正常管理。`hdmi_x11_kiosk` 是一个原生 C++/X11 全屏测试画面，用于先验证画面设计、帧循环和实际 HDMI 输出；它不是最终无桌面后端。

```bash
make x11-kiosk
DISPLAY=:1 XAUTHORITY=/run/user/1000/gdm/Xauthority ./build/hdmi_x11_kiosk
```

当前用户的 GNOME 自动启动项位于 `~/.config/autostart/hdmi-x11-kiosk.desktop`，重新登录后会自动启动该测试画面。

## CMake

项目已提供 `CMakeLists.txt`。安装系统 `cmake` 后可使用：

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

## 当前硬件阻塞

当前 JetPack 的 NVIDIA 专有 X 驱动实际通过 `DP-1` 以 TMDS 信号驱动已连接的 HDMI 屏幕（800×480）；但通用 `libdrm` 枚举到的 `card1-DP-1` 同时报告为 `disconnected`。因此通用 KMS 原型尚不能直接点亮该屏幕，不能把它作为最终后端。详情见 [实施记录](context/impl/hdmi-test-screen.md)。
