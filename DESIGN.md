# HDMI Test Display Design System

## 1. Visual Theme & Atmosphere

界面是一块现代嵌入式性能监视器：背景持续运动以暴露掉帧和撕裂，前景 HUD 克制、精确、易读。视觉参考现代车机诊断页与专业 GPU profiler，而不是传统 PLC 面板。

**Key attributes:** Technical, calm, high-contrast, continuous motion
**Density:** Medium，所有核心指标在 800×480 一屏内完整显示
**Personality:** Professional instrumentation, not decorative dashboard

## 2. Color Palette & Roles

| Token | Hex | Role |
|---|---|---|
| Canvas Deep | `#07111F` | 基础背景 |
| Motion Blue | `#123A63` | 动态网格与波形 |
| Surface Glass | `#111E2ECC` | HUD 半透明面板 |
| Surface Border | `#29415D` | 卡片边框与分隔线 |
| Text Primary | `#F3F7FC` | 标题和主要数值 |
| Text Secondary | `#91A5BC` | 标签和说明 |
| Accent Cyan | `#24D6D0` | FPS、正常状态、运动焦点 |
| Accent Blue | `#4DA3FF` | GPU 指标 |
| Warning Amber | `#FFB454` | 中等负载 |
| Error Coral | `#FF6376` | 高负载与掉帧 |

仅提供深色模式；状态颜色只表达含义，不用作整块卡片背景。

## 3. Typography Rules

字体栈：`Noto Sans CJK SC` → `DejaVu Sans` → sans-serif；数值使用 DejaVu Sans 的等宽数字特性。

| Level | Size | Weight | Line Height | Letter Spacing | Font |
|---|---:|---:|---:|---:|---|
| Screen title | 24px | 600 | 1.2 | 0 | Noto Sans CJK SC |
| Hero metric | 34px | 600 | 1.1 | 0 | DejaVu Sans |
| Card metric | 26px | 600 | 1.1 | 0 | DejaVu Sans |
| Body | 16px | 400 | 1.4 | 0 | Noto Sans CJK SC |
| Caption | 13px | 500 | 1.3 | 0.02em | Noto Sans CJK SC |

最小字号 13px；文本不得溢出卡片或屏幕。

## 4. Component Stylings

### Top status bar
- 高度 64px，左右内边距 24px
- 左侧标题与后端信息，右侧显示 `LIVE`、分辨率和刷新率
- `LIVE` 使用 Accent Cyan 状态点，不使用大面积色块

### Metric cards
- Surface Glass 背景、1px Surface Border、12px 圆角
- 标签在上，主要数值居中偏左，单位弱化
- CPU/GPU/RAM 各使用窄色条；正常、警告、高负载按阈值变色

### Frame telemetry
- FPS 为视觉主指标；同时显示 frame time、frame counter、uptime
- 折线历史图固定保留最近 120 个样本

### Interaction states
- 当前版本无按钮；未来触摸控件最小 48×48px
- Focus：2px Accent Cyan；Pressed：亮度降低 10%；Disabled：不透明度 40%

## 5. Layout Principles

间距采用 4px 基线：4、8、12、16、24、32px。屏幕固定 800×480，安全边距 20px。

- 顶栏：`0–64px`
- 动态测试主画布：全屏背景
- FPS 主卡：左侧 `20,84,280×174`
- CPU/GPU/RAM：右侧三张纵向卡片，分别为 `320,84,460×52`、`320,144,460×52`、`320,204,460×52`；标签左对齐，数值右对齐，数值区域至少保留 160px。
- 底部时间线与系统信息：`20,278,760×178`
- 组件间距 12px，卡片内边距 16px，圆角 12px

## 6. Depth & Elevation

| Level | Styling | Usage |
|---|---|---|
| Base | Canvas Deep | 动态背景 |
| HUD | Surface Glass + 1px border | 指标卡片 |
| Focus | 2px Accent Cyan glow | 关键 FPS 与告警 |
| Overlay | `#07111FE6` | 后续弹层 |

不使用重阴影；嵌入式屏幕通过边框、透明度和亮度建立层级。

## 7. Do's and Don'ts

- DO：背景每帧运动，HUD 保持稳定可读。
- DO：所有坐标由明确布局常量推导。
- DO：使用 FreeType 渲染真实字体并裁剪文本。
- DON'T：使用手写像素字体或缺字回退方块。
- DON'T：用高饱和色填满整张指标卡。
- DON'T：把调试字符串堆在同一行或贴近屏幕边缘。
- DON'T：在窄卡片内并排放置标签与长数值；数值宽度不能证明能容纳时，改用纵向卡片、右对齐和缩写单位。

## 8. Responsive Behavior

当前目标固定为 800×480。若输出尺寸变化：小于 720px 宽时指标卡改为两列；大于 1024px 时保持内容比例并居中，不拉伸字号。触摸目标至少 48px，禁止横向滚动。

## 9. Agent Prompt Guide

实现 UI 前必须读取本文件。快速参考：Canvas Deep 背景、Surface Glass HUD、Accent Cyan 表示实时/正常状态、24px 安全边距、12px 卡片圆角、Noto Sans CJK/DejaVu Sans 字体。所有新增组件必须沿用颜色、字号和 4px 间距体系，并在提交前检查 800×480 无裁切。
