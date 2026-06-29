#include <windows.h>

#include "edr_api.hpp"
#include "lab_log.h"
#include "simulate.hpp"
#include "watchdog.hpp"

static LabLogger g_log;

void edr_api_set_logger(LabLogger& log);

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            g_log.init("edr_sim.log", instance);
            edr_api_set_logger(g_log);
            g_log.logf("[EdrSim] Loaded into PID %lu", GetCurrentProcessId());
            install_edr_hooks(g_log);
            start_integrity_watchdog(g_log, 1500);
            break;
        case DLL_PROCESS_DETACH:
            stop_integrity_watchdog(g_log);
            remove_edr_hooks(g_log);
            break;
        default:
            break;
    }
    return TRUE;
}
