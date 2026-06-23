#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace loader {

struct Config {
    bool enabled = true;
    bool simulate_edr = false;
    std::vector<std::wstring> targets;
    std::wstring mode = L"inprocess";  // inprocess | remote
    std::wstring payload = L"BadDll.dll";
    std::wstring edr_sim = L"EdrSim.dll";
    std::wstring injector = L"Injector.exe";
};

bool load_config(const std::wstring& config_path, Config& out);
void init(HMODULE self_module);

}  // namespace loader
