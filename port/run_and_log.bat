@echo off
set EXE_PATH=h:\Antigravity Projects\ultimate dualshock\port\build\Release\re1_dualshock_port.exe
if exist "%EXE_PATH%" (
    echo Running %EXE_PATH%...
    "%EXE_PATH%" > output_log.txt 2>&1
    echo Done. Check output_log.txt
) else (
    echo Error: Executable not found at %EXE_PATH%
    pause
)
