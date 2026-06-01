@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "SCRIPT_DIR=%~dp0"
if "%~1"=="" (
  echo Usage: %~nx0 ^<blueprint-defaults.json^> [extra args...] 1>&2
  exit /b 2
)
set "SPEC_FILE=%~f1"
shift /1
set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=%EXTRA_ARGS% "%~1"
shift /1
goto collect_args
:run
call "%SCRIPT_DIR%run_commandlet.bat" SetBlueprintDefaults "-Spec=%SPEC_FILE%" %EXTRA_ARGS%
exit /b %ERRORLEVEL%
