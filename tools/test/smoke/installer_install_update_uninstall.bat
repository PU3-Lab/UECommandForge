@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%..\..\windows\bootstrap_dependencies.bat" core
if errorlevel 1 exit /b %ERRORLEVEL%

"%UECF_BASH_EXE%" "%SCRIPT_DIR%installer_install_update_uninstall.sh" %*
exit /b %ERRORLEVEL%
