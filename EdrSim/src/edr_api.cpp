#include "edr_api.hpp"

#include "lab_log.h"
#include "simulate.hpp"
#include "watchdog.hpp"

namespace {

LabLogger* g_api_log = nullptr;

}  // namespace

void edr_api_set_logger(LabLogger& log) {
    g_api_log = &log;
}

extern "C" void WINAPI EdrSim_InstallImmediateHooks() {
    if (g_api_log) {
        install_immediate_hooks(*g_api_log);
    }
}

extern "C" void WINAPI EdrSim_StopWatchdog() {
    if (g_api_log) {
        stop_integrity_watchdog(*g_api_log);
    }
}
