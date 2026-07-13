## Stage 1: 规格与环境基线
**Goal**: 明确首个 HDMI 测试画面的可验证目标，并记录当前图形设备环境。
**Success Criteria**: 需求、验收标准和构建方式已写入项目；运行时会对缺失 DRM 设备给出明确诊断。
**Tests**: `make test` 覆盖测试画面数据；无 `/dev/dri` 时程序返回可读错误。
**Status**: Complete

## Stage 2: 直接 KMS 测试画面
**Goal**: 验证在 Jetson 当前显示驱动栈中可用的无桌面直出路径，并输出测试画面。
**Success Criteria**: 选定的后端能使用当前 HDMI 输出、设置目标模式并显示画面；退出时恢复或由显示服务接管状态。
**Tests**: 单元测试验证画面布局与像素格式；在目标设备执行人工 HDMI 冒烟测试。
**Status**: Complete

## Stage 3: 当前显示栈的可见测试画面
**Goal**: 经当前 NVIDIA X 显示栈在已验证的 HDMI 屏上显示低资源 C++ 全屏测试画面和实时指标。
**Success Criteria**: 原生 X11 测试程序在 `:1` 的 800×480@59.97Hz 屏幕上全屏显示；显示 60 FPS 目标、帧率、CPU、GPU、内存和帧序号；可由服务模板在图形会话后启动。
**Tests**: `make test`、`make x11-kiosk`、`xwininfo -root -tree`、人工核对 HDMI 屏。
**Status**: Complete

## Stage 4: 无桌面 NVIDIA 后端验证
**Goal**: 选择并验证 JetPack NVIDIA 支持的 kiosk/直接输出后端，替代临时 X11 测试路径。
**Success Criteria**: 不依赖 GNOME 桌面仍可输出测试画面，并记录性能与启动行为。
**Tests**: 停止图形桌面后的实机启动、重启后自启动、HDMI 人工审查。
**Status**: Complete

## Stage 5: GPU 压力渲染与共存基线
**Goal**: 以 EGL/OpenGL ES shader 生成动态背景，并建立 Idle、Monitor、Stress 与 YOLO 共存的性能基线。
**Success Criteria**: GPU 背景可由 vsync 驱动稳定输出；HUD 不会成为每帧 CPU 瓶颈；可量化 GPU 压力对推理延迟的影响。
**Tests**: EGL 客户端能连接 Weston、shader 编译成功、`eglSwapInterval(1)` 生效、60 秒帧时间与资源采样记录。
**Status**: In Progress

## Stage 6: Jetson 性能模式与热稳定性
**Goal**: 以当前硬件支持的最高 NVIDIA 电源模式运行显示与后续推理工作负载，并在每次启动后恢复该状态。
**Success Criteria**: 已确认最高有效 nvpmodel 模式；CPU、GPU、EMC 锁定至该模式允许的最高频率；风扇控制持续运行；设置由 systemd 在 Weston/应用启动前应用；存在明确回退命令与温度验证记录。
**Tests**: `nvpmodel -q` 显示最高模式；`jetson_clocks --show` 显示 static max frequency；`tegrastats` 观察 GPU/CPU 频率、温度与功耗；重启服务后 HDMI 仪表盘仍为 active。
**Status**: Complete

## Stage 7: 海康工业相机 SDK 集成基线
**Goal**: 为 MV-CS016-10UC 在 Jetson Orin Nano（Ubuntu 22.04/aarch64）安装官方可用 SDK，并验证设备枚举与首帧采集。
**Success Criteria**: SDK 架构与系统一致；运行时库和开发头文件可被 C++ 项目发现；相机通过 USB 被枚举；官方示例或最小 C++ 程序能打开设备并采集一帧；安装、权限和回退步骤均被记录。
**Tests**: 检查 SDK 版本/架构；`ldconfig` 可发现库；USB 设备枚举；SDK 示例或最小采集程序返回有效图像帧。
**Status**: In Progress

## Stage 8: YOLOv8 TensorRT 双画面验证
**Goal**: 使用 GPU 上的 TensorRT 运行 YOLOv8n，将最新相机帧异步推理，并在 HDMI 上并列呈现原始画面与检测叠加画面。
**Success Criteria**: TensorRT 引擎仅在本机首次构建；相机采集、推理、显示三线程互不阻塞；仪表盘显示推理 FPS 及预处理、推理、后处理平均耗时；模型或相机不可用时有明确状态而显示服务保持运行。
**Tests**: 单元测试覆盖 YOLOv8 输出解码与 NMS；构建通过；启动后确认 TensorRT 使用 GPU、服务日志持续输出推理统计；人工核对两个预览区内容。
**Status**: In Progress

## Stage 9: GPU 预处理与低拷贝叠加
**Goal**: 将相机 BGR 的 letterbox、通道转换、归一化和 CHW 排布从 CPU 移至 CUDA，并减少检测叠加路径的整帧 CPU 拷贝。
**Success Criteria**: 预处理由单个 CUDA kernel 完成；仪表盘仍显示 PRE/INFER/POST；与 CPU 基线比较可量化 PRE 时间和 CPU 占用下降；显示保持 60Hz。
**Tests**: CUDA kernel 的小尺寸 BGR→RGB/CHW 单元测试；`make test` 与 CMake/CTest 通过；实机 `tegrastats` 和仪表盘时间人工核对。
**Status**: In Progress

## Stage 10: GPU 检测框叠加与端到端延迟
**Goal**: 不再在 CPU 复制并绘制整张检测图；在 OpenGL ES 着色器中直接叠加检测框，并为下一步 GPU NMS 留出数据通路。
**Success Criteria**: 右侧预览复用原始相机纹理；检测框由 GPU 绘制、标签继续可读；YOLO 后处理不再包含整帧 BGR 复制；HDMI 保持 60Hz。
**Tests**: 单元测试覆盖检测框归一化；构建和 CTest 通过；实机核对框、标签和原图同步。
**Status**: In Progress
