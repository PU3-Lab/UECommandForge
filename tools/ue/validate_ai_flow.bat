@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "SCRIPT_DIR=%~dp0"
if "%~1"=="" (
  echo Usage: %~nx0 ^<spec_file^> [extra args...] 1>&2
  exit /b 2
)
for %%I in ("%~1") do set "SPEC_FILE=%%~fI"
shift /1
set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=%EXTRA_ARGS% "%~1"
shift /1
goto collect_args
:run
call "%SCRIPT_DIR%run_commandlet.bat" ValidateAIFlow "-SpecFile=%SPEC_FILE%" %EXTRA_ARGS%
exit /b %ERRORLEVEL%
