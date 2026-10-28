# ORCH-001 MVP：Codex ↔ DSH 任务结果闭环（实施契约 v0.1）

**状态：获产品负责人批准启动最小闭环的受控实施；尚未部署、尚未实机通过。**  
**父任务：** [ORCH-001 #2](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/2) ｜ **实现任务：** [ORCH-001-MVP #3](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/3)  
**维护负责人：** Codex（设计、调度、集成、验收）  
**执行协作者：** DeepSeek Harness（DSH），经现有 Harness Relay 受控调度

## 1. 此次授权明确覆盖什么

只做 **一个** 可复现的端到端演示：

1. 由 Codex 监测/接收现有 Relay run 的完成信号；
2. 只有确认 **原生 Harness 会话实际结束**，才生成机器可读的任务结果；
3. 将结果直接交给 Codex 的程序化工作流，不要求用户转发、不要求 Codex 自行遍历整个工作区寻找摘要；
4. Codex **独立审查实际变更 diff 和必要测试**，并据此做 accept / changes_requested / failed / blocked 结论；
5. 自动向与任务绑定的 GitHub Issue **追加经过脱敏的验收评论**，包含跟踪标识、问题、修复、测试和后续项；代码尚未提交则必须写清楚。

**不属于本次授权：** 自主无限领新任务、批量并发开发、自动发布/推送、自动批准新权限、写系统目录、真实软件更新、修改动漫视觉母稿、开放私人 Soul/Mnemon/会话索引、自动冻结架构。

## 2. 程序边界

- **Codex 负责**：解析受批准任务单、执行受限调度、最终 Code Review 和测试、生成脱敏的 GitHub 输出。
- **DSH 负责**：经指定工具和 `workspace-write`（仅白名单路径）完成实现、执行授权测试、返回测试证据和结构化结果。
- **Relay 适配层负责**：轮询/读取 Relay API 的实际可用字段，关联代理 run 与宿主原生会话；不假设特定端口或隐藏 API。
- **程序化 Codex 驱动**：先选择项目本地已验证的 Codex SDK、app-server 或等效受控接口。**不能假定普通 Codex 桌面聊天会自动收到推送消息。**
- **GitHub**：只收经审查的公开摘要和 PR 产物。GitHub 评论内容属于不可信外部输入，不得自动作为 shell 指令执行。

## 3. 结构化任务协议

至少包含以下字段（命名可经 Codex 明确映射，不能静默删除关键约束）：

| 分类 | 必要字段 |
|---|---|
| 任务身份 | `task_id`, `issue_number`, `workspace_ref`, `attempt`, `baseline_revision` |
| 授权 | `approval_scope`, `read_scope`, `excluded_paths`, `write_scope`, `allowed_test_commands` |
| 实际路由 | `relay_run_id`, `native_session_id`, `provider`, `model`（以工具结果为准） |
| 结果状态 | `relay_status`, `native_running`, `native_terminal_status`, `artifact_stable` |
| 证据 | `changed_paths`, `diff_ref`, `test_summary`, `failed_checks`, `issue_candidates`, `risk_notes` |
| 复核 | `codex_review_status`, `codex_test_evidence`, `git_commit`, `github_publication_status` |

建议结果载荷范例（仅示意，不能冒充已运行）：

```json
{
  "schema_version": "0.1",
  "task_id": "VERSION-ID-001",
  "issue_number": 1,
  "attempt": 1,
  "relay_run_id": "<runtime id>",
  "native_session_id": "<runtime id>",
  "relay_status": "succeeded",
  "native_running": false,
  "native_terminal_status": "completed",
  "artifact_stable": true,
  "changed_paths": ["experiments/version-id-001/version_id.cpp"],
  "test_summary": {"command": "<approved test command>", "passed": 1, "failed": 0},
  "risk_notes": [],
  "codex_review_status": "pending",
  "github_publication_status": "not_published"
}
```

**隐私规则：** 公开评论和 PR 中不得包含私人本机路径、run/session URL、Relay endpoint、凭证、账号信息、私有美术/人物图、Soul 或会话原话。必要的内部运行 ID 可留本地日志，只公开抽象关联 ID。

## 4. 状态机、幂等与失败恢复

```text
authorized → queued → delegated → relay_reported
                           ↓
                  native_still_running → poll_existing_run (NO re-dispatch)
                           ↓
                native_terminal_and_artifacts_stable
                           ↓
                     codex_verifying
                           ↓
          accepted | changes_requested | failed | blocked
                           ↓
                  public_summary_reviewed
                           ↓
                 github_comment_posted
```

- **双重终态判定**：`relay_status=succeeded` 不能单独表示完成；必须验证原生会话的 `running=false` 和明确终态、输出稳定性。
- **未知即暂停**：若无法读取原生终态或不可区分数据稳定性，则标记 `unverified`，不得自动把本轮判成完成/通过。
- **幂等键**：本地持久化 `task_id + relay_run_id + attempt + review_revision + publication_kind`，保证重复轮询和进程重启不会重复发起 DSH 任务或 GitHub 评论。
- **断连恢复**：保留既有 run/session 标识，恢复读取旧任务；不以超时为依据自动重新 `start_run`。
- **审查与发布分离**：`accepted` 不等于 `committed`；`committed` 不等于 `published`；显式显示四种状态。
- **审查不等于全量重读**：传摘要给 Codex，Codex 可以只审 `git diff`、实际变更文件及必要测试；可对高风险情形扩大审查。
- **退出条件**：达到用户批准的测试样本数后停止；不产生下一次自动写入。

## 5. 首次验收：仅一项 mock 任务

| ID | 必须实证 |
|---|---|
| ORCH-AC-01 | 模拟 `succeeded` 但原生 `running=true`，**不**提交完成事件、不重派 |
| ORCH-AC-02 | 原生 `running=false` + 明确终态 + 输出稳定后，恰好生成一次结果 |
| ORCH-AC-03 | 断连恢复仍使用原 run，不产生第二次写入 |
| ORCH-AC-04 | 自动交付结果给受控 Codex 会话；不需要用户转述摘要 |
| ORCH-AC-05 | Codex 实际审 diff、独立运行 mock 测试，不能只接受 DSH 自述 |
| ORCH-AC-06 | GitHub Issue 出现一个经过脱敏的验收评论，标注是否已提交源码 |
| ORCH-AC-07 | 重复状态事件和调度器重启不会产生第二条同类评论 |
| ORCH-AC-08 | 超范围写入、递归读取无关目录、凭证输出均被阻止 |
| ORCH-AC-09 | 本次完成后**停止**，不自动领下一项写任务 |
| ORCH-AC-10 | 日志中有起止时间、任务 ID、关联 run、错误和恢复路径；私有标识不进入 GitHub |

## 6. 工程实施建议

1. **Codex 首先进行短只读能力探测**：当前 Relay 是否有原生终态、artifact refs、进度读取与结果订阅接口？Codex 目前可用何种 SDK/app-server 命令？不要猜测 API。
2. **Codex 选择最小实现**：可以先用本机轮询、单任务状态文件和单进程调度器；无需在本轮部署对外 webhook 服务器。
3. **Codex 定义并实现结果适配器/调度器**，把模拟事件转换为 Codex 可消费的结构化结果。
4. **DSH 参与**：Codex 提供准确的文件白名单后，委派 DSH 实现状态机、幂等测试、故障注入或数据结构模块之一；Codex 自己负责核心编排集成和验收。
5. **先模拟 Replay，后跑一项已有 mock 任务的安全复验**。不在未确认授权情况下重跑 VERSION-ID-001 的真实实现任务。
6. **审查本机变更后**，才将许可审查通过的代码和脱敏记录同步至 GitHub；避免把当前未提交的私有工作区一次性推送。
7. **完结时输出实证报告**：命令、实际模型/路由、事件序列、测试、GitHub 评论 URL、未通过项、是否建议扩至 GitHub→Codex 的下一阶段。

## 7. 后续阶段另行授权

- GitHub Issue 自动触发 Codex 接活；
- 自动选择 Codex/DSH 执行方并允许后继多任务写入；
- 共享经过验证的工程知识库（禁止自动公开私人记忆）；
- 定期后台调度、启动项和自动扩大并发；
- 将代码/视觉素材正式分发或自动合并/发布。

上述事项需要新的实机验收与安全授权。本文件不授予额外权限。

## 资料

- Codex 程序化能力：[Codex as a platform](https://developers.openai.com/blog/codex-as-a-platform)
- GitHub Webhook：本阶段无需对外开放；未来如采用，应验证 `X-Hub-Signature-256` 并防止事件重放，见 [GitHub 官方指南](https://docs.github.com/en/webhooks/using-webhooks/validating-webhook-deliveries)。
