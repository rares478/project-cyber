# Build Microsoft Detours as a static library (MinGW / MSVC).
# Fallback: build with MSVC nmake in third_party/microsoft-detours/src if this fails.

set(DETOURS_ROOT "${CMAKE_SOURCE_DIR}/third_party/microsoft-detours" CACHE PATH "Detours source root")

if(NOT EXISTS "${DETOURS_ROOT}/src/detours.cpp")
    message(FATAL_ERROR "Detours not found at ${DETOURS_ROOT}. Clone: git clone https://github.com/microsoft/Detours.git third_party/microsoft-detours")
endif()

set(DETOURS_SOURCES
    "${DETOURS_ROOT}/src/detours.cpp"
    "${DETOURS_ROOT}/src/modules.cpp"
    "${DETOURS_ROOT}/src/disasm.cpp"
    "${DETOURS_ROOT}/src/image.cpp"
    "${DETOURS_ROOT}/src/creatwth.cpp"
    "${DETOURS_ROOT}/src/disolx86.cpp"
    "${DETOURS_ROOT}/src/disolx64.cpp"
    "${DETOURS_ROOT}/src/disolia64.cpp"
    "${DETOURS_ROOT}/src/disolarm.cpp"
    "${DETOURS_ROOT}/src/disolarm64.cpp"
)

add_library(detours STATIC ${DETOURS_SOURCES})

target_include_directories(detours PUBLIC "${DETOURS_ROOT}/src")

target_compile_definitions(detours PUBLIC
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00
)

if(MSVC)
    target_compile_options(detours PRIVATE /W3 /MT)
else()
    target_compile_options(detours PRIVATE -Wall -Wno-unknown-pragmas -Wno-unused-variable)
endif()

target_link_libraries(detours PUBLIC kernel32)
