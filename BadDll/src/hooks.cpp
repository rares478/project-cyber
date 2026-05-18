#include <windows.h>
#include <detours.h>

#include <string>

#include "lab_log.h"

using AppGetStatusFn = const char* (*)();

static LabLogger g_log;
static AppGetStatusFn g_real_app_get_status = nullptr;

static const char* hooked_app_get_status() {
    return "INJECTED (BadDll.dll)";
}

bool install_hooks(HMODULE self) {
    g_log.init("bad_dll.log", self);
    g_log.logf("[BadDll] DLL loaded into PID %lu", GetCurrentProcessId());

    HMODULE app_module = GetModuleHandleW(L"App.exe");
    if (!app_module) {
        g_log.log("[BadDll] App.exe module not found yet.");
        return false;
    }
    g_log.log("[BadDll] Resolved App.exe module handle.");

    g_real_app_get_status = reinterpret_cast<AppGetStatusFn>(
        GetProcAddress(app_module, "App_GetStatus"));
    if (!g_real_app_get_status) {
        g_log.log("[BadDll] App_GetStatus export not found.");
        return false;
    }
    g_log.logf("[BadDll] App_GetStatus at %p", reinterpret_cast<void*>(g_real_app_get_status));

    DetourRestoreAfterWith();
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    const LONG attach_result = DetourAttach(
        reinterpret_cast<PVOID*>(&g_real_app_get_status),
        reinterpret_cast<PVOID>(hooked_app_get_status));

    if (attach_result != NO_ERROR) {
        DetourTransactionAbort();
        g_log.logf("[BadDll] DetourAttach failed (error %ld).", attach_result);
        return false;
    }

    const LONG commit_result = DetourTransactionCommit();
    if (commit_result != NO_ERROR) {
        g_log.logf("[BadDll] DetourTransactionCommit failed (error %ld).", commit_result);
        return false;
    }

    g_log.log("[BadDll] Detour installed on App_GetStatus (hook returns INJECTED string).");
    return true;
}

void remove_hooks() {
    if (!g_real_app_get_status) {
        return;
    }

    g_log.log("[BadDll] Removing detour...");
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(
        reinterpret_cast<PVOID*>(&g_real_app_get_status),
        reinterpret_cast<PVOID>(hooked_app_get_status));
    DetourTransactionCommit();
    g_log.log("[BadDll] Detour removed.");
}
