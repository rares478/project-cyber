#pragma once

#include <windows.h>

class LabLogger;

void install_immediate_hooks(LabLogger& log);

extern "C" {
__declspec(dllexport) void WINAPI EdrSim_InstallImmediateHooks();
__declspec(dllexport) void WINAPI EdrSim_StopWatchdog();
}
