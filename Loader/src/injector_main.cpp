#include <windows.h>
#include <cstdio>
#include <iostream>
#include <string>

#include "lab_log.h"

static LabLogger g_log;

static std::wstring to_wide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}

int main(int argc, char* argv[]) {
    g_log.init("injector.log", nullptr);
    g_log.logf("[Injector] Started (PID %lu)", GetCurrentProcessId());

    if (argc != 3) {
        g_log.log("[-] Usage: Injector.exe <pid> <path_to_dll>");
        return 1;
    }

    DWORD pid = 0;
    try {
        pid = static_cast<DWORD>(std::stoul(argv[1]));
    } catch (...) {
        g_log.logf("[-] Invalid PID: %s", argv[1]);
        return 1;
    }

    if (pid == GetCurrentProcessId()) {
        g_log.log("[-] Refusing to inject into self.");
        return 1;
    }

    g_log.logf("[Injector] Target PID: %lu", pid);
    g_log.logf("[Injector] DLL path: %s", argv[2]);

    const std::wstring dll_path = to_wide(argv[2]);
    const size_t dll_path_bytes = (dll_path.size() + 1) * sizeof(wchar_t);

    g_log.log("[Injector] OpenProcess...");
    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        pid);
    if (!process) {
        g_log.logf("[-] OpenProcess failed: %lu", GetLastError());
        return 1;
    }

    g_log.log("[Injector] VirtualAllocEx for DLL path...");
    void* remote_buf = VirtualAllocEx(process, nullptr, dll_path_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_buf) {
        g_log.logf("[-] VirtualAllocEx failed: %lu", GetLastError());
        CloseHandle(process);
        return 1;
    }
    g_log.logf("[Injector] Remote buffer at %p", remote_buf);

    if (!WriteProcessMemory(process, remote_buf, dll_path.c_str(), dll_path_bytes, nullptr)) {
        g_log.logf("[-] WriteProcessMemory failed: %lu", GetLastError());
        VirtualFreeEx(process, remote_buf, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }
    g_log.log("[Injector] Wrote DLL path into target process.");

    const auto load_library_w = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
    if (!load_library_w) {
        g_log.log("[-] GetProcAddress(LoadLibraryW) failed.");
        VirtualFreeEx(process, remote_buf, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }

    g_log.log("[Injector] CreateRemoteThread(LoadLibraryW)...");
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, load_library_w, remote_buf, 0, nullptr);
    if (!thread) {
        g_log.logf("[-] CreateRemoteThread failed: %lu", GetLastError());
        VirtualFreeEx(process, remote_buf, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }

    WaitForSingleObject(thread, INFINITE);
    DWORD remote_module = 0;
    GetExitCodeThread(thread, &remote_module);

    CloseHandle(thread);
    VirtualFreeEx(process, remote_buf, 0, MEM_RELEASE);
    CloseHandle(process);

    if (remote_module == 0) {
        g_log.log("[-] LoadLibraryW in target returned NULL.");
        return 1;
    }

    g_log.logf("[Injector] Injection OK. Remote module handle: 0x%lX", static_cast<unsigned long>(remote_module));
    return 0;
}
