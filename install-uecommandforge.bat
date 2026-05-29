@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%tools\windows\bootstrap_dependencies.bat" core
if errorlevel 1 exit /b %ERRORLEVEL%

"%UECF_BASH_EXE%" "%SCRIPT_DIR%install-uecommandforge.sh" %*
exit /b %ERRORLEVEL%
