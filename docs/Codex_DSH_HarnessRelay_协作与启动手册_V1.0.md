# Codex → DeepSeek Hermes / DSH Harness Relay 协作与启动手册 V1.0

> 适用项目：Linux 全生态软件版本管理中心（Omarchy / Arch / Hyprland / Wayland）  
> V3.3 对齐修订：2026-10-28（文件名保留原 V1.0 标识）。  
> 证据等级：本会话已通过 Relay `doctor`、工作区/能力查询和首轮只读 `start_review`；这些结果是该时点的运行快照。  
> 项目产品 PRD：`Linux全生态软件版本管理中心_PRD_V3.3_Relay已联通_首轮审查与开发基线准备.md`。  
> 视觉基准：`image(20261027-085314).png`。历史交付 ZIP 名称为 `linux_update_center_visual_assets_v3_full.zip`；当前工作区中的解压目录为 `Linux全生态版本管理中心_定稿视觉素材库_V3.0/`，不表示 ZIP 当前也位于该工作区。

## 1. 正式职责与工具链

- **你（用户）**：审定视觉、产品需求、重大风险、正式发布。
- **Codex（总控）**：主 UI 的 QML 视觉和动画、页面状态与布局、开发计划、分解任务、代码审查、自动化测试、集成与最终验收。
- **DeepSeek Hermes（通过 DSH）**：经 Codex 明确规定范围后完成非争用的业务模块、数据解析、K3 适配器、只读 Review、测试和文档；不得自行决定技术架构或更换视觉素材。
- **Harness Relay MCP**：Codex 调度 DSH 的通信与工具接口，不是 Qt 程序运行时依赖。

注意：DeepSeek Hermes 与 DSH provider/model 命名以 `list_capabilities` 实际返回为准。不得假定 `Hermes` 一定等于特定模型 ID，也不得凭空猜参数。

每次委派都要从实际能力结果中明确 provider、model 与 reasoning；不得自动回退、切换路由或臆造不可用模型。若目标路由不在能力列表中，停止并报告。

## 2. Relay 连接核验（V3.3 首轮已通过）

Relay endpoint 由 Codex proxy 从当前 DSH Host 动态发现。端口会变化；不得把历史值或当前观测值写成固定 endpoint、重复注册 MCP 或启动第二个 Host。V3.3 首轮已通过 `doctor`、`list_workspaces` 和 `list_capabilities`；Relay 已连通时不重复重启 Codex/DSH。

只有连接异常时才重新运行 `doctor`，核对实际返回的 descriptor/status 路径、Host 状态和 endpoint；若 Host 正常而 proxy 查找目录不一致，先修复发现路径，不要反复重启或复制 MCP 注册。

**新环境首次核验：**

1. 仅在新环境首次核验或连接异常时，在当前 Codex 会话调用 `doctor`、`list_workspaces` 和 `list_capabilities`；本项目 V3.3 首轮已通过，正常连接时不重复握手。
2. 只采用工具返回的 endpoint、已注册工作区绝对路径、provider/model 和参数 schema。
3. 本阶段只读；不得调用 `start_run`、修改文件、注册工作区、安装包、运行 sudo 或更改系统配置。

```text
你是本项目 Codex 开发总负责人。只做 Harness Relay MCP 只读预检。
1) Call doctor, list_workspaces and list_capabilities; report actual responses only.
2) Use only the returned endpoint, registered workspace paths and provider/model values.
3) Do not call start_run, write files, register workspaces, duplicate MCP entries, start another Host, install packages, run sudo or update the system.
4) Do not read outside the task's exact contextReadScope. Do not execute shell commands unless the task names the exact command and paths.
5) A SHA256 manifest does not authorize reading every referenced asset. Do not run full-manifest sha256sum -c unless the user explicitly authorized every asset path and that command.
6) If a tool or rtk is unavailable, stop and report; do not substitute another execution path or broaden permissions.
```

### 预期检查项（非预设结果）

- `doctor`: 返回可解释的 Relay/Host 诊断；失败不视为已经连通。
- `list_workspaces`: 报告 DSH 已注册工作区的**真实绝对路径**；未登记时先在 DSH 侧由用户注册。
- `list_capabilities`: 返回实际工具能力、provider/model；选择后才尝试 `start_review`。
- 连接异常时核对 proxy 实际 descriptor 路径与 Host 的 `DSH_HOME`；不得因为端口变化就硬编码地址、重复注册或启动第二个 Host。

## 3. 首轮只读 Review（已完成；后续任务按范围授权）

首轮 `start_review` 已完成，不再列为待办。任何后续 review 都要先确定精确的 `reviewTargets` 和 `contextReadScope`；只读权限不代表可读取整个工作区，也不代表可以执行任意校验命令。

```text
作为 Codex 总负责人，使用已注册的 DSH workspace 与实际返回的 provider/model 发起本项只读审查。
reviewTargets: <逐项列出要审查的文件>
contextReadScope: <仅列验证这些目标确需读取的文件/目录>
excludedPaths: <列明用户数据、素材二进制、缓存、凭证及无关目录>
writeScope: []
只读取声明的范围；不得粘贴源码，不得自行扩展目录，不得执行未列出的 shell 命令。
仅在读取范围明确包含全部对应素材且用户授权时，才可运行校验这些素材的命令。不得因为读取 SHA256 清单而运行全量 sha256sum -c。
通过 wait_run 到终态，报告准确路径/行号、实际读取范围、执行命令、证据、风险和未完成事项。
```

**首轮范围偏差记录（DSH 返回内容）**：首次只读审查限定为 `docs/`、素材 README 与 `SHA256SUMS.txt`，但 DSH 报告执行了完整 247 文件 SHA256 校验。这超出了声明的读取范围；该校验是 DSH 自述，不视为 Codex 独立验证。后续任务必须明确禁止未授权的全量清单校验；需要扩展读取范围时先由 Codex 取得用户授权。

## 4. 第三步——授权后首个小型写入任务

只有用户/项目政策允许写入，且 Codex 已确认以下信息，才能用 `start_run`：
- 从 `list_workspaces` 得到的注册 workspace；
- 从 `list_capabilities` 得到的真实 provider/model；
- 具体模块与允许写入文件；
- 禁止修改文件（视觉母稿/素材包、系统配置、凭证、其他模块）；
- 基线 revision、验收测试、回退方法。

首个建议任务：**模拟数据版本解析模块及单测**，完全不调用系统包管理器；或独立读取测试 fixtures 的只读适配器。它应易于 Codex 验收，不与 QML 主体改动冲突。

```text
作为 Codex，基于已经完成的只读 Review，准备一个可独立验收的 DSH 实现任务。
先展示：真实注册 workspace、当前 revision、被允许修改的文件列表、禁止修改列表、输入输出契约和可执行测试命令。
只在获得此任务写入授权后调用 Harness Relay start_run，使用 permissionPreset: "workspace-write" 和 list_capabilities 支持的 provider/model。task 应当只包含工作区内已有任务说明文件位置与实现范围。
使用 wait_run 跟踪到终态。必要时通过 reply_run/steer_run 给出明确修正；出现失控或越权情况调用 cancel_run 并检查部分修改。
然后由 Codex 独立检查完整 git diff、运行测试、确认没有真实执行系统包管理器操作。通过后才接受该任务；否则回退或继续修正。
```

⚠️ `permissionPreset: "workspace-write"` **不是 root 权限，也不意味着可以真实安装系统包**。绝不可用联调任务在你的 Omarchy 机器上运行 `pacman -Syu`、官方 Omarchy upgrade 或 AUR 构建。

## 5. 推荐模块切分

| Codex 直接负责 | DSH 可优先承担 |
|---|---|
| V3.0 动漫母稿还原与层次编排 | 素材路径/尺寸/许可清单只读检查 |
| QML 页面、状态动画、Shader、减弱动效 | 独立纯数据模型、转换与测试 |
| WB3 自由网格、COL3 表格、DET3/CMP3/TAG3 | 批处理排序/筛选纯逻辑测试、模拟 fixtures |
| 系统权限架构、安全审查、最终合并 | K3 各软件来源的发现/解析/更新计划候选实现 |
| 更新计划与用户授权 UI | 适配器 mock、失败恢复测试、日志/审计格式 |

允许 Codex 调整拆分，但应优先减少同文件冲突。DSH 不得以“帮忙提高美感”为由重做动漫人物或更换定稿视觉。

## 6. 协作任务单模板

```markdown
# DSH Task: TASK-001
Workspace: <从 list_workspaces 获取的绝对路径>
Baseline revision: <实际 commit hash 或明确未初始化>
Goal: <单一可验证的目标>
reviewTargets: <逐项列明审查目标>
contextReadScope: <逐项列明允许读取的上下文>
excludedPaths: <凭证、缓存、私有数据、未授权素材等>
Allowed commands: <只列本任务确需运行的准确命令和路径；默认无命令>
writeScope: <严格列出允许写入的文件/目录；只读时为空>
Forbidden writes: <正式视觉资产、凭证、系统配置、未授权模块>
Inputs/outputs: <函数、模型或数据契约>
Safety: Never sudo, install packages or run real updates. Never run full-manifest checksum unless every input is authorized.
Tests: <具体可运行命令，或说明当前尚无测试运行器>
Permission preset: <与本次授权一致；只读审查固定 read-only>
Acceptance criteria: <编号和明确结果>
Required handoff: Changed files, diff summary, tests, unknowns, risks
```

如果 Repo 还没有初始化，就先由 Codex 根据用户批准的阶段目标制定初始化方案；不要在只读握手过程中擅自创建 GitHub 仓库。

## 7. 审查与验收的六个关卡

1. **范围**：只修改被批准模块；代码和日志不含凭证。
2. **正确性**：Codex 自行复现构建与单测；DSL/适配器解析未知状态不得伪成功。
3. **业务安全**：Omarchy 完整系统升级、AUR 和 CLI 所有权约束未被绕开。
4. **用户体验**：Codex 亲自检查 QML 与视觉基准、键盘、缩放、异常状态、reduced motion。
5. **可追溯**：保存任务 ID、实际 model、permission、日志、基线、diff、结论、未完成事项。
6. **范围审计**：逐项核对实际读取文件和执行命令是否在 task 声明内；只读任务越界也要记录并报告。

`wait_run` 的终态不是“验收通过”，GitHub 提交也不自动代表可以发布。

## 8. V3.3 阶段状态与后续顺序

- Relay 联调和首轮只读 review：已完成；端点动态发现，不因正常状态重复重启或复审。
- 本地基线：按用户批准创建根 AGENTS、.gitignore 和本地 Git 初始提交；不配置远端、不 push。
- GUI 原型：Qt Quick 可运行静态视觉样机和资源缺口表由 Codex 主导；须在单独授权后开始。
- 第一个 DSH 写入任务：可考虑隔离 mock 版本/来源解析模块与测试；须单独授权，Codex 审查后才可接受。
- 系统级测试保持只读或模拟，直到权限和事务安全设计经过独立审核。

本节描述候选阶段顺序，不构成对 DSH 写入任务或 Qt Quick PoC 的当前授权；两项工作分别取得授权后再启动。

**尚未完成**：Qt Quick PoC、目标环境视觉/性能验证、许可证核查及后续单模块 DSH 写入任务；它们分别需要自己的范围与授权。

## 9. 官方资料与范围限定

- Codex 支持通过 CLI 或 IDE 注册 MCP server，配置可共享：https://developers.openai.com/learn/docs-mcp
- MCP 工具存在权限与数据泄露风险，应审核实际 MCP server 和访问范围：https://developers.openai.com/api/docs/guides/tools-connectors-mcp
- 用户提供的 `doctor`、`list_workspaces`、`start_review`、`start_run`、`wait_run`、`reply_run`、`steer_run`、`cancel_run` 来自用户已经安装的 DSH Relay 提示，**不是上述 OpenAI 通用文档对该第三方 Relay 的公开保证**。最终以本机真实工具列表与参数 schema 为准。
