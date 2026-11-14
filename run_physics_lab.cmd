@echo off
setlocal
cd /d "%~dp0"
set "APPDATA=%~dp0work\appdata"
set "LOCALAPPDATA=%~dp0work\localappdata"
if not exist "game\data\physics_trace.json" (
  echo No native physics trace. Run tools/physics-lab.ps1 first.
  pause
  exit /b 1
)
"C:\Tools\Godot\godot.exe" --path "%~dp0game" --single-threaded-scene --render-thread safe --resolution 1280x800 res://scenes/physics_lab.tscn
if errorlevel 1 pause
endlocal
