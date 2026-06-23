cmake_minimum_required(VERSION 3.24)
project(BadDll LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)

add_library(BadDll SHARED
    src/dllmain.cpp
    src/hooks.cpp
    src/unhook.cpp
)

target_include_directories(BadDll PRIVATE include)

target_link_libraries(BadDll PRIVATE detours lab_log lab_syscalls kernel32)

set_target_properties(BadDll PROPERTIES
    OUTPUT_NAME "BadDll"
    PREFIX ""
)
