#include "loader.hpp"

extern "C" __declspec(dllexport) void WINAPI Lab_ShutdownInjectedModules() {
    loader::shutdown_injected_modules();
}
