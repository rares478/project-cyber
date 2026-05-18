# Quick demo helper — all logs print to this console automatically.
param(
    [ValidateSet("insecure-inprocess", "insecure-remote", "secure")]
    [string]$Mode = "insecure-inprocess"
)

$ErrorActionPreference = "Stop"

$deploy = Join-Path (Split-Path -Parent $PSScriptRoot) "deploy"
if (-not (Test-Path (Join-Path $deploy "App.exe"))) {
    Write-Error "Run scripts/build.ps1 first."
}

$configPath = Join-Path $deploy "Loader.config.json"
$config = Get-Content $configPath -Raw | ConvertFrom-Json

$appArgs = @("--demo")

switch ($Mode) {
    "insecure-inprocess" {
        $config.mode = "inprocess"
        $appArgs = @("--insecure", "--demo")
        $expected = "INJECTED"
    }
    "insecure-remote" {
        $config.mode = "remote"
        $appArgs = @("--insecure", "--demo")
        $expected = "INJECTED"
    }
    "secure" {
        $config.mode = "inprocess"
        $appArgs = @("--secure", "--demo")
        $expected = "OK (App.exe)"
    }
}

$config | ConvertTo-Json -Depth 4 | Set-Content $configPath -Encoding UTF8

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " DLL injection lab demo: $Mode" -ForegroundColor Cyan
Write-Host " Loader.config mode: $($config.mode)" -ForegroundColor Cyan
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
