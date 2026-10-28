@echo off
setlocal

echo [BUILD] Checking for running re1_dualshock_port.exe...
taskkill /F /IM re1_dualshock_port.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [BUILD] Terminated running re1_dualshock_port.exe.
)

set SCRIPT_DIR=%~dp0
set PORT_DIR=%SCRIPT_DIR%port
set BUILD_DIR=%PORT_DIR%\build

set CMAKE_EXE=
if exist "C:\Program Files\CMake\bin\cmake.exe" (
    set "CMAKE_EXE=C:\Program Files\CMake\bin\cmake.exe"
) else (
    where cmake >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        set CMAKE_EXE=cmake
    )
)

if "%CMAKE_EXE%"=="" (
    echo [ERROR] CMake executable could not be located.
    exit /b 1
)

if not exist "%BUILD_DIR%" (
    echo [BUILD] Generating build directory...
    "%CMAKE_EXE%" -S "%PORT_DIR%" -B "%BUILD_DIR%"
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] CMake configuration failed.
        exit /b %ERRORLEVEL%
    )
)

echo [BUILD] Compiling Release build...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release
set BUILD_STATUS=%ERRORLEVEL%

if %BUILD_STATUS% EQU 0 (
    echo [BUILD SUCCESS] Binary: %BUILD_DIR%\Release\re1_dualshock_port.exe
) else (
    echo [BUILD FAILED] Build failed with exit code %BUILD_STATUS%
)

exit /b %BUILD_STATUS%
