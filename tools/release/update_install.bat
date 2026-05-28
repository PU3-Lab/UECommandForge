@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
where bash >nul 2>nul
if errorlevel 1 (
  echo [update_install] bash not found. Install Git for Windows or run update_install.sh from Git Bash. 1>&2
  exit /b 2
)

bash "%SCRIPT_DIR%update_install.sh" %*
exit /b %ERRORLEVEL%
