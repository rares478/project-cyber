#pragma once

#include <windows.h>

// Per-component logger: mirrors to console and appends to a dedicated log file
// (app.log, version.log, bad_dll.log, injector.log) beside the host executable.
class LabLogger {
public:
    LabLogger() = default;

    void init(const char* log_file_name, HMODULE module = nullptr);
    void log(const char* message);
    void logf(const char* fmt, ...);
    void log_w(const wchar_t* message);

private:
    bool ready_ = false;
    char log_path_[MAX_PATH]{};
    void write_line(const char* line);
};
