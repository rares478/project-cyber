#include "watchdog.hpp"

#include "lab_log.h"
#include "simulate.hpp"

namespace {

volatile bool g_running = false;
HANDLE g_thread = nullptr;
DWORD g_interval_ms = 1500;
DWORD g_initial_delay_ms = 2500;

DWORD WINAPI watchdog_thread(LPVOID param) {
    auto* log = static_cast<LabLogger*>(param);
    Sleep(g_initial_delay_ms);
    while (g_running) {
        check_and_restore_hooks(*log);
        Sleep(g_interval_ms);
    }
    return 0;
}

}  // namespace

void start_integrity_watchdog(LabLogger& log, DWORD interval_ms) {
    if (g_thread) {
        return;
    }

    g_interval_ms = interval_ms;
    g_running = true;
    g_thread = CreateThread(nullptr, 0, watchdog_thread, &log, 0, nullptr);
    if (!g_thread) {
        g_running = false;
        log.logf("[EdrSim] Failed to start integrity watchdog (error %lu).", GetLastError());
        return;
    }

    log.logf("[EdrSim] Integrity watchdog started (initial delay %lu ms, interval %lu ms).", g_initial_delay_ms, interval_ms);
}

void stop_integrity_watchdog(LabLogger& log) {
    if (!g_thread) {
        return;
    }

    g_running = false;
    WaitForSingleObject(g_thread, 5000);
    CloseHandle(g_thread);
    g_thread = nullptr;
    log.log("[EdrSim] Integrity watchdog stopped.");
}
