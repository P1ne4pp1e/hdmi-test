# YOLOv8n TensorRT 测试

运行时路径是 `海康采集线程 -> latest-only 缓冲 -> YOLO TensorRT 线程 -> latest-only 叠加帧 -> EGL 显示线程`。

- 推理运行时只使用 C++、CUDA 与 TensorRT，不会加载 ONNX Runtime。
- `models/yolov8n.pt` 是下载的官方权重；首次在本机导出为 `models/yolov8n.onnx`，这是 TensorRT 构建引擎的输入格式，不参与运行时推理。
- 首次启动时，TensorRT 从 ONNX 生成本机专用的 `models/yolov8n_fp16.engine`；之后直接加载该引擎。引擎不得跨 JetPack/TensorRT/GPU 复制。
- 输入固定为 640×640 RGB letterbox，GPU 执行 TensorRT FP16 推理；预处理、GPU 推理、NMS/叠加分别计时。
- HDMI 左侧保留原始相机画面，右侧显示检测叠加画面；底部的显示、CPU、GPU、内存、相机指标保持不变，顶部右侧显示 YOLO 指标。

模型与引擎均被 `.gitignore` 排除，避免把二进制设备产物提交到仓库。

## 2026-07-13 TensorRT 10 绑定结论

本机已成功构建 `yolov8n_fp16.engine`（8.4 MB）。该 ONNX 除最终 `output0` 外，还暴露 `421`、`436`、`451` 三个检测头输出；TensorRT 10 要求它们全部绑定设备缓冲区，否则 `enqueueV3` 会拒绝执行。运行时现按名称枚举并绑定全部输出，仅回读 `output0` 做 NMS 和叠加。

使用本机官方 `trtexec` 验证该引擎可在 Orin GPU 推理：平均 GPU 计算 13.70 ms、端到端延迟 15.38 ms、72.68 QPS。服务运行时采样到 `GR3D_FREQ 80%@624MHz`，HDMI 输出保持 60 FPS。
