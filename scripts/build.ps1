# Build the DLL injection lab (requires CLion bundled MinGW on PATH).
$ErrorActionPreference = "Stop"

$mingwBin = "C:\Program Files\JetBrains\CLion 2025.2.1\bin\mingw\bin"
$ninja = "C:\Program Files\JetBrains\CLion 2025.2.1\bin\ninja\win\x64"
$env:Path = "$mingwBin;$ninja;$env:Path"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not (Test-Path "third_party\microsoft-detours\src\detours.cpp")) {
    git clone --depth 1 https://github.com/microsoft/Detours.git third_party/microsoft-detours
}

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug `
    "-DCMAKE_MAKE_PROGRAM=$ninja/ninja.exe" `
    "-DCMAKE_CXX_COMPILER=$mingwBin/g++.exe" `
    "-DCMAKE_C_COMPILER=$mingwBin/gcc.exe"

cmake --build build --target deploy

Write-Host ""
Write-Host "Build OK. Artifacts in: $root\deploy" -ForegroundColor Green
Write-Host ""
Write-Host "Run a demo (logs print below automatically):" -ForegroundColor Cyan
Write-Host "  .\scripts\demo.ps1 -Mode insecure-inprocess"
Write-Host "  .\scripts\demo.ps1 -Mode insecure-remote"
Write-Host "  .\scripts\demo.ps1 -Mode secure"
