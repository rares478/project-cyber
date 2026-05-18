#include <windows.h>

bool install_hooks(HMODULE self);
void remove_hooks();

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            install_hooks(instance);
            break;
        case DLL_PROCESS_DETACH:
            remove_hooks();
            break;
        default:
            break;
    }
    return TRUE;
}
