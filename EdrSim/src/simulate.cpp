#include "simulate.hpp"

#include <climits>
#include <cstdint>
#include <cstring>
#include <vector>

#include "lab_log.h"

namespace {

struct HookSite {
    const char* export_name = nullptr;
    BYTE* target = nullptr;
    BYTE expected_patch[5]{};
    bool active = false;
    bool install_immediately = false;
};

static std::vector<HookSite> g_hooks;
static bool g_deferred_hooks_installed = false;

static LONG NTAPI dummy_nt_stub() {
    return 0;  // STATUS_SUCCESS
}

static bool apply_jmp_patch(LabLogger& log, HookSite& site, const char* reason) {
    site.target = reinterpret_cast<BYTE*>(GetProcAddress(GetModuleHandleA("ntdll.dll"), site.export_name));
    if (!site.target) {
        log.logf("[EdrSim] Export not found: %s", site.export_name);
        return false;
    }

    const auto handler = reinterpret_cast<BYTE*>(reinterpret_cast<void*>(&dummy_nt_stub));
    const int64_t rel = static_cast<int64_t>(handler - (site.target + 5));
    if (rel < INT32_MIN || rel > INT32_MAX) {
        log.logf("[EdrSim] JMP rel32 out of range for %s", site.export_name);
        return false;
    }

    BYTE patch[5]{};
    patch[0] = 0xE9;
    const int32_t rel32 = static_cast<int32_t>(rel);
    std::memcpy(patch + 1, &rel32, sizeof(rel32));

    DWORD old_protect = 0;
    if (!VirtualProtect(site.target, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
        log.logf("[EdrSim] VirtualProtect failed for %s (error %lu).", site.export_name, GetLastError());
        return false;
    }

    std::memcpy(site.target, patch, sizeof(patch));
    std::memcpy(site.expected_patch, patch, sizeof(patch));

    DWORD ignored = 0;
    VirtualProtect(site.target, sizeof(patch), old_protect, &ignored);

    site.active = true;
    if (reason) {
        log.logf(
            "[EdrSim] %s %s at %p (byte[0]=0x%02X)",
            reason,
            site.export_name,
            site.target,
            site.target[0]);
    }
    return true;
}

static bool hooks_tampered(const HookSite& site) {
    if (!site.active || !site.target) {
        return false;
    }
    return std::memcmp(site.target, site.expected_patch, sizeof(site.expected_patch)) != 0;
}

static void install_deferred_hooks(LabLogger& log) {
    if (g_deferred_hooks_installed) {
        return;
    }

    int hooked = 0;
    for (auto& site : g_hooks) {
        if (!site.install_immediately && !site.active) {
            if (apply_jmp_patch(log, site, "Hooked (deferred)")) {
                ++hooked;
            }
        }
    }
    g_deferred_hooks_installed = true;
    if (hooked > 0) {
        log.logf("[EdrSim] Installed %d deferred ntdll JMP hook(s).", hooked);
    }
}

}  // namespace

bool install_edr_hooks(LabLogger& log) {
    log.log("[EdrSim] Preparing EDR hook targets...");

    g_hooks.clear();
    g_deferred_hooks_installed = false;
    g_hooks.push_back({ "NtAllocateVirtualMemory", nullptr, {}, false, true });
    g_hooks.push_back({ "NtProtectVirtualMemory", nullptr, {}, false, false });

    log.log("[EdrSim] NtAllocate hook on payload request; NtProtect deferred to watchdog.");
    return true;
}

void install_immediate_hooks(LabLogger& log) {
    int hooked = 0;
    for (auto& site : g_hooks) {
        if (site.install_immediately && !site.active) {
            if (apply_jmp_patch(log, site, "Hooked")) {
                ++hooked;
            }
        }
    }
    if (hooked > 0) {
        log.logf("[EdrSim] Installed %d immediate ntdll JMP hook(s) (0xE9).", hooked);
    }
}

void check_and_restore_hooks(LabLogger& log) {
    install_deferred_hooks(log);

    for (auto& site : g_hooks) {
        if (!site.active || !site.target) {
            continue;
        }
        if (hooks_tampered(site)) {
            log.logf("[EdrSim] Integrity watchdog: tamper detected on %s", site.export_name);
            apply_jmp_patch(log, site, "Hook restored");
        }
    }
}

void remove_edr_hooks(LabLogger& log) {
    for (auto& site : g_hooks) {
        site.active = false;
    }
    g_hooks.clear();
    g_deferred_hooks_installed = false;
    log.log("[EdrSim] EDR hooks cleared.");
}
