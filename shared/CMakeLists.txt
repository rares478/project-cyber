add_library(lab_log STATIC lab_log.cpp)
target_include_directories(lab_log PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(lab_log PUBLIC kernel32)
