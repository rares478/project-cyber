#include "lab_log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

void attach_parent_console() {
    static bool attached = false;
    if (attached) {
        return;
    }
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        attached = true;
    }
}

void get_base_directory(wchar_t* out, DWORD out_chars, HMODULE module) {
    if (module == nullptr) {
        module = GetModuleHandleW(nullptr);
    }
    if (GetModuleFileNameW(module, out, out_chars) == 0) {
        GetCurrentDirectoryW(out_chars, out);
        return;
    }

    wchar_t* last_slash = wcsrchr(out, L'\\');
    if (last_slash != nullptr) {
        *(last_slash + 1) = L'\0';
    }
}

}  // namespace

void LabLogger::init(const char* log_file_name, HMODULE module) {
    wchar_t dir[MAX_PATH]{};
    get_base_directory(dir, MAX_PATH, module);

    wchar_t wide_path[MAX_PATH]{};
    swprintf(wide_path, MAX_PATH, L"%ls%hs", dir, log_file_name);

    WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, log_path_, MAX_PATH, nullptr, nullptr);
    ready_ = true;

    write_line("========== log session start ==========");
}

void LabLogger::write_line(const char* line) {
    attach_parent_console();

    SYSTEMTIME st{};
    GetLocalTime(&st);
    char stamped[512];
    snprintf(
        stamped,
        sizeof(stamped),
        "[%04u-%02u-%02u %02u:%02u:%02u] %s",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        line);

    fprintf(stdout, "%s\n", stamped);
    fflush(stdout);
    OutputDebugStringA(stamped);
    OutputDebugStringA("\n");

    if (!ready_) {
        return;
    }

    FILE* file = fopen(log_path_, "a");
    if (file) {
        fprintf(file, "%s\n", stamped);
        fclose(file);
    }
}

void LabLogger::log(const char* message) {
    write_line(message);
}

void LabLogger::logf(const char* fmt, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    write_line(buffer);
}

void LabLogger::log_w(const wchar_t* message) {
    char utf8[1024];
    WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8, sizeof(utf8), nullptr, nullptr);
    write_line(utf8);
}
