# Linux Ecosystem Software Version Manager

This public repository is a reviewed project snapshot. It contains planning documents, a Qt Quick/QML + C++ GUI prototype, and selected numeric measurements. The prototype uses synthetic records only; it does not connect to package managers or perform real updates.

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
