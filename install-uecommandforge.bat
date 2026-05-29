@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%tools\windows\bootstrap_dependencies.bat" core
if errorlevel 1 exit /b %ERRORLEVEL%

set "CHERE_INVOKING=1"
"%UECF_BASH_EXE%" -l "%SCRIPT_DIR%install-uecommandforge.sh" %*
exit /b %ERRORLEVEL%
