# 实施记录：直接 HDMI 测试画面

关联规格：[../kits/hdmi-test-screen.md](../kits/hdmi-test-screen.md)

## 已完成

- 以 C++20、Makefile 和 `libdrm` 建立最小测试框架；不依赖 X11、Wayland、Qt 或 SDL。
- 实现纯内存 ARGB8888 测试画面：深色背景、标题栏、网格、三种状态色卡片和 RGB 色条。
- 实现 DRM/KMS 输出：自动发现 `/dev/dri/card*`、优先选择 HDMI connector、选择首选 mode、创建 dumb buffer、设置 CRTC，并在退出时恢复原 CRTC 状态。
- 实现 `--device /dev/dri/cardN` 和 `--help`。
- 修复自动设备选择：跳过没有 connector 的 `card0`，优先选取有 connector 的 `card1`；新增 `--probe` 列出每张 card 的 connector、连接状态和 mode 数量。
- 实现原生 C++/X11 全屏测试程序 `hdmi_x11_kiosk`：深色现代化测试画面、状态卡片、时间和 1 Hz 帧循环。
- 将该程序启动至当前 NVIDIA X 会话 `:1`，并为当前用户配置 GNOME 自动启动项。
- 建立不依赖显示硬件的单元测试，并完成构建与测试。

## 验证结果

| Gate | 命令/方法 | 结果 |
| --- | --- | --- |
| 1：构建 | `make` | 通过 |
| 2：单元测试 | `make test` | 通过 |
| 5：帮助页 | `./build/hdmi_test --help` | 通过 |
| 5：无设备诊断 | 受限执行环境中运行 `./build/hdmi_test` | 通过；返回 1 并明确提示 `/dev/dri/card*` |
| 5：当前设备诊断 | `./build/hdmi_test --device /dev/dri/card1` | 返回 1：通用 DRM connector 中没有已连接且有显示模式的输出 |
| 5：硬件清单 | `./build/hdmi_test --probe` | 通过；`card0` 没有 connector，`card1` 有 `DP-1`，但它报告为 `disconnected` 且有 0 个 mode |
| 5：桌面实际输出 | `DISPLAY=:1 XAUTHORITY=/run/user/1000/gdm/Xauthority xrandr --query --verbose` | 通过；NVIDIA X 驱动显示 `DP-1 connected primary 800x480`、`SignalFormat: TMDS`、有效 EDID |
| 5/6：通用 KMS 实机点亮 | 在本机运行 | 未通过：通用 libdrm connector 状态与 NVIDIA 专有 X 驱动的实际输出不一致 |
| 1：X11 测试画面构建 | `make x11-kiosk` | 通过 |
| 5：X11 冒烟测试 | `timeout 3s env DISPLAY=:1 XAUTHORITY=… ./build/hdmi_x11_kiosk` | 通过；程序持续运行至超时 |
| 5：可见窗口验证 | `xwininfo -name 'HDMI Display Test'` | 通过；窗口为 800×480、`IsViewable`、由 GNOME 管理 |

## 环境观察（2026-07-12）

- 系统：Ubuntu，Linux `5.15.185-tegra`，`aarch64`。
- `libdrm` 开发文件可用（版本 `2.4.113`）。
- 内核 sysfs 已注册 DRM `card0`（`226:0`）与 `card1`（`226:1`），并加载了 `tegra_drm` 与 `nvidia_drm`。
- 当前完整主机环境存在 `/dev/dri/card0`、`/dev/dri/card1` 和对应的 `/dev/char/226:*` 设备节点；当前用户属于 `video` 组，可打开 `card1`。
- 先前“设备节点不存在”的结果来自受限执行环境没有暴露 `/dev/dri`，不是主机的真实硬件故障。
- 通用 libdrm 中唯一可见 connector 是 `card1-DP-1`，其 `status` 为 `disconnected`，没有可用 mode；但这不是当前 HDMI 的真实连接状态，见下方“显示栈结论”。
- 主板型号：`NVIDIA Jetson Orin Nano Developer Kit`；系统为 JetPack 6.2.2 对应的 L4T `R36.5.0`。根据 DRM 的实际枚举，正确显示设备是 `/dev/dri/card1`，当前唯一 connector 被驱动命名为 `DP-1`；`card0` 不是外部显示输出设备。
- Qt 与 CMake 当前未安装；本阶段刻意不依赖它们。

## 外部阻塞与下一步

程序代码已可构建，DRM 设备节点也已可用；但当前 NVIDIA 专有显示栈不向通用 libdrm 路径暴露实际已连接的 HDMI 输出。通用 KMS 原型不能继续作为点亮 HDMI 的实现后端。下一步应验证 NVIDIA 支持的无桌面显示后端；在该决策前保留现有程序作为 DRM 环境探测工具。

```bash
make
./build/hdmi_test
```

若实际 card 不是自动发现的第一个设备，则运行：

```bash
./build/hdmi_test --device /dev/dri/card0
```

在通用 KMS connector 与实际输出状态一致的平台上，预期 HDMI 显示深色测试画面；按 `Ctrl-C` 后程序退出并尝试恢复此前显示状态。

## 后续排查：已排除空闲休眠（2026-07-12）

用户已手动唤醒显示器，但通用 DRM sysfs 状态仍为 `disconnected`。进一步检查后该状态已被证明不代表当前桌面输出。

- `card1-DP-1/status` 仍为 `disconnected`；`enabled` 为 `disabled`，`modes` 为空。
- 没有发现与 HDMI、DP、EDID 或热插拔相关的内核报错。
- 当前内核只向通用 libdrm 暴露 `DP-1` connector。用户确认使用官方载板的原生 HDMI 口，且 X RandR 已证实 TMDS 信号与 EDID 均正常。
- `gdm.service` 当前运行。停止它不会修复通用 libdrm connector 与 NVIDIA 专有 X 驱动之间的状态差异；当前不应仅为重试此原型而停止桌面。

因此当前首要动作不是修改线缆或停止桌面，而是选择并验证与 NVIDIA JetPack 显示驱动兼容的无桌面后端。

## 显示栈结论（2026-07-12）

Xorg 日志和当前 X RandR 查询给出决定性证据：

- NVIDIA 专有 X 驱动识别 `LTM LONTIUM (DFP-1)` 为 `connected`，接口为 `Internal TMDS`。
- 当前活动输出名为 `DP-1`，分辨率为 `800x480@59.97Hz`，具有有效 EDID；`SignalFormat` 是 `TMDS`。
- 当前登录界面正在该输出上显示。

故物理 HDMI 链路完全正常。此前将通用 `/sys/class/drm/card1-DP-1/status` 的 `disconnected` 解释成物理 HDMI 未连接是错误的；正确解释是当前 JetPack NVIDIA 专有 X 驱动与通用 libdrm/KMS connector 枚举并不等价。后续测试应围绕 NVIDIA 兼容的无桌面后端设计。

## 当前可见测试画面（2026-07-12）

为避免在未验证 NVIDIA 无桌面后端前中断正常 HDMI 输出，当前先使用已被证明可用的 NVIDIA X 会话显示原生 C++/X11 全屏测试画面：

- 程序：`build/hdmi_x11_kiosk`；没有使用 pygame。
- 实际会话：`DISPLAY=:1`、`XAUTHORITY=/run/user/1000/gdm/Xauthority`。
- X server 验证：窗口名 `HDMI Display Test`，大小 `800×480`，状态 `IsViewable`。
- 自动启动：`/home/p1ne4pp1e/.config/autostart/hdmi-x11-kiosk.desktop`。
- 常驻服务：`/home/p1ne4pp1e/.config/systemd/user/hdmi-x11-kiosk.service` 已启用且处于 `active (running)`；它将测试程序置于 systemd 用户服务 cgroup 中，并设为异常自动重启。

该路径满足当前“屏幕有可见测试画面”的验证目标，但仍依赖 GNOME/X11；Stage 4 必须验证 NVIDIA 支持的无桌面 kiosk 后端，不能把它视作最终架构。

## 构建系统记录

已添加 CMake 配置。当前系统未安装 `cmake`；尝试通过 `sudo apt-get install cmake` 安装时被 sudo 密码提示阻断。现阶段继续以 Makefile 构建并验证，待可用 sudo 凭据后运行安装并执行 CMake 验证。
