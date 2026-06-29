#include "loader.hpp"
#include "lab_log.h"

#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace loader {

namespace {

LabLogger g_log;
HMODULE g_self = nullptr;

std::wstring get_module_directory(HMODULE module) {
    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(module, path, MAX_PATH) == 0) {
        return {};
    }
    fs::path p(path);
    return p.parent_path().wstring();
}

std::wstring get_host_executable_name() {
    wchar_t path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) {
        return {};
    }
    return fs::path(path).filename().wstring();
}

bool iequals(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (towlower(a[i]) != towlower(b[i])) {
            return false;
        }
    }
    return true;
}

bool is_target_process(const Config& config) {
    if (config.targets.empty()) {
        return true;
    }
    const std::wstring host = get_host_executable_name();
    for (const auto& target : config.targets) {
        if (iequals(host, target)) {
            return true;
        }
    }
    return false;
}

std::wstring resolve_path(const std::wstring& base_dir, const std::wstring& relative) {
    if (relative.empty()) {
        return {};
    }
    fs::path p(relative);
    if (p.is_absolute()) {
        return fs::weakly_canonical(p).wstring();
    }
    return fs::weakly_canonical(fs::path(base_dir) / p).wstring();
}

void log_wide(const wchar_t* msg) {
    g_log.log_w(msg);
}

bool inject_inprocess(const std::wstring& dll_path, const wchar_t* label) {
    g_log.logf("[version.dll] LoadLibrary (%ls)", label);
    HMODULE mod = LoadLibraryW(dll_path.c_str());
    if (!mod) {
        std::wstringstream ss;
        ss << L"[version.dll] LoadLibrary failed (" << GetLastError() << L"): " << dll_path;
        log_wide(ss.str().c_str());
        return false;
    }
    log_wide((std::wstring(L"[version.dll] Loaded ") + label + L": " + dll_path).c_str());
    return true;
}

bool load_edr_simulator(const Config& config, const std::wstring& base_dir) {
    if (!config.simulate_edr) {
        return true;
    }

    const std::wstring edr_sim_path = resolve_path(base_dir, config.edr_sim);
    if (!fs::exists(edr_sim_path)) {
        log_wide((L"[version.dll] EDR simulator not found: " + edr_sim_path).c_str());
        return false;
    }

    g_log.log("[version.dll] simulate_edr enabled — loading EdrSim before payload.");
    return inject_inprocess(edr_sim_path, L"EdrSim");
}

bool inject_remote(const std::wstring& injector_path, const std::wstring& payload_path) {
    const DWORD pid = GetCurrentProcessId();
    g_log.logf("[version.dll] Method 2: spawning Injector for PID %lu", pid);

    std::wstringstream cmd;
    cmd << L"\"" << injector_path << L"\" " << pid << L" \"" << payload_path << L"\"";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::wstring cmdline = cmd.str();
    log_wide((L"[version.dll] CreateProcess: " + cmdline).c_str());

    if (!CreateProcessW(
            nullptr,
            cmdline.data(),
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi)) {
        std::wstringstream ss;
        ss << L"[version.dll] CreateProcess failed (" << GetLastError() << L")";
        log_wide(ss.str().c_str());
        return false;
    }

    WaitForSingleObject(pi.hProcess, 30'000);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (exit_code != 0) {
        g_log.logf("[version.dll] Injector exit code: %lu", exit_code);
        return false;
    }

    g_log.log("[version.dll] Remote injection completed successfully.");
    return true;
}

}  // namespace

void init(HMODULE self_module) {
    g_self = self_module;
    g_log.init("version.log", self_module);
    g_log.logf("[version.dll] Proxy loaded (PID %lu)", GetCurrentProcessId());

    const std::wstring base_dir = get_module_directory(self_module);
    log_wide((L"[version.dll] Base directory: " + base_dir).c_str());

    Config config;
    const std::wstring config_path = resolve_path(base_dir, L"Loader.config.json");
    log_wide((L"[version.dll] Reading config: " + config_path).c_str());

    if (!load_config(config_path, config)) {
        g_log.log("[version.dll] Config missing or invalid — using defaults.");
        config.targets.push_back(L"App.exe");
    } else {
        g_log.log("[version.dll] Config loaded.");
    }

    if (!config.enabled) {
        g_log.log("[version.dll] Disabled in config — no injection.");
        return;
    }

    const std::wstring host = get_host_executable_name();
    log_wide((L"[version.dll] Host executable: " + host).c_str());

    if (!is_target_process(config)) {
        g_log.log("[version.dll] Host is not a configured target — skipping.");
        return;
    }

    log_wide((L"[version.dll] Injection mode: " + config.mode).c_str());

    const std::wstring payload_path = resolve_path(base_dir, config.payload);
    if (!fs::exists(payload_path)) {
        log_wide((L"[version.dll] Payload not found: " + payload_path).c_str());
        return;
    }

    if (iequals(config.mode, L"remote")) {
        const std::wstring injector_path = resolve_path(base_dir, config.injector);
        if (!fs::exists(injector_path)) {
            log_wide((L"[version.dll] Injector not found: " + injector_path).c_str());
            return;
        }
        if (config.simulate_edr) {
            const std::wstring edr_sim_path = resolve_path(base_dir, config.edr_sim);
            if (fs::exists(edr_sim_path)) {
                g_log.log("[version.dll] simulate_edr enabled — remote inject EdrSim first.");
                inject_remote(injector_path, edr_sim_path);
            } else {
                log_wide((L"[version.dll] EDR simulator not found: " + edr_sim_path).c_str());
            }
        }
        inject_remote(injector_path, payload_path);
    } else {
        g_log.log("[version.dll] Method 1: in-process LoadLibrary");
        load_edr_simulator(config, base_dir);
        inject_inprocess(payload_path, L"payload");
    }
}

void shutdown_injected_modules() {
    HMODULE edr = GetModuleHandleW(L"EdrSim.dll");
    if (edr) {
        using StopFn = void (WINAPI*)();
        if (const auto stop = reinterpret_cast<StopFn>(GetProcAddress(edr, "EdrSim_StopWatchdog"))) {
            stop();
        }
        g_log.log("[version.dll] Stopped EdrSim watchdog before unload.");
    }

    HMODULE payload = GetModuleHandleW(L"BadDll.dll");
    if (payload) {
        using PrepFn = void (WINAPI*)();
        if (const auto prep = reinterpret_cast<PrepFn>(GetProcAddress(payload, "BadDll_PrepareUnload"))) {
            prep();
        }
        g_log.log("[version.dll] Unloading BadDll.dll before version detach.");
        FreeLibrary(payload);
    }

    if (edr) {
        g_log.log("[version.dll] Unloading EdrSim.dll before version detach.");
        FreeLibrary(edr);
    }
}

}  // namespace loader
