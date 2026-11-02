cmake_minimum_required(VERSION 3.21)
project(cosmos_core_model LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core)
set(CMAKE_AUTOMOC ON)
include(CTest)

add_library(version_id STATIC version_id.cpp)
target_compile_features(version_id PUBLIC cxx_std_17)
target_include_directories(version_id PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(core_model STATIC core_model.cpp)
target_compile_features(core_model PUBLIC cxx_std_17)
target_include_directories(core_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core_model PUBLIC version_id)

add_library(qt_software_list_model STATIC
    qt_software_list_model.cpp
    qt_software_list_model.hpp
)
target_compile_features(qt_software_list_model PUBLIC cxx_std_17)
target_include_directories(qt_software_list_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qt_software_list_model PUBLIC core_model Qt6::Core)

if (BUILD_TESTING)
    add_executable(version_id_tests version_id_tests.cpp)
    target_link_libraries(version_id_tests PRIVATE version_id)
    add_test(NAME version_id_tests COMMAND version_id_tests)

    add_executable(core_model_tests core_model_tests.cpp)
    target_link_libraries(core_model_tests PRIVATE core_model)
    add_test(NAME core_model_tests COMMAND core_model_tests)

    add_executable(qt_model_tests qt_model_tests.cpp)
    target_link_libraries(qt_model_tests PRIVATE qt_software_list_model)
    add_test(NAME qt_model_tests COMMAND qt_model_tests)
endif()
