@echo off
setlocal
cd /d "%~dp0"
set "APPDATA=%~dp0work\appdata"
set "LOCALAPPDATA=%~dp0work\localappdata"
rem Always rebuild incrementally; a checked-in .exe may be stale after a code change.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build-live-demo.ps1"
if errorlevel 1 (
  pause
  exit /b 1
)
set "OCTO_GODOT=C:\Tools\Godot\godot.exe"
if not exist "%OCTO_GODOT%" set "OCTO_GODOT=godot"
"%OCTO_GODOT%" --path "%~dp0game" --single-threaded-scene --render-thread safe --resolution 1280x800 res://scenes/live_demo.tscn
if errorlevel 1 pause
endlocal
