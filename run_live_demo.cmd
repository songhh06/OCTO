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
if not exist "%~dp0game\assets\Octopus_GameReady_v5.glb" (
  echo.
  echo [ERROR] GameReady v5 model is NOT installed in this checkout.
  echo Expected: %~dp0game\assets\Octopus_GameReady_v5.glb
  echo Extract OCTO_GameReady_v5_install_and_test.zip into the OCTO repository ROOT.
  echo Current project branch must be feature/P2V-model-curved-contact-body.
  echo The old procedural model will NOT be substituted silently.
  pause
  exit /b 2
)
echo [OCTO] Checking the REAL GameReady v5 GLB before launching.
python "%~dp0tools\check_octopus_model.py" "%~dp0game\assets\Octopus_GameReady_v5.glb"
if errorlevel 1 (
  echo [ERROR] GameReady GLB validation failed.
  pause
  exit /b 3
)
"%OCTO_GODOT%" --headless --path "%~dp0game" --editor --quit
if errorlevel 1 (
  echo [ERROR] Godot model import failed.
  pause
  exit /b 4
)
echo [OCTO] Launching GAME READY V5 with native FEM torso; F3 toggles FEM debug surface.
"%OCTO_GODOT%" --path "%~dp0game" --single-threaded-scene --render-thread safe --resolution 1280x800 res://scenes/live_demo.tscn
if errorlevel 1 pause
endlocal
