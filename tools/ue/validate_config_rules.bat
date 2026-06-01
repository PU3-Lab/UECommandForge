@echo off
setlocal EnableExtensions DisableDelayedExpansion

if "%~1"=="" (
  echo Usage: %~nx0 ^<schema.json^> [config.ini] [extra args...] 1>&2
  exit /b 2
)

set "SCRIPT_DIR=%~dp0"
set "SCHEMA_PATH=%~f1"
shift /1

set "CONFIG_ARG="
if "%~1"=="" goto collect_config_args
set "FIRST_ARG=%~1"
if "%FIRST_ARG:~0,1%"=="-" goto collect_config_args
set CONFIG_ARG=-Config="%~f1"
shift /1

:collect_config_args
set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=%EXTRA_ARGS% "%~1"
shift /1
goto collect_args

:run
call "%SCRIPT_DIR%run_commandlet.bat" ValidateConfigRules -Schema="%SCHEMA_PATH%" %CONFIG_ARG% %EXTRA_ARGS%
exit /b %ERRORLEVEL%
