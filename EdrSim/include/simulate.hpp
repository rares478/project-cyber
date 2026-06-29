#pragma once

#include <windows.h>

class LabLogger;

bool install_edr_hooks(LabLogger& log);
void install_immediate_hooks(LabLogger& log);
void remove_edr_hooks(LabLogger& log);
void check_and_restore_hooks(LabLogger& log);
