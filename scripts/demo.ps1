# Quick demo helper — all logs print to this console automatically.
param(
    [ValidateSet("insecure", "insecure-inprocess", "insecure-remote", "insecure-unhook", "secure")]
    [string]$Mode = "insecure-inprocess"
)

$ErrorActionPreference = "Stop"

if ($Mode -eq "insecure") {
    $Mode = "insecure-inprocess"
}

$deploy = Join-Path (Split-Path -Parent $PSScriptRoot) "deploy"
if (-not (Test-Path (Join-Path $deploy "App.exe"))) {
    Write-Error "Run scripts/build.ps1 first."
}

foreach ($log in @("app.log", "version.log", "edr_sim.log", "bad_dll.log", "injector.log")) {
    $p = Join-Path $deploy $log
    if (Test-Path $p) { Remove-Item $p -Force }
}

$configPath = Join-Path $deploy "Loader.config.json"
$config = Get-Content $configPath -Raw | ConvertFrom-Json

$appArgs = @("--demo")

switch ($Mode) {
    "insecure-inprocess" {
        $config.mode = "inprocess"
        $config.simulate_edr = $false
        $appArgs = @("--insecure", "--demo")
        $expected = "INJECTED"
    }
    "insecure-remote" {
        $config.mode = "remote"
        $config.simulate_edr = $false
        $appArgs = @("--insecure", "--demo")
        $expected = "INJECTED"
    }
    "insecure-unhook" {
        $config.mode = "inprocess"
        $config.simulate_edr = $true
        $appArgs = @("--insecure", "--demo")
        $expected = "INJECTED (EDR sim + syscall unhook; watchdog re-hooks ~1.5s)"
    }
    "secure" {
        $config.mode = "inprocess"
        $config.simulate_edr = $false
        $appArgs = @("--secure", "--demo")
        $expected = "OK (App.exe)"
    }
}

$config | ConvertTo-Json -Depth 4 | Set-Content $configPath -Encoding UTF8

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " DLL injection lab demo: $Mode" -ForegroundColor Cyan
Write-Host " Loader.config mode: $($config.mode)" -ForegroundColor Cyan
Write-Host " simulate_edr: $($config.simulate_edr)" -ForegroundColor Cyan
Write-Host " Expected status line: $expected" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

Push-Location $deploy
try {
    & ".\App.exe" @appArgs
    $exitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

Write-Host ""
if ($exitCode -ne 0) {
    Write-Host "App.exe exited with code $exitCode" -ForegroundColor Red
} else {
    Write-Host "Demo finished." -ForegroundColor Green
}

$logFiles = @(
    @{ Name = "App";       File = "app.log" },
    @{ Name = "version.dll"; File = "version.log" },
    @{ Name = "EdrSim";    File = "edr_sim.log" },
    @{ Name = "BadDll";    File = "bad_dll.log" },
    @{ Name = "Injector";  File = "injector.log" }
)

Write-Host ""
Write-Host "Log files (deploy/):" -ForegroundColor Cyan
foreach ($entry in $logFiles) {
    $path = Join-Path $deploy $entry.File
    if (Test-Path $path) {
        Write-Host ""
        Write-Host "--- $($entry.Name) -> $($entry.File) ---" -ForegroundColor Yellow
        Get-Content $path
    } else {
        Write-Host "  $($entry.File) (not created - $($entry.Name) did not run)" -ForegroundColor DarkGray
    }
}

exit $exitCode
