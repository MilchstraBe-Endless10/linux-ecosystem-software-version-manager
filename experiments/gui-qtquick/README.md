# Qt Quick GUI prototype

This is a prototype built with Qt Quick/QML and C++. It uses only synthetic software records. It does not connect to package managers or execute real changes.

The V3.0 background and character asset library is not included. The QML image paths refer to the separate local visual package and those image layers will not appear in this public copy. Screenshots were left out because they contain the excluded concept assets.

## Build

Requirements: Qt 6.8 Core, Gui, Qml, Quick, QuickControls2; CMake 3.21 or newer; C++17 compiler.

    cmake -S experiments/gui-qtquick -B experiments/gui-qtquick/cmake-build -DCMAKE_BUILD_TYPE=Release
    cmake --build experiments/gui-qtquick/cmake-build --parallel
    experiments/gui-qtquick/cmake-build/gui_qtquick_poc --self-test

## Measurement notes

Native Wayland requested 1672×941 and 1280×720, but both mapped windows ended at 941×1030. Numeric data is recorded in the metrics JSON files. Offscreen size captures validated the two responsive layouts; those runs are not native rendering or frame-rate evidence.

WB3 is implemented with a drag handler and grid snapping, but a real pointer drag has not been verified. DET3/CMP3 comparison state and reduced-motion state were checked with offscreen captures. The screenshots are intentionally not present in this public snapshot.
