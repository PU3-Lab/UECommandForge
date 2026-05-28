@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%run_commandlet.bat" AssetSnapshot %*
exit /b %ERRORLEVEL%
