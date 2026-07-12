# Jetson 最高性能设置记录

## 目标

让 HDMI 仪表盘与后续本地 YOLO/TensorRT 工作负载使用本机硬件实际支持的最高稳定频率，而不是按需降频。

## 初始基线（2026-07-12）

- `nvpmodel -q`：`10W`，模式 ID 为 `0`。
- 当前 `/etc/nvpmodel.conf` 的硬件专用配置只定义 `10W`、`7W_AI`、`7W_CPU` 三种模式；因此 ID `0` 已经是此设备/当前 BSP 支持的最高模式，不替换为其他 Jetson 型号的 `MAXN`/`SUPER` 配置。
- CPU governor：`schedutil`。
- GPU governor：`nvhost_podgov`。
- 最高频率上限：CPU `1510400 kHz`、GPU `624750000 Hz`。
- `nvfancontrol.service` 已启用且运行。

## 应用策略

1. 保持 nvpmodel 在模式 `0`；它是当前硬件支持的最高功耗预算。
2. 用 `jetson_clocks` 锁定 CPU、GPU、EMC 到该预算允许的最高频率，并使用 `--fan` 防止持续负载热降频。
3. 安装 `hdmi-max-performance.service`，确保设置在 Weston 与显示客户端前完成并在开机后恢复。
4. 不替换 nvpmodel 硬件配置、不修改电压、不超频；这些做法会越过 NVIDIA 针对当前模块定义的安全边界。

## 已应用与验证（2026-07-12）

- 已执行 `nvpmodel -m 0` 和 `jetson_clocks --fan`。
- 已安装并启用 `hdmi-max-performance.service`；它在每次启动时重新应用上述设置，并排在 Weston/HDMI 客户端之前。
- `jetson_clocks --show` 验证：6 个 CPU 核全部固定 `1510400 kHz`，GPU 固定 `624750000 Hz`，EMC 固定 `2133000000 Hz`，CPU idle states 已禁用，风扇 PWM 为 `255`。
- 10 秒 `tegrastats` 验证：CPU `1510 MHz`、GPU `624 MHz`、EMC `2133 MHz` 持续保持；GPU 温度约 `43–44°C`，没有温度限制；HDMI 客户端持续记录 `60.0 FPS`。
- 未禁用 Weston、NVIDIA 驱动、`nvfancontrol` 或其他业务服务；高性能设置只锁定硬件频率与风扇。

## 回退

恢复动态调频并重启：

```bash
sudo systemctl disable --now hdmi-max-performance.service
sudo reboot
```

当前 nvpmodel 的最高模式本身为模式 `0`；若需要低功耗模式，按 `nvpmodel -q` 和 `/etc/nvpmodel.conf` 中列出的 ID 显式选择。
