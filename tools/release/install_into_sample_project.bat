@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
where bash >nul 2>nul
if errorlevel 1 (
  echo [install_into_sample_project] bash not found. Install Git for Windows or run install_into_sample_project.sh from Git Bash. 1>&2
  exit /b 2
)

bash "%SCRIPT_DIR%install_into_sample_project.sh" %*
exit /b %ERRORLEVEL%
