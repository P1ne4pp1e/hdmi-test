# hdmi-test

Jetson Orin Nano 上的无桌面 HDMI 视觉仪表盘测试项目。

## 当前测试程序

当前运行路径：海康工业相机采集 → CUDA 预处理 → TensorRT YOLOv8n → OpenGL ES 检测框/标签叠加 → Weston DRM HDMI 输出。

系统服务：

- `hdmi-weston-kiosk.service`：Weston NVIDIA DRM 后端。
- `hdmi-weston-smoke.service`：`hdmi_egl_stress` 仪表盘。
- `hdmi-max-performance.service`：Jetson 性能模式。

## 依赖

- C++20、CUDA 12.6 NVCC、TensorRT 10、OpenGL ES/EGL、Wayland、FreeType、海康 MVS SDK。
- YOLO 权重与本机 TensorRT 引擎位于 `models/`，不提交到 Git。

## 构建与验证

```bash
make test
make
./build/hdmi_hik_camera_probe
```

运行中的服务状态：

```bash
sudo systemctl status hdmi-weston-smoke.service
```

## CMake

项目已提供 `CMakeLists.txt`。安装系统 `cmake` 后可使用：

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```
