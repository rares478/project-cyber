# Build instructions (MinGW / CLion)

For full architecture, components, and implementation details, see [README.md](README.md).
## Prerequisites

- Windows 10+
- CMake 3.24+
- JetBrains CLion MinGW (or any MinGW-w64 g++ 13+ on `PATH`)

Detours source is vendored under `third_party/microsoft-detours` (clone via `scripts/build.ps1`).

## Build

```powershell
.\scripts\build.ps1
```

Output: `deploy/` contains `App.exe`, `version.dll`, `BadDll.dll`, `Injector.exe`, `Loader.config.json`.

## Demos

All output is printed to your terminal **and** written to per-component log files in `deploy/`:

| Component | Log file |
|-----------|----------|
| App.exe | `app.log` |
| version.dll (Loader) | `version.log` |
| BadDll.dll | `bad_dll.log` |
| Injector.exe | `injector.log` |

`demo.ps1` prints all log file contents automatically when the demo finishes.

```powershell
.\scripts\demo.ps1 -Mode insecure-inprocess
.\scripts\demo.ps1 -Mode insecure-remote
.\scripts\demo.ps1 -Mode secure
```

Each demo runs `App.exe --demo` (5 short status lines). Loader and payload messages appear in the same console.

## MSVC fallback (if MinGW + Detours link fails)

1. Open **x64 Native Tools Command Prompt for VS**.
2. `cd third_party\microsoft-detours\src` → `nmake`
3. Point `BadDll` CMake at `detours.lib` and build BadDll with MSVC while keeping other targets on MinGW, or build the full solution with MSVC.

## Config

Edit `deploy/Loader.config.json`:

- `"mode": "inprocess"` — Method 1 (`LoadLibrary` in same process)
- `"mode": "remote"` — Method 2 (`Injector.exe` + `CreateRemoteThread`)
