#include "syscall.h"

#include <cstring>

#include "lab_log.h"

namespace {

using SyscallFn = NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);

PVOID resolve_export_from_clean(LPVOID clean_ntdll, HMODULE live_ntdll, const char* export_name) {
    const auto live_proc = reinterpret_cast<PVOID>(GetProcAddress(live_ntdll, export_name));
    if (!live_proc || !clean_ntdll) {
        return nullptr;
    }
    const DWORD_PTR offset =
        reinterpret_cast<DWORD_PTR>(live_proc) - reinterpret_cast<DWORD_PTR>(live_ntdll);
    return reinterpret_cast<PVOID>(reinterpret_cast<DWORD_PTR>(clean_ntdll) + offset);
}

DWORD extract_ssn(PVOID export_fn) {
    if (!export_fn) {
        return 0;
    }
    const auto stub = reinterpret_cast<const BYTE*>(export_fn);
    if (stub[0] == 0x4C && stub[1] == 0x8B && stub[2] == 0xD1 && stub[3] == 0xB8) {
        return static_cast<DWORD>(stub[4] | (stub[5] << 8));
    }
    return 0;
}

SyscallFn build_syscall_stub(DWORD ssn) {
    alignas(16) static BYTE stub_page[4096]{};
    static SIZE_T stub_offset = 0;

    if (stub_offset + 32 >= sizeof(stub_page)) {
        return nullptr;
    }

    auto* stub = stub_page + stub_offset;
    stub_offset += 32;

    stub[0] = 0x4C;
    stub[1] = 0x8B;
    stub[2] = 0xD1;
    stub[3] = 0xB8;
    stub[4] = static_cast<BYTE>(ssn & 0xFF);
    stub[5] = static_cast<BYTE>((ssn >> 8) & 0xFF);
    stub[6] = static_cast<BYTE>((ssn >> 16) & 0xFF);
    stub[7] = static_cast<BYTE>((ssn >> 24) & 0xFF);
    stub[8] = 0x0F;
    stub[9] = 0x05;
    stub[10] = 0xC3;

    DWORD old_protect = 0;
    if (!VirtualProtect(stub, 32, PAGE_EXECUTE_READ, &old_protect)) {
        return nullptr;
    }

    return reinterpret_cast<SyscallFn>(stub);
}

}  // namespace

static SyscallFn g_nt_protect_virtual_memory = nullptr;
static DWORD g_nt_protect_ssn = 0;

bool syscalls_init_from_disk(LabLogger& log) {
    char ntdll_path[MAX_PATH]{};
    const UINT len = GetSystemDirectoryA(ntdll_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        log.log("[BadDll] syscalls_init_from_disk: GetSystemDirectoryA failed.");
        return false;
    }
    if (ntdll_path[len - 1] != '\\') {
        std::strcat(ntdll_path, "\\");
    }
    std::strcat(ntdll_path, "ntdll.dll");

    HANDLE h_file = CreateFileA(
        ntdll_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h_file == INVALID_HANDLE_VALUE) {
        log.logf("[BadDll] syscalls_init_from_disk: CreateFileA failed (%lu).", GetLastError());
        return false;
    }

    HANDLE h_map = CreateFileMappingA(h_file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    if (!h_map) {
        CloseHandle(h_file);
        return false;
    }

    LPVOID clean = MapViewOfFile(h_map, FILE_MAP_READ, 0, 0, 0);
    if (!clean) {
        CloseHandle(h_map);
        CloseHandle(h_file);
        return false;
    }

    HMODULE live = GetModuleHandleA("ntdll.dll");
    const bool ok = syscalls_init(log, clean, live);

    UnmapViewOfFile(clean);
    CloseHandle(h_map);
    CloseHandle(h_file);
    return ok;
}

bool syscalls_init(LabLogger& log, LPVOID clean_ntdll, HMODULE live_ntdll) {
    if (g_nt_protect_virtual_memory) {
        return true;
    }

    const PVOID clean_export = resolve_export_from_clean(
        clean_ntdll, live_ntdll, "NtProtectVirtualMemory");
    if (!clean_export) {
        log.log("[BadDll] syscalls_init: could not resolve NtProtectVirtualMemory from disk ntdll.");
        return false;
    }

    g_nt_protect_ssn = extract_ssn(clean_export);
    if (g_nt_protect_ssn == 0) {
        log.log("[BadDll] syscalls_init: failed to extract SSN for NtProtectVirtualMemory.");
        return false;
    }

    g_nt_protect_virtual_memory = build_syscall_stub(g_nt_protect_ssn);
    if (!g_nt_protect_virtual_memory) {
        log.log("[BadDll] syscalls_init: failed to build syscall stub page.");
        return false;
    }

    log.logf("[BadDll] NtProtectVirtualMemory SSN=0x%02lX", g_nt_protect_ssn);
    return true;
}

NTSTATUS sys_nt_protect_virtual_memory(
    HANDLE process_handle,
    PVOID* base_address,
    PSIZE_T region_size,
    ULONG new_protect,
    PULONG old_protect) {
    if (!g_nt_protect_virtual_memory) {
        return static_cast<NTSTATUS>(0xC0000001L);
    }
    return g_nt_protect_virtual_memory(
        process_handle, base_address, region_size, new_protect, old_protect);
}
