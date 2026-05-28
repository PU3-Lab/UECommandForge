@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
where bash >nul 2>nul
if errorlevel 1 (
  echo [package_tools] bash not found. Install Git for Windows or run package_tools.sh from Git Bash. 1>&2
  exit /b 2
)

bash "%SCRIPT_DIR%package_tools.sh" %*
exit /b %ERRORLEVEL%
