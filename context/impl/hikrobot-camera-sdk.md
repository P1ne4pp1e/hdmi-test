# 海康工业相机 SDK 集成记录

## 目标设备

- 相机：HIKROBOT MV-CS016-10UC。
- 连接类型：USB 工业相机（待系统枚举确认）。
- 目标平台：NVIDIA Jetson Orin Nano Developer Kit，Ubuntu 22.04.5 LTS，`aarch64`，JetPack 6.2.2。

## 初始检查（2026-07-13）

- 用户主目录、`/opt` 和 `/usr/local` 未发现已有 MVS/HIKROBOT SDK 安装包或安装目录。
- 当前命令输出中没有发现已枚举的相机 USB 设备。
- 安装必须使用 HIKROBOT 官方、支持 Linux `aarch64`/Ubuntu 22.04 的 MVS SDK；x86_64 包不能在 Jetson 上使用。

## 已安装与验证（2026-07-13）

- 安装包：`MVS_Linux_STD_V5.0.1_260512.zip`，SHA-256 为 `cd6c4e3352afb1f6395b9be8a692b4fa8a911ae7eea7ff1f9181970f221bf264`。
- 从包中选择 `MVS-5.0.1_aarch64_20260512.deb`，其 Debian 架构为 `arm64`；已以 `dpkg` 安装，运行时位于 `/opt/MVS`。
- SDK 已提供 C++ 头文件 `/opt/MVS/include/MvCameraControl.h`、ARM64 运行时库 `/opt/MVS/lib/aarch64/libMvCameraControl.so` 和 ARM64 示例。
- 安装器已创建 HIKROBOT USB vendor ID `2bdf` 的 udev 规则；当前用户属于 `plugdev` 组，规则已 reload/trigger。
- 已从官方 `GrabImage` 示例单独编译并成功调用 SDK；当前输出为 `Find No Devices!`，说明 SDK 能正常初始化和枚举，但相机尚未被系统 USB 枚举。
- 安装器建立了 `MvLogServer.service`，目前为 active。
- SDK 安装器的字体复制步骤引用了小写 `fonts`，而包内目录为 `Fonts`，因此报告一次非致命复制错误；SDK 头文件、库和采集示例不受影响。

## 系统库路径

SDK 原安装器只修改用户 shell 的 `LD_LIBRARY_PATH`。额外安装 `/etc/ld.so.conf.d/mvs-aarch64.conf` 并执行 `ldconfig`，让后续 systemd 服务与 C++ 程序无需依赖交互式 shell 也能找到 ARM64 SDK 库。

## 安装原则

1. 仅安装官方 ARM64 SDK 与其明确声明的依赖。
2. 不使用 Windows SDK、x86_64 SDK 或未经验证的第三方下载源。
3. 先验证 SDK 示例的设备枚举和采集，再把 API 接入本项目的 C++ 显示管线。
4. 若官方下载需要登录、授权或接受许可，由用户提供已下载的官方 ARM64 安装包或完成网页授权后再继续。
