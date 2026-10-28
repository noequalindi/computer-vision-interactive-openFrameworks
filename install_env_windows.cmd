@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install_windows.ps1" %*
set "kit_result=%ERRORLEVEL%"
pause
exit /b %kit_result%
