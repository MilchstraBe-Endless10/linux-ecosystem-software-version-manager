# AGENTS.md — Linux 全生态软件版本管理中心（参考模板）

> 将本模板放入用户实际注册的工作区仓库根目录前，应由 Codex 审查，**本文件不是已经写入用户机器的仓库配置**。

## Roles

- Codex is the sole integration lead, GUI visual/interaction owner, architecture reviewer and final acceptance authority for delegated changes.
- DSH/DeepSeek Hermes only implements explicitly scoped tasks delegated by Codex through Harness Relay. Never choose a different approved design, rewrite primary QML UI or override user decisions.
- Follow `Linux全生态软件版本管理中心_PRD_V3.2_Codex主导与DSH协作架构.md` and the approved V3.0 light anime illustration reference.

## GUI & code

- Preferred, not yet benchmark-frozen: Qt 6 Qt Quick/QML single primary GUI with C++ models. No Flutter runtime inside Qt.
- Codex owns the visual scene, animation, UI state, user interactions and final GUI screenshot comparison.
- DSH default scopes are mock backend adapters, tests, parsers, documentation and read-only reviews.
- Existing artwork must be reused; do not silently replace or regenerate characters/backgrounds.
- Design mock numbers are never presented as verified installed versions.

## Safety and delegation

- Call Relay doctor and list_workspaces read-only in a fresh Codex session before delegation. Use list_capabilities for real provider/model values.
- start_review before writing. start_run only with an explicit workspace-write task and file whitelist. Treat run terminal state separately from acceptance.
- Never invoke sudo or real system upgrades from test tasks. No pacman partial upgrade, unreviewed AUR script or unauthorized shell installer.
- Do not paste secrets, environment variables, SSH keys or credentials into relay task prompts.
- Avoid concurrent writes to the same files and always inspect git diff and rerun tests.
- Review source authenticity, package ownership, approvals and recovery logs before any real update execution is designed.
- Keep visual, build and simulated task tests separate from production update actions.

## Acceptance

- Every implementation task must include a precise scope, permitted files, baseline, tests, deliverables and rollback route.
- Codex independently reviews all DSH edits and decides acceptance. Report limitations and unverified behavior explicitly.
