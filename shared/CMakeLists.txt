add_library(lab_log STATIC lab_log.cpp)
target_include_directories(lab_log PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(lab_log PUBLIC kernel32)

add_library(lab_syscalls STATIC syscalls/syscall.cpp)
target_include_directories(lab_syscalls PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/syscalls")
target_link_libraries(lab_syscalls PUBLIC lab_log kernel32)
