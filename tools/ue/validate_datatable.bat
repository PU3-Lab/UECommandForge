@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
  echo Usage: %~nx0 ^<schema.json^> [asset_path] [extra args...] 1>&2
  exit /b 2
)

set "SCRIPT_DIR=%~dp0"
set "SCHEMA_PATH=%~f1"
shift /1

set "ASSET_ARG="
if not "%~1"=="" (
  set "FIRST_ARG=%~1"
  if not "!FIRST_ARG:~0,1!"=="-" (
    set ASSET_ARG=-Asset="%~1"
    shift /1
  )
)

set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=!EXTRA_ARGS! "%~1"
shift /1
goto collect_args

:run
call "%SCRIPT_DIR%run_commandlet.bat" ValidateDataTable -Schema="%SCHEMA_PATH%" %ASSET_ARG% %EXTRA_ARGS%
exit /b %ERRORLEVEL%
