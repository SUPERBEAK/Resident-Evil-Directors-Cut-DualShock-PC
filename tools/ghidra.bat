@echo off
:: ghidra.bat - start an installed Ghidra 12.1.3 with a project-local settings/cache dir.
:: The PS1 loader (ghidra_psx_ldr, installed by setup_tools.sh) lives in that settings dir,
:: so your normal Ghidra profile in %APPDATA% is never touched.
::
:: Usage: set GHIDRA_DIR to your Ghidra folder (the one containing ghidraRun.bat), then run this file:
::   set GHIDRA_DIR=C:\path\to\ghidra_12.1.3_PUBLIC
::   tools\ghidra.bat
setlocal
if not defined GHIDRA_DIR (
    echo GHIDRA_DIR is not set: set it to your Ghidra 12.1.3 folder, the one containing ghidraRun.bat.
    exit /b 1
)
set "TOOLS=%~dp0external"
for %%I in ("%TOOLS%") do set "TOOLS=%%~fI"
set "TEMPDIR=%~dp0..\temp"
for %%I in ("%TEMPDIR%") do set "TEMPDIR=%%~fI"
set "GHIDRA_JAVA_OPTIONS=%GHIDRA_JAVA_OPTIONS% -Dapplication.settingsdir=%TOOLS%\ghidra_settings -Dapplication.cachedir=%TOOLS%\ghidra_cache -Dapplication.tempdir=%TEMPDIR%"
call "%GHIDRA_DIR%\ghidraRun.bat" %*
