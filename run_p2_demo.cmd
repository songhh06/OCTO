@echo off
setlocal
cd /d "%~dp0"
set "APPDATA=%~dp0work\appdata"
set "LOCALAPPDATA=%~dp0work\localappdata"
if not exist "game\data\demo\bend.json" (
  echo Demo recordings missing. See docs/P2_DEMO.md to regenerate.
  pause
  exit /b 1
)
"C:\Tools\Godot\godot.exe" --path "%~dp0game" --single-threaded-scene --render-thread safe --resolution 1280x800 res://scenes/physics_demo.tscn
if errorlevel 1 pause
endlocal
