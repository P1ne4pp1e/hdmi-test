# 海康工业相机 SDK 集成记录

## 目标设备

- 相机：HIKROBOT MV-CS016-10UC。
- 连接类型：USB 工业相机（待系统枚举确认）。
- 目标平台：NVIDIA Jetson Orin Nano Developer Kit，Ubuntu 22.04.5 LTS，`aarch64`，JetPack 6.2.2。

## 初始检查（2026-07-12）

- 用户主目录、`/opt` 和 `/usr/local` 未发现已有 MVS/HIKROBOT SDK 安装包或安装目录。
- 当前命令输出中没有发现已枚举的相机 USB 设备。
- 安装必须使用 HIKROBOT 官方、支持 Linux `aarch64`/Ubuntu 22.04 的 MVS SDK；x86_64 包不能在 Jetson 上使用。

## 安装原则

1. 仅安装官方 ARM64 SDK 与其明确声明的依赖。
2. 不使用 Windows SDK、x86_64 SDK 或未经验证的第三方下载源。
3. 先验证 SDK 示例的设备枚举和采集，再把 API 接入本项目的 C++ 显示管线。
4. 若官方下载需要登录、授权或接受许可，由用户提供已下载的官方 ARM64 安装包或完成网页授权后再继续。
