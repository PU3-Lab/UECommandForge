@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%..\windows\bootstrap_dependencies.bat" core
if errorlevel 1 exit /b %ERRORLEVEL%

"%UECF_BASH_EXE%" "%SCRIPT_DIR%package_tools.sh" %*
exit /b %ERRORLEVEL%
