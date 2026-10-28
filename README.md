# Linux Ecosystem Software Version Manager

This public repository is a reviewed project snapshot. It contains planning documents, a Qt Quick/QML + C++ GUI prototype, and selected numeric measurements. The prototype uses synthetic records only; it does not connect to package managers or perform real updates.

## 最新项目进度与协作任务

- [V3.6 公开状态、Qt Quick 技术验证和 Codex → DSH 协作任务](docs/PROJECT_STATUS_V3.6_PUBLIC.md)
- 完整 PRD 的内部版本与公开审查版本采取分离管理：敏感路径、Relay 会话、私有记忆、未完成许可审查的动漫素材不得直接公开。
- Codex 主导 UI/动画与代码验收；DeepSeek Harness 通过受控 Relay 写入任务参与纯函数、适配器和测试开发。

## Current status

Qt Quick/QML + C++ remains a conditional PoC candidate. The desktop tests requested 1672×941 and 1280×720, but both native Wayland windows were resized after mapping to 941×1030. The captured geometry events show the requested size before display and a different compositor-provided size after display. Target-size layout was checked offscreen, but has not yet been validated in native Wayland at both requested sizes.

On this machine, the two 12-second Wayland runs reported about 59 FPS, P95/P99 frame intervals of 18/19 ms, maximum intervals of 175 ms, 10,000 synthetic records, and peak RSS around 206–207 MiB. These values describe one device and session, not a performance guarantee. WB3 pointer dragging and minimized-window CPU sampling remain unverified.

## Visual asset boundary

The V3.0 design asset library and screenshots containing those images are intentionally absent from this public snapshot. The asset notes mention third-party marks and AI-generated concept art, without itemized redistribution permissions. The QML source refers to the local asset directory for its background and character layers; without that separately reviewed material, those layers are absent. Do not infer a redistribution license from this repository.

No project license has been selected. Public visibility does not grant permission to reuse the code or documents. Qt deployment licensing must also be reviewed before distributing a packaged application.

## Build the GUI prototype

Requirements: Qt 6.8 with Core, Gui, Qml, Quick, and QuickControls2; CMake 3.21 or newer; and a C++17 compiler.

From the repository root:

    cmake -S experiments/gui-qtquick -B experiments/gui-qtquick/cmake-build -DCMAKE_BUILD_TYPE=Release
    cmake --build experiments/gui-qtquick/cmake-build --parallel
    experiments/gui-qtquick/cmake-build/gui_qtquick_poc --self-test

The GUI is a visual mockup. Its controls do not execute installation or update actions.

## Collaboration model

Codex owns visual direction, GUI integration, review, and final acceptance. DSH works only on explicitly delegated tasks through Harness Relay. See AGENTS.md and the documents under docs/ for the current collaboration and architecture notes.

## Codex × DeepSeek Harness: approved minimum orchestration PoC

- [ORCH-001 MVP implementation spec](docs/ORCH-001_MVP_IMPLEMENTATION_SPEC.md)
- [Implementation work item #3](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/3) · [overall orchestration plan #2](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/2)
- Scope: **one bounded mock task**, DSH native completion → structured Codex handoff → independent Codex validation → sanitized GitHub audit comment. Codex owns final acceptance.
- This is **approved for implementation, not yet deployed or verified**. Autonomous backlog consumption, automatic merging, privileged system operations and publishing private assets remain out of scope.

## P0 Active work: GUI fidelity + backend in parallel

- **GUI-FINAL-001 [#8](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/8)** — match the approved private V3.0 visual reference (compositing, hero alpha, central orb, full detail/table interactions) with native Wayland screenshot evidence. The new generated art remains private until license review.
- **BACKEND-CORE-001 [#9](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/9)** — build the installation-instance model and Qt DTO contract in parallel; Codex reviews the existing [Flatpak adapter PR #6](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/pull/6) and can supervise bounded DSH pure-function tasks.
- **SEC-001 [#4](https://github.com/MilchstraBe-Endless10/linux-ecosystem-software-version-manager/issues/4)** remains open: one synthetic bubblewrap probe execution path was observed, but not every tool path and not OS-level denial behavior. It blocks **unattended continuous DSH writes**, not ordinary supervised development.
- **Task recovery**: Codex should restore all open threads from the local `docs/ACTIVE_WORK.md` before execution. Never mark an uncommitted private module as published or upload private project history/assets by accident.
