cmake_minimum_required(VERSION 3.24)
project(App LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)

add_executable(App main.cpp)
target_include_directories(App PRIVATE "${CMAKE_SOURCE_DIR}/shared")
target_link_libraries(App PRIVATE lab_log kernel32)

if(WIN32)
    set_target_properties(App PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()
