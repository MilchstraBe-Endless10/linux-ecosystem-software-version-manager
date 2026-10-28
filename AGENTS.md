# Project collaboration rules

## Authority and responsibilities
- The user is product owner and approves product scope, visual baselines, security policy, major architecture choices, and publication decisions.
- Codex leads project execution, owns Qt Quick/QML visual implementation and animation, integrates changes, reviews code, and performs final acceptance.
- DeepSeek Harness (DSH) may act only on a clearly delegated task through Harness Relay. A delegated task is limited to its stated read, write, command, and model scope. DSH may not independently change approved visual references, design tokens, or the main GUI.
- A task result is not accepted until Codex reviews it.

## Architecture and safety
- Qt Quick/QML + C++ is a conditional PoC preference, not a frozen production architecture.
- Do not connect real package managers or execute software installation/update actions in UI prototypes.
- Resolve Relay endpoint, workspace, provider, model, and tool arguments from the live Relay response; do not hard-code dynamic endpoint values or guess routing.
- Do not run privileged commands, install system packages, access credentials, or expand a delegated task's scope without explicit user authorization.
- Keep public snapshots free of private memory, session records, credentials, and unlicensed visual assets.

## Incident learning and DSH memory handoff
- At the beginning of each task, restore the local `docs/ACTIVE_WORK.md` if present, reconcile it with live GitHub Issues/PRs, and search verified incidents with `python3 tools/experience_kb.py search "<symptom>"`. This is the fallback when DSH memory tools are absent.
- Follow `docs/CODEX_DSH_DEBUG_LEARNING_LOOP.md` for evidence-first diagnosis, authoritative source research, minimal reproduction, scoped DSH delegation, Codex independent tests and prevention.
- Every repeated or non-trivial defect becomes an incident `candidate`. Only after Codex independent verification and a grounded root cause may it become `verified`. Record evidence, failure mode, fix scope and regression test, not just a task summary.
- A `handoff` JSON file is not DSH memory ingestion. Use the **actually discovered** DSH memory API only after capabilities and permissions are verified; re-query the entry to prove persistence. If unavailable, leave `pending_native_memory_ingestion`. Never write guessed Soul/Mnemon internal files.
- Public incident records must be sanitized; CI cannot replace manual secrecy/licensing review. Preserve private active work, raw transcripts, session identifiers, local host paths and visual assets outside public Git history.
- These instructions do not grant OS-level path controls, unattended continuous DSH writes or auto-publish/merge rights; SEC-001 limitations still apply.
