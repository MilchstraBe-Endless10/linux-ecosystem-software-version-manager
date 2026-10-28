cmake_minimum_required(VERSION 3.21)
project(gui_qtquick_poc LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(gui_qtquick_poc main.cpp)
target_compile_features(gui_qtquick_poc PRIVATE cxx_std_17)
target_compile_definitions(gui_qtquick_poc PRIVATE
    GUI_POC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    COSMOS_WORKSPACE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/../.."
)
target_link_libraries(gui_qtquick_poc PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)
