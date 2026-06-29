cmake_minimum_required(VERSION 3.24)
project(Loader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)

set(LOADER_SOURCES
    src/dllmain.cpp
    src/loader.cpp
    src/config.cpp
    src/exports_forward.cpp
    src/exports_lab.cpp
)

add_library(version SHARED ${LOADER_SOURCES})
target_include_directories(version PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_link_libraries(version PRIVATE lab_log kernel32)

add_executable(Injector src/injector_main.cpp)
target_link_libraries(Injector PRIVATE lab_log kernel32)

set_target_properties(version PROPERTIES PREFIX "")

# Copy config beside built version.dll
add_custom_command(TARGET version POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/res/Loader.config.json"
        "$<TARGET_FILE_DIR:version>/Loader.config.json"
    COMMENT "Copy Loader.config.json"
)
