@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
where bash >nul 2>nul
if errorlevel 1 (
  echo [verify_release_package] bash not found. Install Git for Windows or run verify_release_package.sh from Git Bash. 1>&2
  exit /b 2
)

bash "%SCRIPT_DIR%verify_release_package.sh" %*
exit /b %ERRORLEVEL%
