cmake_minimum_required(VERSION 3.21)
project(cosmos_flatpak_cli_ci LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)
enable_testing()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../src/core/flatpak
                 ${CMAKE_CURRENT_BINARY_DIR}/flatpak-core)

add_executable(flatpak_cli_real_integration_tests
    ../flatpak_cli_real_integration.cpp
)
target_compile_features(flatpak_cli_real_integration_tests PRIVATE cxx_std_17)
target_link_libraries(flatpak_cli_real_integration_tests PRIVATE
    flatpak_cli_adapter
    qt_software_list_model
    Qt6::Core
)
