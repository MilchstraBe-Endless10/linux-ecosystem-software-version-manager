# ADR-001 · Qt Quick/QML + C++ 与 Flutter 双框架评估

- **日期**：2026-10-27
- **项目**：Linux 全生态软件版本管理中心（Omarchy / Arch / Hyprland / Wayland）
- **状态**：推荐首选；**尚未经过目标设备实机样机验证，不是最终冻结架构**
- **输入**：PRD V3.0 正式浅色动漫浮空城视觉、工作台 WB3、软件清单 COL3、详情 DET3/CMP3/TAG3、系统升级 K3 与安全约束。

## 决策摘要

**可以在一个项目里同时使用 Qt Quick/QML + C++ 和 Flutter，但将 Flutter 作为 Qt 主窗口中的动画/局部 UI 引擎并不值得。** 它把原本一套可靠的 UI 变成两套渲染引擎、两套组件模型和事件处理生命周期。Flutter Linux 桌面是正式支持的目标，但 Flutter 的 Add-to-app 嵌入功能目前**没有列 Linux 为受支持平台**，因此单窗口混合方案必须承担专门的宿主、GPU surface、输入法、窗口协议和版本维护工作。

**推荐：Qt 6 Qt Quick/QML（唯一 UI）+ Qt/C++ 应用层和数据模型 + 单独的最小权限安装执行边界。** 不需要 Flutter 来实现光效、场景视差、花瓣/节点动态、弹性详情过渡；基础 QML 动画与 GPU Shader 足以做第一阶段特效。Flutter 保留为竞争性独立 PoC，若 Qt 在目标硬件未达标可以整体切换，而不是硬拼二者。

## 1. 原理与可行性

- **Qt Quick/QML + C++ 是自然组合**：QML 表现与布局、C++ 负责 model/view、来源识别和应用 API，C++ 类型直接暴露给 QML。
- **Flutter + C++ 也是自然组合**：Dart/Flutter 单 UI + dart:ffi 或 Linux 平台插件桥接 C/C++ 后端。
- **Qt + Flutter 同窗不是自然组合**：两者均需要自身的 UI 生命周期/事件调度/渲染表面；Qt 提供与其他 UI 工具包通用混用接口，但 Flutter Linux Add-to-app 未官方支持。自己编写 embedder 并非理论不可能，却是高维护风险项目。
- **Qt 主窗 + Flutter 独立窗/进程**：可以通过本地 IPC 交换状态，但视觉上成为两个由 Wayland compositor 管理的窗口，无法自然替代 Qt 内部 QML 元素；额外进程引入资源和发布成本。
- **Flutter 仅用于离线视觉 PoC**：无运行时依赖问题，但是同一交互要迁移重写，低优先级。

## 2. 多维度比较（面向本项目的工程判断，非实测跑分）

| 考虑项 | Qt Quick/QML + C++ 唯一 UI | Flutter + C/C++ 唯一 UI | Qt + Flutter 同产品双 UI |
|---|---|---|---|
| V3.0 动漫背景/流光/复杂转场 | 强。QML transition、ShaderEffect、MultiEffect、可选 Scene Graph | 强。动画组件生态丰富 | 分开看皆强，统一特效层和焦点复杂 |
| 1 万软件/多实例大表 | C++ 模型 + 虚拟化 TableView，机制成熟 | 可实现，需要表格虚拟化和原生桥接 | 状态和选择同步重复劳动 |
| Hyprland Wayland | Qt Wayland 客户端明确受支持 | Flutter Linux 桌面支持；目标环境需测试 | 跨 GUI toolkit 的嵌入与焦点风险较高 |
| 安全更新事务 | 原生 C++ 模块与独立执行边界明确 | 原生 plugin/FFI + 独立执行边界 | 双引擎 IPC/权限边界复杂度无必要 |
| 动画层资源消耗 | 一套主要渲染器，GPU 压力须实测 | 一套主要渲染器，GPU 压力须实测 | 双引擎/双窗口潜在额外资源，不能假定轻量 |
| UI 研发效率 | QML 比纯 Qt Widgets 灵活，需 C++ 配合 | 组件、状态、转场开发速度通常较高 | 两套 UI/渲染调试翻倍倾向 |
| 长期维护 | 单框架版本矩阵，Qt 模块许可须审查 | 单框架版本矩阵，GTK/plugin 适配须审查 | 双框架升级矩阵、生命周期与相容性测试更多 |
| AI 辅助代码可靠性 | 功能/视觉职责清晰，文档较完整 | 社区组件与示例丰富 | 混合嵌入样例少、边缘行为不易验证 |
| 开源许可证 | Qt LGPL/GPL 模块分别核验，商标/素材另审 | Flutter SDK 宽松许可，Native/plugin 另审 | 两套以及桥接插件全部核验 |
| 失败时回退 | 可以保留 C++ 数据服务，整体替换 UI | 同理 | 容易形成难以拆除的相互依赖 |

## 3. 推荐职责结构（不预先冻结具体进程与服务）

```text
+ Qt Quick/QML：正式界面唯一渲染/交互体系
  - 静态视觉 V3.0、2.5D 视差、动效层、窗口自适应
  - UI-A、WB3 拖拽、DEN-C、COL3、DET3/CMP3/TAG3
+ Qt/C++：读模型与业务编排
  - 模型：QAbstractItemModel/QAbstractTableModel -> QML TableView
  - 安装实例和来源映射、更新计划、权限要求、日志与通知状态
+ 独立的更新执行安全边界（具体技术后续安全设计）
  - 事务、Omarchy 完整升级、源可信验证、适配器、操作审计
  - 只有获得批准的需要权限操作才被授权；GUI 永远不以 root 常驻
```

从左向右的数据流由业务状态驱动动画，不是动画决定版本/任务事实。关闭 GUI 后的 G2 执行、可选后台 C2 由任务系统独立处理；特效不参与任务生命期。

## 4. 与既定素材包的匹配

优先复用 V3.0 中原图和 206 个坐标裁切；保留主场景、银发角色、白猫、樱花和发光节点。

- 先把静态母稿的版式、清单行、详情区真实重建，确保 UI 可操作，不以整张 PNG 假造功能。
- 再制作 2.5D 背景层/角色透视素材：城堡远景、瀑布、前景花瓣、角色主体。**原 PNG 并不含无遮挡分层源素材**；必须确立补充美术资产。
- QML SpringAnimation/Transitions 负责卡片弹出、悬停反馈、选中导航；ShaderEffect/MultiEffect 负责局部泛光/柔和模糊；列表滚动、风险文案减少动效。
- 不默认依赖 Qt Quick 3D 或 Qt Lottie，因为其当前开源模块许可证可能只有 GPLv3；有需要时先评审软件许可证和具体模块版本。

## 5. 高风险点与缓解

| 风险 | 后果 | 验证/缓解 |
|---|---|---|
| Qt 与 Flutter 无 Linux 官方 Add-to-app 标准路径 | 耗费自定义嵌入维护成本 | 不采用双 UI 主路线；只保留 Flutter 单独 PoC |
| Wayland 独立窗口嵌套、IME/焦点等 | 透明窗口、输入与布局不稳定 | 使用 Qt Wayland 单 UI，在真实 Hyprland 评估 |
| 大面积漫反射/半透明加动态纹理 | GPU、耗电升高 | 设置动效等级、暂停不可见层；量测真实指标 |
| TableView 行中保存选中/动效状态 | 虚拟化回收出现错误行 | 模型为单一状态真源，复用委托重置/暂停 |
| C++ 内存与执行权限问题 | 系统级故障或错误更新 | 严格执行边界、输入过滤、编译检查和安全测试 |
| Qt Quick 3D / Lottie 模块 GPL 限制 | 与选定许可证冲突 | 选许可证前禁止默认用这些模块 |
| 已确认视觉母稿是扁平 PNG | 难以产生精确角色动作 | 资产缺口清单，分层美术补齐，不归咎于 UI 框架 |

## 6. 阶段性可执行验收（计划，并非已完成）

1. **同图还原**：逐层构建 V3.0 左导航、场景、悬浮详情、底部表格，视觉对齐，不以整图冒充可交互 UI。
2. **动画系统**：花瓣流动、节点呼吸、卡片浮现/状态过渡；支持 reduced motion 和全部循环动效关闭。
3. **数据容量**：10,000 个模拟实例，排序/筛选/冻结列/可编辑列、多实例对比；记录实际滚动帧时间和峰值内存。
4. **Hyprland**：Wayland 原生启动、分数缩放、平铺缩窄、中文输入法、键盘焦点、拖放与多窗口生命周期。
5. **任务安全**：只读无需特权；手动批准的模拟事务；拒绝权限；GUI 退出后任务数据和审计继续完整。
6. **打包许可**：所有 Qt 模块、字体、品牌、插画和动画资源逐项列出许可证与分发义务。
7. **对照验证**：只有 Qt 样机达不到目标体验或 Wayland 稳定性要求时，再做独立 Flutter 单 UI 样机进行同条件比较。**不把两个渲染器合成默认设计。**

## 7. 官方资料（2026-10-27 核验）

- Flutter Add-to-app 平台：https://docs.flutter.dev/add-to-app
- Flutter Linux 构建、FFI：https://docs.flutter.dev/platform-integration/linux/building
- Flutter 桌面支持：https://docs.flutter.dev/platform-integration/desktop
- Qt Quick + C++：https://doc.qt.io/qt-6/qtquick-index.html
- Qt Quick 状态、动画：https://doc.qt.io/qt-6/qtquick-statesanimations-animations.html
- Qt MultiEffect：https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html
- Qt Quick TableView：https://doc.qt.io/qt-6/qml-qtquick-tableview.html
- Qt Wayland Client：https://doc.qt.io/qt-6/qtwaylandclient-index.html
- Qt 混合其他 UI Toolkit：https://doc.qt.io/qt-6/mixing-qt-with-other-ui-toolkits.html
- Qt 许可：https://doc.qt.io/qt-6/licensing.html

### 裁定

**Qt Quick/QML + C++ 是首选，但要经过实际原型验收才算架构冻结。Flutter 不进入首发正式 GUI 的运行时。**
