@echo off
setlocal
cd /d "%~dp0"
set "APPDATA=%~dp0work\appdata"
set "LOCALAPPDATA=%~dp0work\localappdata"
if not exist "build\release\octo_live.exe" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build-live-demo.ps1"
  if errorlevel 1 (
    pause
    exit /b 1
  )
)
"C:\Tools\Godot\godot.exe" --path "%~dp0game" --single-threaded-scene --render-thread safe --resolution 1280x800 res://scenes/live_demo.tscn
if errorlevel 1 pause
endlocal
