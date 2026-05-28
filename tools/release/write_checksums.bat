@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
where bash >nul 2>nul
if errorlevel 1 (
  echo [write_checksums] bash not found. Install Git for Windows or run write_checksums.sh from Git Bash. 1>&2
  exit /b 2
)

bash "%SCRIPT_DIR%write_checksums.sh" %*
exit /b %ERRORLEVEL%
