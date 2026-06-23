#pragma once

#include <windows.h>

class LabLogger;

void start_integrity_watchdog(LabLogger& log, DWORD interval_ms = 1500);
void stop_integrity_watchdog(LabLogger& log);
