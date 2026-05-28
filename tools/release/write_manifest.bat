@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
where bash >nul 2>nul
if errorlevel 1 (
  echo [write_manifest] bash not found. Install Git for Windows or run write_manifest.sh from Git Bash. 1>&2
  exit /b 2
)

bash "%SCRIPT_DIR%write_manifest.sh" %*
exit /b %ERRORLEVEL%
