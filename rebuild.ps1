# Rebuild script for PowerShell
$ErrorActionPreference = "Continue"

Write-Host "[BUILD] Checking for running re1_dualshock_port.exe..." -ForegroundColor Cyan
Get-Process -Name "re1_dualshock_port" -ErrorAction SilentlyContinue | Stop-Process -Force

$PortDir = Join-Path $PSScriptRoot "port"
$BuildDir = Join-Path $PortDir "build"

$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCmd) { $cmakeCmd.Source } else { $null }

if (-not $cmake -and (Test-Path "C:\Program Files\CMake\bin\cmake.exe")) {
    $cmake = "C:\Program Files\CMake\bin\cmake.exe"
}
if (-not $cmake) { $cmake = "cmake" }

if (-not (Test-Path $BuildDir)) {
    Write-Host "[BUILD] Configuring CMake..." -ForegroundColor Cyan
    & $cmake -S "$PortDir" -B "$BuildDir"
}

Write-Host "[BUILD] Compiling Release..." -ForegroundColor Cyan
& $cmake --build "$BuildDir" --config Release
if ($LASTEXITCODE -eq 0) {
    Write-Host "[BUILD SUCCESS] Binary: $BuildDir\Release\re1_dualshock_port.exe" -ForegroundColor Green
} else {
    Write-Host "[BUILD FAILED] Code: $LASTEXITCODE" -ForegroundColor Red
}
exit $LASTEXITCODE
