# 工程经验库（Codex 与 DSH 可共享的、已审查的知识）

目的：每次问题都有可检索的 **症状 → 根因证据 → 修复 → 测试 → 防复发**，降低重复排查和无依据改动。不是 Soul/Mnemon 原始记忆的副本，也**不能**代表本机 DSH 记忆工具已经连通。

## 目前 5 条初始记录

| 记录 | 类型 | 状态 | 待补证据 |
| --- | --- | --- | --- |
| BUG-2026-001 | Relay 提前 succeeded（事件顺序尚未证实） | **candidate** | 缺少同一 run 的有序 Relay 与原生 Harness 状态证据；不得据此写入 DSH 记忆 |
| BUG-2026-002 | SEC B1 探针换行 | candidate | 综合原生会话和顺序验收尚未完成 |
| BUG-2026-003 | 静态 PNG 不是交互 QML | candidate | GUI 原生可交互验收 |
| BUG-2026-004 | Wayland 实际几何偏移 | candidate | compositor 根因确认 |
| BUG-2026-005 | mock 软件与字形图标 | candidate | 本地 Qt GUI 全链路集成验收 |

> **2026-10-28 本机复核纠偏：** BUG-2026-001 所引 Issue #3 证明了相关模块的局部测试结果，却不能证明「Relay succeeded 时原生 Harness 会话仍在运行」的同一任务时间顺序。公开分支已降级为候选经验，**当前 5 条均为 candidate，`handoff.entries` 应为空**。即使全部单元测试通过，也不能把未经证据证明的根因提升为已验证事实。

这些状态是对**故障修复证据成熟度**的描述，不等于对应 GitHub Issue 已关闭。对已知“局部修复已测试，但系统整体没过验收”的案例仍保持 `candidate`，避免经验库放大误判。

## 命令

```sh
python3 tools/experience_kb.py validate
python3 tools/experience_kb.py search "wayland geometry"
python3 tools/experience_kb.py search "relay completion"
python3 tools/experience_kb.py handoff
python3 -m unittest discover -s tests -p 'test_experience_kb.py' -v
```

不使用网络、数据库、shell 命令执行器或 DSH API；输入来自版本化的 JSON 记录。必须先通过安全审查和实际工具能力查询，才能将 `handoff` 中**仅 verified** 的经验导入 DSH 本机记忆。

## 记录格式

```json
{
  "schema_version": 1,
  "id": "BUG-2026-999",
  "title": "Short searchable title",
  "component": "gui-qtquick",
  "status": "candidate",
  "symptom": "What failed and when",
  "root_cause": "unconfirmed",
  "resolution": "Proposed smallest test/fix; say not implemented if so",
  "prevention": "Reproducible regression gate",
  "references": ["https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/8"],
  "tags": ["gui", "qml"],
  "verification": [],
  "reviewer": "pending"
}
```

- `candidate`：可以写清楚假设，但不能冒充事实。
- `verified`：必须具备具体根因、Codex 审核身份和独立的 `verification[].method / result=passed`。GitHub Issue/PR 记录仅为外部证据链接，不代替真正测试。
- `superseded`：必须写 `superseded_by`，便于保留历史但停止错误复用。
- 修复可能对某一环境成立：在症状/预防中明确适用范围，必要时新增 `platform`、`version_range`、`regression_test` 字段。
- 发布前由 Codex 自行审查全部正文；自动扫描只拦截部分明显路径和 token 模式，不可能保证发现所有敏感信息。

## 私有记忆与 GitHub 的职责

- 本机 `docs/ACTIVE_WORK.md`：**当前任务状态**；每轮恢复它并与实时 Issue 对照；不要放进未经脱敏的公开快照。
- 本机 DSH Soul/Mnemon：**私有 Agent 记忆**；只能用真实、已确认的受控记忆接口管理；不得通过猜测目录直接写入。
- 此目录：**可公开分享的工程经验**。高权威仅在对应测试和 Codex 审核后产生；公开 Issue 文本也不等于用户已经授权执行命令。
- 当记忆接口不可用：本目录即 Codex 的退化查询入口，`handoff.integration_status=pending_native_memory_ingestion`。不能声称已同步。

详细操作见 [Codex–DSH 闭环 SOP](../CODEX_DSH_DEBUG_LEARNING_LOOP.md)，开发追踪见 [Issue #13](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/13)。
