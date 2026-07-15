# hdmi-test

Jetson Orin Nano 上的无桌面视觉仪表盘技术路线验证项目。目标是在不运行 GNOME/GDM 的情况下，以低额外开销持续输出相机、推理与设备状态画面。

## 当前架构

```text
Hikrobot USB Camera ─┬─> 最新帧缓存 ─> CUDA 预处理 ─> TensorRT YOLOv8n
                     │                                      │
                     └──────────────> OpenGL ES 仪表盘 <─────┘
                                            │
Touchscreen ─> Weston/libinput ─> Wayland wl_touch ──────────┘
                                            │
                                 NVIDIA DRM KMS ─> 物理 HDMI 屏
```

渲染线程由 EGL vsync 驱动；相机采集和 YOLO 推理各自独立线程运行，画面始终展示最新可用结果。

## 运行内容

- 左右并列的原始相机画面与 YOLO 叠加画面。
- CPU、GPU、内存、显示帧率、相机帧率、推理分段耗时与端到端延迟分位数。
- GPU shader 动态背景与 GPU 绘制检测框。
- 最多五点的 Wayland 原生触控可视化：右上角显示 `TOUCH N / 5`，每个活动触点显示青色圆环。

当前触控设备为 `OpenWare Multi-Touch-V5000`。Weston 已将其映射到唯一输出；Linux/NVIDIA 驱动中该输出名称显示为 `DP-1`，对应载板的物理 HDMI 屏。

## 服务与显示恢复

| 服务 | 责任 |
| --- | --- |
| `hdmi-max-performance.service` | 应用 Jetson 性能模式与频率设置。 |
| `hdmi-weston-kiosk.service` | 加载 `nvidia-drm modeset=1`，以 NVIDIA DRM KMS 启动 Weston。 |
| `hdmi-weston-smoke.service` | 启动 `hdmi_egl_stress` 仪表盘。 |

`hdmi-weston-kiosk.service` 会预加载 `nvidia-drm`，这是创建 `/dev/dri/card1` 的必要条件；没有该节点时 Weston 无法接管显示输出。

```bash
sudo systemctl status hdmi-weston-kiosk.service hdmi-weston-smoke.service
sudo journalctl -u hdmi-weston-smoke.service -f
```

## 构建与验证

依赖：C++20、CUDA 12.6、TensorRT 10、OpenGL ES/EGL、Wayland、FreeType、海康 MVS SDK。`models/` 下的 ONNX、TensorRT 引擎不提交到 Git。

```bash
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/hdmi_hik_camera_probe
```

`make` 保留为同等的便捷入口；新增功能应同时保持 CMake 和 Makefile 构建路径可用。

## 实机检查清单

```bash
cat /sys/class/drm/card1-DP-1/status
sudo systemctl is-active hdmi-weston-kiosk.service hdmi-weston-smoke.service
```

期望输出分别为 `connected`、`active`、`active`。然后在屏幕任意位置使用一至五根手指触摸，检查圆环与手指位置一致，并确认画面保持 60 FPS。
