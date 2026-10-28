# GUI-FX-001 · Qt Quick V3.0 visual fidelity implementation and dependency decisions

> **Status:** partial **source implementation** only. This public branch is derived from the older public Qt PoC, **not** the user's actively edited private GUI-POC-005. Codex must merge concepts against the local updated QML; do **not** overwrite the active private file with this older public copy.
>
> Private approved screenshot, generated character/background images and image-derived screenshots are **not** redistributed here.

## Diagnosis against the approved 1672×941 reference

The present GUI mismatch is **not simply a resolution problem**. It is a compositing and functionality gap:

| Region | Approved V3.0 reference | Current PoC condition | Priority |
| --- | --- | --- | --- |
| Hero | Large seated illustration integrated with central cube and a cat | Smaller character, alpha matte/crop question; missing cat | P0 |
| Background | Separate near/middle/far floating-city elements | Mostly flattened sky/scene; little depth | P0 |
| Center | Luminous violet/blue cube, orbital rails, manager nodes, bloom | Flat ring/node arrangement, cube omitted or reduced | P0 |
| Details | Full instance cards, version diff, paths, buttons | Simplified fixed demo rows and fewer controls | P0 |
| Table | Multi-select, sort/filter, last-check, operation buttons | Mostly static mock cells | P0 |
| Motion | Light trails, glow, petals, parallax, reduced-motion | Simple timer animations, limited actual effects | P1 |
| Sizing | Actual native wide/narrow Wayland layouts | Offscreen responsive tests, wide native window unproven | P0 |

Pixel-perfect reproduction is not guaranteed when character and background artwork differs from the approved reference; Codex must deliver side-by-side real native screenshots, state meaningful mismatches and obtain owner acceptance.

## Verified technical choice (no framework rewrite)

- **Qt 6.8+ QtQuick.Effects / MultiEffect**: composited bloom and shadow; avoid deprecated Qt 5 GraphicalEffects.
- **QtQuick.Particles**: scene-graph GPU particles; built-in glowdot sprite for the *non-final* proof. Licensed isolated floral sprite required for final petal art.
- **QtQuick / Qt Shader Tools (qsb)**: reserved for a high-fidelity 3D-looking luminous cube and rippling energy trails; compile GLSL to `.qsb`, do not paste Qt 5 GLSL strings into ShaderEffect.
- **Krita / rembg + Pillow**: alpha matting and edge decontamination; inspect *actual alpha values*, not only RGBA PNG format. The alpha audit is provided as `asset_check.py`. rembg model weights have separate licenses, evaluate before redistributing.
- **Qt SVG** for vector icon layers (not for raster anime art). Avoid adding GPL-only Qt modules (Qt Lottie/Qt Quick 3D) without a project license decision.

Arch package names, to be **checked on the host** before choosing any install action: `qt6-declarative`, `qt6-shadertools`, `qt6-svg`, `qt6-tools`, `krita`. Prefer no privileged installation just to test already-installed modules; if a package is absent, Codex should use normal Arch full-upgrade-safe dependency process after host owner authorizes system changes. Do not silently run `sudo pacman` or partial-upgrade a running Arch system.

Official sources:
- Qt effects: https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html
- Qt particles: https://doc.qt.io/qt-6/qml-qtquick-particles-imageparticle.html
- Shader tools: https://doc.qt.io/qt-6/qtshadertools-qsb.html
- rembg: https://github.com/danielgatis/rembg
- Qt licenses: https://doc.qt.io/qt-6/licensing.html

## Source implementation in this branch

- `MagicCore.qml`: centered glow object, orbit rings, pulse animation and `MultiEffect`, gated by the existing `root.sceneRunning` so reduced-motion/minimize/idle do not run timers.
- `LightMotes.qml`: native Qt particles with the *bundled Qt* glowdot placeholder. Replace once separated, licensed petal artwork exists.
- `Main.qml`: integrates the two visual components in the **older public mock QML only**. The real private PoC has advanced since this snapshot; cherry-pick/reimplement locally, not wholesale overwrite.
- `asset_check.py`: uses Pillow to report source pixel size, alpha presence, transparency fractions and opaque corner warnings. **Never modifies/exports private images**.

## Developer validation sequence

1. Codex restore `docs/ACTIVE_WORK.md` and private GUI-POC-005. Diff the public PR against its own QML and migrate the **two components only**, resolving locally changed layout rules. Do not replace current QML with the stale public snapshot.
2. Read-only check of existing Qt modules and their runtime imports; install only genuinely missing packages after reviewing system package changes. `import QtQuick.Effects` and `import QtQuick.Particles` must resolve, otherwise report a dependency gap; fail rather than silently omitting effects.
3. Run `python3 experiments/gui-qtquick/asset_check.py <character_png> --kind character --target 1437x1095` locally against private generated character, and `--kind background --target 1672x941` against background. **Do not publish their paths or images**. Remediate any opaque rectangle by *source alpha correction* before QML compositing.
4. Recompose layered private scene: high-resolution background, proper alpha cutout, separately owned foreground petals/glow overlays, central cube, cards and interactive data table. Do not upscale small crop files and call them 4K.
5. Add real tab/compare-mode/table actions. No real package-manager updates; all controls are mock until backend safety acceptance.
6. Capture **actual native Wayland window** at 1672×941 and 1280×720 (plus 1024×600 regression) and compare screenshots with approved private V3.0 source. Offscreen checks are supplemental, not substitutes.
7. Validate GPU frame-time P95/P99, idle/minimized CPU, pause-on-reduced-motion, 10k list virtualization, WB3 actual drag, overlapping/z-index and responsive details. Check screenshot visual fidelity, not only 60 FPS.
8. Codex publishes a factual acceptance table and can close P0 GUI scope only after no obvious matte rectangle, readable text, intact original composition and demonstrable interactions. Core backend remains an independent parallel workstream.

## Review status

No Qt runtime is installed in the assistant's execution environment, therefore **these QML changes have not passed a real Qt build/runtime screenshot**. The PR is an implementation starting point, not a completed 1:1 restoration. This fact must not be hidden.

SEC-001 separately blocks *unattended continuous DSH file writes*; it does not block an authorized Codex GUI implementation nor supervised DSH mock-model work.
