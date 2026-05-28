@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
where bash >nul 2>nul
if errorlevel 1 (
  echo [install_local] bash not found. Install Git for Windows or run install_local.sh from Git Bash. 1>&2
  exit /b 2
)

bash "%SCRIPT_DIR%install_local.sh" %*
exit /b %ERRORLEVEL%
