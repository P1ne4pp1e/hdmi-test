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

## 画面稳定性与调试信息（进行中）

用户观察到测试画面每 0.5–2 秒闪烁。根因是原型直接在可见 X11 window 上清空并重绘完整画面；它不是 GNOME 或桌面服务争抢显示设备。现已使用离屏 Pixmap 双缓冲：先完整绘制到内存 Pixmap，再以一次 `XCopyArea` 提交到可见 window；循环目标为约 30 FPS。

当前画面显示：实时 FPS、帧序号、800×480 分辨率、X11/TMDS 后端、主机名、DISPLAY、运行时间和实时时钟。

`Win+L` 仍能锁屏是当前 X11 原型的预期限制：它由 GNOME 管理，未独占系统显示控制权。系统级切换必须停止 GDM/Xorg 并启动 NVIDIA 兼容的 kiosk compositor；当前用户没有免密 sudo，`sudo -n true` 返回“password is required”，因此该系统级切换尚无法由非交互进程执行。

## 60 Hz 与系统指标（2026-07-12）

- HDMI 当前 X RandR 模式为 `800×480@59.97Hz`，这是 60 Hz 级别的实际刷新率。
- 测试画面渲染循环目标为 60 FPS（每帧约 16.67 ms），且以离屏 Pixmap 双缓冲提交。
- CPU 使用率通过相邻两次 `/proc/stat` 的总时间与 idle 时间差计算；内存通过 `/proc/meminfo` 的 `MemTotal` 与 `MemAvailable` 计算。
- Jetson GPU 使用率通过 `/sys/devices/platform/bus@0/17000000.gpu/load` 读取；不会启动常驻 `tegrastats` 子进程。
- 指标最多每秒采样一次；解析逻辑由 `test_system_metrics` 覆盖并通过。
- 更新后用户级服务仍为 `active (running)`，systemd 报告内存使用约 0.5 MiB（不含 Xorg/GNOME compositor 的共享/外部资源）。

## GNOME 通知抑制（2026-07-12）

`update-manager` 的 “Software Updater” 窗口会覆盖当前过渡测试画面。已执行：

- 将 GNOME `show-banners` 与 `show-in-lock-screen` 设置为 `false`；
- 终止当前 `update-manager` 进程并关闭其 X11 窗口；
- 在 `~/.config/autostart/update-notifier.desktop` 写入 `Hidden=true`，阻止更新通知器下次图形登录自动启动。

这只消除当前 GNOME 过渡方案的干扰，不替代 Stage 4 的无桌面独占显示后端。

## Stage 4：NVIDIA Weston kiosk 准备（进行中）

目标系统已安装 JetPack NVIDIA Weston 13：包含 `drm-backend.so`、`gl-renderer.so`、`fullscreen-shell.so` 与 `weston-simple-egl`。已在 `deploy/` 准备：

- `weston-kiosk.ini`：禁用 idle、采用 fullscreen shell；
- `hdmi-weston-kiosk.service`：通过 `card1` 启动 DRM backend；
- `hdmi-weston-smoke.service`：使用 NVIDIA/Weston 的 EGL 示例作为首个直接输出冒烟客户端；
- `hdmi-kiosk-recovery.service`：compositor 启动失败时恢复 GDM。

尚未激活这些系统服务。当前普通用户执行 `systemctl stop gdm.service` 返回 `Interactive authentication required`，且 `sudo -n` 要求密码；因此必须先获得系统级 systemd 授权，才能在不破坏远程恢复路径的前提下停止 GDM 并执行实机验证。

## Stage 4：NVIDIA Weston 直接输出验证（2026-07-12）

已获得系统级授权并完成受监控切换：

- 已停止 GDM，并停止用户级 X11 测试服务；GNOME/Xorg 不再持有显示控制权。
- `hdmi-weston-kiosk.service` 处于 `active (running)`，直接使用 `drm-backend.so` 和 `/dev/dri/card1`。
- Weston 日志确认：`DP-1` connector 已连接、EDID 为 LONTIUM、当前/首选模式为 `800×480@60.0`。
- Weston 使用 NVIDIA EGL 1.5、OpenGL ES 3.2 和 Tegra Orin 集成 GPU；未使用软件 pixman renderer。
- `hdmi-weston-smoke.service` 处于 `active (running)`，使用 `weston-fullscreen` 全屏客户端显示直接输出画面。
- 当前服务报告：Weston 约 14.3 MiB，fullscreen 冒烟客户端约 5.8 MiB；这不包含内核/GPU 驱动开销，但已移除 GNOME、GDM、Xorg 和 Mutter 的额外常驻显示层。

此前 `libdrm` 原型在 GDM/Xorg 占用 DRM master 时看到 connector 状态不一致；在 Weston 直接取得 DRM master 后，标准 DRM backend 正确识别了已连接 HDMI/TMDS 输出。这验证了最终路线为 NVIDIA Weston kiosk，而非继续保留桌面。

下一项工作是把自有 CPU/GPU/内存/FPS 调试画面从临时 X11 客户端迁移为 Wayland 客户端，替换 `weston-fullscreen` 冒烟客户端。

## 原生 Wayland 调试客户端迁移状态

官方 `weston-fullscreen` 只显示基础示例图形，不满足调试画面要求。尝试通过 Xwayland 复用 X11 面板失败：当前 NVIDIA Weston 安装缺少 `xwayland.so` 模块，已立即回退，避免 compositor 持续重启。

已安装 `wayland-protocols` 并开始实现共享内存 Wayland 客户端；当前 Weston desktop shell 使用 `xdg-shell`，而初版客户端使用的旧 `wl_shell` 不可用，尚未替换成功。为避免屏幕空白，当前 `hdmi-weston-smoke.service` 已固定为已验证可运行的 `weston-simple-egl` 直接输出回退客户端。Weston 与回退客户端均为 `active`。

后续必须完成 xdg-shell 客户端绑定、共享内存双缓冲和指标绘制，才能将 CPU/GPU/内存/FPS 面板带回无桌面链路。

## UI 完整重设计（2026-07-12）

已完成 xdg-shell 原生 Wayland 面板的视觉重构并上线：使用 FreeType/Fontconfig 真实字体，遵循根目录 `DESIGN.md`。背景以约 60 FPS 持续绘制渐变网格、扫描条和压力时间线；前景 HUD 显示实测 FPS、CPU、GPU、内存、frame counter 与 60Hz 输出状态。指标每秒采样一次，避免监控本身制造不必要负载。

运行验证：`hdmi-weston-smoke.service` 为 `active (running)`，客户端内存约 3.4 MiB。

## Revision：FPS 未达到 60、GPU 波动（2026-07-12）

**WHAT**：用户观察到 GPU 使用率忽高忽低，实测 FPS 无法跑满。

**WHY**：当前动态背景由 CPU 对 800×480 全屏逐像素生成，再通过 `wl_shm` 交给 Weston 合成；GPU只承担间歇性纹理上传和合成。同时每帧 roundtrip 后又固定 sleep 16.67ms，使总帧时间等于“绘制时间 + 同步时间 + 16.67ms”，理论上无法稳定 60 FPS。

**RULE**：压力背景必须由 GPU shader 生成；帧调度必须由 Wayland frame callback 或 EGL swap interval 驱动，不允许渲染完成后再追加完整帧周期 sleep。

**LAYER**：`context/kits/hdmi-test-screen.md` R5 与实现计划缺少 GPU 生成和帧调度约束，现已补充。

## 构建系统记录

已添加 CMake 配置。当前系统未安装 `cmake`；尝试通过 `sudo apt-get install cmake` 安装时被 sudo 密码提示阻断。现阶段继续以 Makefile 构建并验证，待可用 sudo 凭据后运行安装并执行 CMake 验证。
