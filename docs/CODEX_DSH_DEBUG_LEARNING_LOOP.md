# Codex ↔ DSH：有证据的排障、修复、回归与经验同步

> 版本：2026-10-28。用于 Codex 主导 + DeepSeek Harness (DSH) 受控执行。此流程**不能替代** SEC-001 的操作系统级权限隔离。公开本文件不表示本地 DSH 记忆已同步。

## 触发与任务恢复

每次接触 GUI、Core、SEC 或 Relay 相关任务：

1. Codex 从**当前本地** `docs/ACTIVE_WORK.md` 恢复所有未完成事项，并对照 GitHub Issue/PR 状态。若本地文件不存在，记录缺失并以真实 GitHub 状态恢复；不要从旧聊天编造进度。
2. 针对症状运行 `python3 tools/experience_kb.py search "wayland geometry"` 或更精确关键词；只有本地已验证的经验才能直接作为已知修复先例。
3. 若 DSH 提供了实际注册的记忆 `query_memory` 接口，Codex 先列能力、使用最小相关键只读查询；**不要猜测插件名称**、直接遍历 Soul/Mnemon 原始会话、或使用广域 Relay `list_runs` 查找经验。若工具不可用，直接使用 versioned KB + 已有 Skills 继续，不因记忆接口缺失停工。
4. 相同症状不等于相同根因。记录平台版本、复现条件、预期/实际、最小日志、相关 commit、已有测试和影响范围，检查是否属于旧 BUG 的复发。无根据的修复不得执行。

## 七阶段闭环

| 阶段 | 谁负责 | 要求与交付 |
| --- | --- | --- |
| 1. 归档故障 | Codex | `candidate`，最小复现、基线、失败证据及影响面；不要立刻填“已解决” |
| 2. 检索经验 | Codex + DSH 只读能力 | 搜索本地 verified 记录、精确 GitHub Issues 和 DSH 已验证记忆；防止重复探索 |
| 3. 外部研究 | Codex 指挥 DSH 研究 | 优先**官方文档、API、工具链、源代码**；记录网址、适用版本、与现场证据的差异；社区帖子只作补充 |
| 4. 根因假设 | Codex | 区分观察与假设，提出能证伪的测试。根因未知时不得声称“修复” |
| 5. 小范围修复 | DSH 受控执行（适合时） | Codex 定义实际 Relay workspace/provider、只读/可写范围、排除路径、允许命令、测试、回退；DSH 只做隔离模块，不私改主 QML/视觉基准 |
| 6. 独立复验 | Codex | Relay `succeeded` 不等于原生会话成功；先确认原生终态和产物稳定，再检查 diff、重跑回归、必要时做 Wayland/视觉/性能实测，确认没有越权 |
| 7. 知识沉淀 | Codex 验收后 | 经验证才由 `candidate → verified`；记录根因、修复、独立测试、复发预防、适用边界和 Issue/PR。脱敏后进入共享 KB；仅通过**真实可用**的 DSH 记忆工具同步，并验证查询结果 |

**失败路径：** 未确认根因 → `candidate`；测试不通过 → `changes_requested`；原生终态不明 → `unverified`；写入越界/安全授权不足 → stop + report。不要悄悄重复派发 Relay run；不要用一次 QML 离屏成功替代原生 Wayland 验收。

## DSH 任务契约（每次单独填写）

- `issue_id`、`incident_id`、基线 revision、关联历史经验 ID。
- **症状与可复现证据**（只提供路径和范围，不粘贴私有会话/凭证）。
- 已排除的原因、尚待证伪的根因假设、外部文献 URLs 与版本。
- 准确 `reviewTargets` / `contextReadScope` / `excludedPaths` / `writeScope`、允许命令和模型路由（实际查询）。这些是任务边界声明，**不是 OS 级 ACL**。
- 任务完成条件：changed paths、diff summary、测试命令/退出码、可重复复现步骤、剩余风险、回退办法；Codex 独立验收。
- 若修复影响人机交互 QML，则 **Codex 仍拥有 GUI 与最终视觉验收**，DSH 可处理故障定位、mock 模型、适配器和单测。

## 记忆对接：可信知识与私有记忆分离

共享权威源顺序：产品负责人批准的 PRD/视觉基准与安全决策 → 已验收的源码/测试 → verified 工程 KB/ADR → candidate 候选 → Agent 私有临时记忆。私有 DSH Soul/Mnemon 不能反向覆盖更高优先级决策。

- **Codex 查询入口**：`python3 tools/experience_kb.py search "关键词"`，以及经过明确授权的 DSH `query_memory`。
- **Codex 验证后**：先执行 `python3 tools/experience_kb.py validate`、相关回归测试，得到单条 verified 记录；再运行 `python3 tools/experience_kb.py handoff` 生成审核用 JSON。
- **DSH 记忆写入**：只有当 Harness 当前实际发现可写 `remember`（或等价受控记忆动作）时，Codex 才可在权限范围内调用，并再次查询刚写入的 `incident_id` 核实持久化。必须记录 `synced/failed/unavailable`，不得把生成 JSON 描述为已写入 Soul。
- **无记忆工具兜底**：私有 `docs/ACTIVE_WORK.md` 与受控的私有工作区知识资料仍可工作；待同步状态固定为 `pending_native_memory_ingestion`，不因记忆插件缺失反复中断开发。
- **公开边界**：个人会话、真实主机路径、Loopback Relay、私有 Soul、原始日志、美术素材、token、未公开软件清单都不得提交；公开 KB 只放经过 Codex 核验的脱敏技术案例。CI 静态脱敏检查只是第一道筛网，Codex 仍需人工审查。

## 应优先解决的当前问题，不是空泛“优化”

| 现象 | 必要证据 | 推荐查证依据 | 交付定义 |
| --- | --- | --- | --- |
| Wayland 941×1030 目标尺寸失效 | Qt QWindow geometry + compositor 客户端/工作区规则 + DPR | Qt QWindow、Hyprland 实际窗口规则 | 实机目标尺寸对照成功或证实限制；不能靠离屏截图 |
| 原图 1:1 静态但 QML 不等同 | 页面点击/拖拽、分层截图、动态录屏、复用 delegate | Qt QML Profiler/QtQuick 性能文档 | 控件真可交互，关键构图可对照 |
| 渲染长帧、动画负载 | 目标实际几何、帧时间 P95/P99、GUI 线程事件、CPU/GPU、idle | Qt QML Profiler 官方工具 | 定位耗时绑定/渲染层；先测量再改 Shader |
| 本机扫描与图标错误 | 包数据库、desktop ID、Icon=、manager/安装作用域证据 | Freedesktop 规范、Qt QIcon | 真图标与真版本，未检查不冒充最新 |
| Relay 终态乱序 | 精确 run/session 关联、产物指纹、原生运行状态 | 实际 Relay/DSH 工具契约 | 不重派、结果幂等、Codex 独立验收 |

## 依据

- Google SRE：有事实证据的故障复盘、可验证的预防行动与明确负责人：https://sre.google/workbook/postmortem-culture/
- Qt 官方 QML Profiler：https://doc.qt.io/qt-6/qtquick-profiling.html
- Qt 官方性能分析：https://doc.qt.io/qt-6/qtquick-performance.html
- Qt 官方原生主题图标：https://doc.qt.io/qt-6/qicon.html
- Codex 工程经验持续存储与 AGENTS：https://developers.openai.com/cookbook/examples/codex/iterating-development-workflows-with-codex
- GitHub 外部事件数据不可直接作为可执行指令：https://docs.github.com/en/actions/concepts/security/script-injections

> 此文件是工程 SOP，**不会自动向本机 Relay 派发任务**。Codex 必须真正执行、留证并验证后，才能把任务状态标成完成。
