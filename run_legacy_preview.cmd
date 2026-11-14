@echo off
setlocal
cd /d "%~dp0"
set "APPDATA=%~dp0work\appdata"
set "LOCALAPPDATA=%~dp0work\localappdata"
if not exist "%APPDATA%" mkdir "%APPDATA%"
if not exist "%LOCALAPPDATA%" mkdir "%LOCALAPPDATA%"
set "OCTO_PREVIEW_ENGINE=C:\Tools\Godot\godot.exe"
if not exist "%OCTO_PREVIEW_ENGINE%" set "OCTO_PREVIEW_ENGINE=godot"
"%OCTO_PREVIEW_ENGINE%" --path "%~dp0game" --single-threaded-scene --render-thread safe --resolution 1280x800 res://scenes/octopus_preview.tscn
if errorlevel 1 pause
endlocal
