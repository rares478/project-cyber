#include <windows.h>

#include "lab_log.h"
#include "unhook.hpp"

bool install_hooks(LabLogger& log, HMODULE self);
void remove_hooks(LabLogger& log);

static LabLogger g_log;

static bool is_edr_sim_loaded() {
    return GetModuleHandleW(L"EdrSim.dll") != nullptr;
}

static void call_edr_export(const char* name) {
    HMODULE edr = GetModuleHandleW(L"EdrSim.dll");
    if (!edr) {
        return;
    }
    using Fn = void (WINAPI*)();
    const auto fn = reinterpret_cast<Fn>(GetProcAddress(edr, name));
    if (fn) {
        fn();
    }
}

extern "C" __declspec(dllexport) void WINAPI BadDll_PrepareUnload() {
    if (is_edr_sim_loaded()) {
        call_edr_export("EdrSim_StopWatchdog");
        unhook_ntdll(g_log);
    }
}

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            g_log.init("bad_dll.log", instance);
            g_log.logf("[BadDll] DLL loaded into PID %lu", GetCurrentProcessId());
            if (!install_hooks(g_log, instance)) {
                g_log.log("[BadDll] install_hooks failed during attach.");
            }
            if (is_edr_sim_loaded()) {
                g_log.log("[BadDll] EdrSim detected — install EDR hooks, then syscall unhook.");
                call_edr_export("EdrSim_InstallImmediateHooks");
                unhook_ntdll(g_log);
            }
            break;
        case DLL_PROCESS_DETACH:
            remove_hooks(g_log);
            break;
        default:
            break;
    }
    return TRUE;
}
