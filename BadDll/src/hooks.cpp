#include <windows.h>
#include <detours.h>

#include "lab_log.h"

using AppGetStatusFn = const char* (*)();

static AppGetStatusFn g_real_app_get_status = nullptr;

static const char* hooked_app_get_status() {
    return "INJECTED (BadDll.dll)";
}

bool install_hooks(LabLogger& log, HMODULE self) {
    (void)self;

    HMODULE app_module = GetModuleHandleW(L"App.exe");
    if (!app_module) {
        log.log("[BadDll] App.exe module not found yet.");
        return false;
    }
    log.log("[BadDll] Resolved App.exe module handle.");

    g_real_app_get_status = reinterpret_cast<AppGetStatusFn>(
        GetProcAddress(app_module, "App_GetStatus"));
    if (!g_real_app_get_status) {
        log.log("[BadDll] App_GetStatus export not found.");
        return false;
    }
    log.logf("[BadDll] App_GetStatus at %p", reinterpret_cast<void*>(g_real_app_get_status));

    DetourRestoreAfterWith();
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    const LONG attach_result = DetourAttach(
        reinterpret_cast<PVOID*>(&g_real_app_get_status),
        reinterpret_cast<PVOID>(hooked_app_get_status));

    if (attach_result != NO_ERROR) {
        DetourTransactionAbort();
        log.logf("[BadDll] DetourAttach failed (error %ld).", attach_result);
        return false;
    }

    const LONG commit_result = DetourTransactionCommit();
    if (commit_result != NO_ERROR) {
        log.logf("[BadDll] DetourTransactionCommit failed (error %ld).", commit_result);
        return false;
    }

    log.log("[BadDll] Detour installed on App_GetStatus (hook returns INJECTED string).");
    return true;
}

void remove_hooks(LabLogger& log) {
    if (!g_real_app_get_status) {
        return;
    }

    log.log("[BadDll] Removing detour...");
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    const LONG detach_result = DetourDetach(
        reinterpret_cast<PVOID*>(&g_real_app_get_status),
        reinterpret_cast<PVOID>(hooked_app_get_status));
    if (detach_result != NO_ERROR) {
        DetourTransactionAbort();
        log.logf("[BadDll] DetourDetach failed (error %ld).", detach_result);
        return;
    }

    const LONG commit_result = DetourTransactionCommit();
    if (commit_result != NO_ERROR) {
        log.logf("[BadDll] DetourTransactionCommit failed on detach (error %ld).", commit_result);
        return;
    }

    g_real_app_get_status = nullptr;
    log.log("[BadDll] Detour removed.");
}
