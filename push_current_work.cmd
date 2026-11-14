@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\push-current-work.ps1"
if errorlevel 1 (
 echo Upload failed. Read the error above.
 pause
 exit /b 1
)
pause
