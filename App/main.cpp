#include <windows.h>
#include <iostream>
#include <string>

#include "lab_log.h"

static LabLogger g_log;

// Exported hook target for BadDll (Microsoft Detours)
extern "C" __declspec(dllexport) const char* App_GetStatus() {
    return "OK (App.exe)";
}

struct AppOptions {
    bool secure_mode = true;
    bool demo_mode = false;
};

static AppOptions ParseOptions(int argc, char* argv[]) {
    AppOptions opts;
    for (int i = 1; i < argc; ++i) {
        if (_stricmp(argv[i], "--insecure") == 0) {
            opts.secure_mode = false;
        } else if (_stricmp(argv[i], "--secure") == 0) {
            opts.secure_mode = true;
        } else if (_stricmp(argv[i], "--demo") == 0) {
            opts.demo_mode = true;
        }
    }
    return opts;
}

static void LogLoadedModulePath(HMODULE module, const char* label) {
    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(module, path, MAX_PATH) == 0) {
        g_log.logf("[-] Failed to resolve path for %s (error %lu)", label, GetLastError());
        return;
    }

    char pathA[MAX_PATH]{};
    WideCharToMultiByte(CP_UTF8, 0, path, -1, pathA, MAX_PATH, nullptr, nullptr);
    g_log.logf("[+] %s loaded from: %s", label, pathA);
}

static bool EnableDllSearchOrderMitigation() {
    if (SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32)) {
        g_log.log("[+] DLL search restricted to System32 (blocks local proxy DLL hijacking).");
        return true;
    }
    g_log.log("[-] Failed to restrict DLL search order.");
    return false;
}

static bool EnableArbitraryCodeGuard() {
    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY signaturePolicy{};
    signaturePolicy.MicrosoftSignedOnly = 1;
    if (!SetProcessMitigationPolicy(ProcessSignaturePolicy, &signaturePolicy, sizeof(signaturePolicy))) {
        g_log.log("[-] Failed to set Microsoft-signed DLL policy.");
        return false;
    }
    g_log.log("[+] Microsoft-signed DLL policy enabled (unsigned payloads blocked).");

    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY dynamicCodePolicy{};
    dynamicCodePolicy.ProhibitDynamicCode = 1;
    if (!SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &dynamicCodePolicy, sizeof(dynamicCodePolicy))) {
        g_log.log("[-] Failed to enable ACG.");
        return false;
    }
    g_log.log("[+] Arbitrary Code Guard enabled (blocks detours / code patching).");
    return true;
}

int main(int argc, char* argv[]) {
    g_log.init("app.log", nullptr);

    g_log.log("--- App.exe (DLL hijack + detour lab victim) ---");
    g_log.logf("[*] PID: %lu", GetCurrentProcessId());

    const AppOptions opts = ParseOptions(argc, argv);
    g_log.logf("[*] Mode: %s", opts.secure_mode ? "SECURE" : "INSECURE");
    if (opts.demo_mode) {
        g_log.log("[*] Demo: short run (5 status lines)");
    }
    g_log.log("    Usage: App.exe [--secure | --insecure] [--demo]");

    if (opts.secure_mode) {
        EnableDllSearchOrderMitigation();
        EnableArbitraryCodeGuard();
    } else {
        g_log.log("[!] UNSECURED: local proxy version.dll and unsigned BadDll may load.");
    }

    g_log.log("[*] Calling LoadLibraryA(\"version.dll\")...");
    HMODULE hVersion = LoadLibraryA("version.dll");
    if (hVersion) {
        LogLoadedModulePath(hVersion, "version.dll");
    } else {
        g_log.logf("[-] LoadLibrary(version.dll) failed: %lu", GetLastError());
    }

    g_log.log("[*] Waiting 1.5s for loader thread / injection...");
    Sleep(1500);

    const int iterations = opts.demo_mode ? 5 : 20;
    const DWORD interval_ms = opts.demo_mode ? 2000 : 3000;
    g_log.logf("[+] Running status loop (%d lines, %lu ms interval)", iterations, interval_ms);

    for (int i = 0; i < iterations; ++i) {
        const char* status = App_GetStatus();
        g_log.logf("[status] %s", status);
        Sleep(interval_ms);
    }

    if (hVersion) {
        g_log.log("[*] FreeLibrary(version.dll)");
        FreeLibrary(hVersion);
    }

    g_log.log("[*] App.exe exiting normally.");
    return 0;
}
