# 实施记录：直接 HDMI 测试画面

关联规格：[../kits/hdmi-test-screen.md](../kits/hdmi-test-screen.md)

## 已完成

- 以 C++20、Makefile 和 `libdrm` 建立最小测试框架；不依赖 X11、Wayland、Qt 或 SDL。
- 实现纯内存 ARGB8888 测试画面：深色背景、标题栏、网格、三种状态色卡片和 RGB 色条。
- 实现 DRM/KMS 输出：自动发现 `/dev/dri/card*`、优先选择 HDMI connector、选择首选 mode、创建 dumb buffer、设置 CRTC，并在退出时恢复原 CRTC 状态。
- 实现 `--device /dev/dri/cardN` 和 `--help`。
- 建立不依赖显示硬件的单元测试，并完成构建与测试。

## 验证结果

| Gate | 命令/方法 | 结果 |
| --- | --- | --- |
| 1：构建 | `make` | 通过 |
| 2：单元测试 | `make test` | 通过 |
| 5：帮助页 | `./build/hdmi_test --help` | 通过 |
| 5：无设备诊断 | 受限执行环境中运行 `./build/hdmi_test` | 通过；返回 1 并明确提示 `/dev/dri/card*` |
| 5：当前设备诊断 | `./build/hdmi_test --device /dev/dri/card1` | 返回 1：没有已连接且有显示模式的 connector |
| 5/6：实际 HDMI 点亮 | 在本机运行 | 阻塞：`card1-DP-1` 状态为 `disconnected`，没有可用 mode |

## 环境观察（2026-07-12）

- 系统：Ubuntu，Linux `5.15.185-tegra`，`aarch64`。
- `libdrm` 开发文件可用（版本 `2.4.113`）。
- 内核 sysfs 已注册 DRM `card0`（`226:0`）与 `card1`（`226:1`），并加载了 `tegra_drm` 与 `nvidia_drm`。
- 当前完整主机环境存在 `/dev/dri/card0`、`/dev/dri/card1` 和对应的 `/dev/char/226:*` 设备节点；当前用户属于 `video` 组，可打开 `card1`。
- 先前“设备节点不存在”的结果来自受限执行环境没有暴露 `/dev/dri`，不是主机的真实硬件故障。
- 当前唯一可见 connector 是 `card1-DP-1`，其 `status` 为 `disconnected`，没有可用 mode。这是当前 HDMI 实机验证的真实阻塞。
- Qt 与 CMake 当前未安装；本阶段刻意不依赖它们。

## 外部阻塞与下一步

程序代码已可构建，DRM 设备节点也已可用；但显示驱动尚未检测到已连接显示器。必须先让显示器在 connector 状态中显示为 `connected`，才能执行 HDMI 实机冒烟验证。优先检查显示器输入源、线缆与转接器、实际物理输出接口，以及 Jetson 显示输出配置；完成后运行：

```bash
make
./build/hdmi_test
```

若实际 card 不是自动发现的第一个设备，则运行：

```bash
./build/hdmi_test --device /dev/dri/card0
```

预期 HDMI 显示深色测试画面；按 `Ctrl-C` 后程序退出并尝试恢复此前显示状态。
