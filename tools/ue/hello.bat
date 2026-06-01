@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%run_commandlet.bat" Hello %*
exit /b %ERRORLEVEL%
