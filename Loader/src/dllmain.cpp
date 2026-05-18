#include "loader.hpp"

static DWORD WINAPI LoaderInitThread(LPVOID param) {
    loader::init(static_cast<HMODULE>(param));
    return 0;
}

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, LoaderInitThread, instance, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
