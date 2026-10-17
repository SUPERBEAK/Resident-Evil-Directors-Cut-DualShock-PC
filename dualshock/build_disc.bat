@echo off
setlocal
rem build_disc.bat - compile the 13 game executables from the decompiled C and build a disc image
rem laid out like the original Dual Shock (USA) dump: (Track 1).bin + (Track 2).bin + .cue.
rem Output: decomp\build\disc\   (the original disc is read from game_files\RE1-Dualshock)

set "DECOMP=%~dp0."
set "TOOLS=%~dp0..\tools\external"

echo [1/2] Compiling the 13 executables from C (make clean + make)...
"%TOOLS%\msys64\usr\bin\bash.exe" "%DECOMP%\tools\build_disc_make.sh" "%TOOLS%" "%DECOMP%"
if errorlevel 1 goto fail

echo.
echo [2/2] Building the disc image...
"%TOOLS%\venv\Scripts\python.exe" "%DECOMP%\tools\build_disc.py"
if errorlevel 1 goto fail

echo.
echo Done.
pause
exit /b 0

:fail
echo.
echo BUILD FAILED - see the messages above.
pause
exit /b 1
