@echo off
setlocal EnableExtensions DisableDelayedExpansion

if "%~1"=="" (
  echo Usage: %~nx0 ^<rollback.json^> [extra args...] 1>&2
  exit /b 2
)

set "SCRIPT_DIR=%~dp0"
set "ROLLBACK_PATH=%~1"
shift /1

set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=%EXTRA_ARGS% "%~1"
shift /1
goto collect_args

:run
call "%SCRIPT_DIR%run_commandlet.bat" RollbackAssetChanges -RollbackPlan="%ROLLBACK_PATH%" %EXTRA_ARGS%
exit /b %ERRORLEVEL%
