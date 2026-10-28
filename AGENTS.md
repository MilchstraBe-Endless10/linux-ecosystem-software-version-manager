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
