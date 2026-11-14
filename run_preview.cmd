@echo off
setlocal
cd /d "%~dp0"
echo [OCTO] Running GameReady v5 live physics (not the old procedural preview).
echo [OCTO] For the old visual prototype, use run_legacy_preview.cmd.
call "%~dp0run_live_demo.cmd"
exit /b %ERRORLEVEL%
