#pragma once

#include <windows.h>

#include "nt_types.h"

class LabLogger;

bool syscalls_init(LabLogger& log, LPVOID clean_ntdll, HMODULE live_ntdll);
bool syscalls_init_from_disk(LabLogger& log);
NTSTATUS sys_nt_protect_virtual_memory(
    HANDLE process_handle,
    PVOID* base_address,
    PSIZE_T region_size,
    ULONG new_protect,
    PULONG old_protect);
