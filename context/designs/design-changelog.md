# Design Changelog

## 2026-07-12：完整重设计调试画面

- 建立根目录 `DESIGN.md` 九节视觉规范。
- 将手写 5×7 像素字体替换为 Fontconfig + FreeType，使用系统 Noto Sans CJK/DejaVu Sans。
- 将三块高饱和色卡重构为深色动态压力测试背景和半透明 HUD。
- 重排 800×480 布局：64px 顶栏、FPS 主卡、CPU/GPU/RAM 指标区、底部运动压力时间线。
- 动态背景以每帧运动的渐变、网格和扫描条验证刷新连续性；指标每秒采样，画面目标 60 FPS。
- FPS 改为实际循环测量值；内存数值使用较小字号，避免右侧裁切。
