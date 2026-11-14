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
if exist "%~dp0game\assets\Octopus_GameReady_v5.glb" (
  python "%~dp0tools\check_octopus_model.py" "%~dp0game\assets\Octopus_GameReady_v5.glb"
  if errorlevel 1 (
    echo Uploaded octopus GLB validation failed.
    pause
    exit /b 1
  )
  "%OCTO_GODOT%" --headless --path "%~dp0game" --editor --quit
  if errorlevel 1 (
    echo Godot model import failed.
    pause
    exit /b 1
  )
)
"%OCTO_GODOT%" --path "%~dp0game" --single-threaded-scene --render-thread safe --resolution 1280x800 res://scenes/live_demo.tscn
if errorlevel 1 pause
endlocal
