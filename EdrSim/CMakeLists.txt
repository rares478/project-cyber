cmake_minimum_required(VERSION 3.24)
project(EdrSim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)

add_library(EdrSim SHARED
    src/dllmain.cpp
    src/edr_api.cpp
    src/simulate.cpp
    src/watchdog.cpp
)

target_include_directories(EdrSim PRIVATE include)

target_link_libraries(EdrSim PRIVATE lab_log kernel32)

set_target_properties(EdrSim PROPERTIES
    OUTPUT_NAME "EdrSim"
    PREFIX ""
)
