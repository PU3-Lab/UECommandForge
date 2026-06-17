@echo off
setlocal EnableExtensions DisableDelayedExpansion

if "%~1"=="" (
  echo Usage: %~nx0 ^<diff.allowlist.json^> [extra args...] 1>&2
  exit /b 2
)

set "SCRIPT_DIR=%~dp0"
set "ALLOWLIST_PATH=%~f1"
shift /1

set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=%EXTRA_ARGS% "%~1"
shift /1
goto collect_args

:run
call "%SCRIPT_DIR%run_commandlet.bat" DiffPlatformConfig -Allowlist="%ALLOWLIST_PATH%" %EXTRA_ARGS%
exit /b %ERRORLEVEL%
