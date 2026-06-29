cmake_minimum_required(VERSION 3.24)
project(CyberDllLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR "This project targets Windows only.")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

include(cmake/BuildDetours.cmake)

add_subdirectory(shared)

add_subdirectory(EdrSim)
add_subdirectory(BadDll)
add_subdirectory(Loader)
add_subdirectory(App)

# Deploy bundle next to build tree
set(DEPLOY_DIR "${CMAKE_SOURCE_DIR}/deploy")
get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

add_custom_target(deploy ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:App>" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:version>" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:BadDll>" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:EdrSim>" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Injector>" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/Loader/res/Loader.config.json" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MINGW_BIN_DIR}/libstdc++-6.dll" "${DEPLOY_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MINGW_BIN_DIR}/libwinpthread-1.dll" "${DEPLOY_DIR}/"
    DEPENDS App version BadDll EdrSim Injector
    COMMENT "Copying lab binaries to deploy/"
)
