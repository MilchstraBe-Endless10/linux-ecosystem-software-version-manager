cmake_minimum_required(VERSION 3.16)
project(flatpak_cli_adapter LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

enable_testing()
add_subdirectory(../model core-model)

add_library(flatpak_cli_adapter STATIC flatpak_cli_adapter.cpp)
target_compile_features(flatpak_cli_adapter PUBLIC cxx_std_17)
target_include_directories(flatpak_cli_adapter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../model
)
target_link_libraries(flatpak_cli_adapter PUBLIC core_model Qt6::Core)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(flatpak_cli_adapter PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(flatpak_cli_adapter_tests tests.cpp)
target_link_libraries(flatpak_cli_adapter_tests PRIVATE flatpak_cli_adapter)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(flatpak_cli_adapter_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(flatpak_cli_adapter_multi_installation_tests multi_installation_tests.cpp)
target_link_libraries(flatpak_cli_adapter_multi_installation_tests PRIVATE flatpak_cli_adapter)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(flatpak_cli_adapter_multi_installation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME flatpak_cli_adapter_tests COMMAND flatpak_cli_adapter_tests)
add_test(NAME flatpak_cli_adapter_multi_installation_tests COMMAND flatpak_cli_adapter_multi_installation_tests)
