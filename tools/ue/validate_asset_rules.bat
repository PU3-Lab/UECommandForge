@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
  echo Usage: %~nx0 ^<policy.json^> [extra args...] 1>&2
  exit /b 2
)

set "SCRIPT_DIR=%~dp0"
set "POLICY_PATH=%~1"
shift /1

set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=!EXTRA_ARGS! "%~1"
shift /1
goto collect_args

:run
call "%SCRIPT_DIR%run_commandlet.bat" ValidateAssetRules -Policy="%POLICY_PATH%" %EXTRA_ARGS%
exit /b %ERRORLEVEL%
