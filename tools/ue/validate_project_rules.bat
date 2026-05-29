@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" (
  echo Usage: %~nx0 -AssetPolicy=^<policy.json^> [-BuildCsPolicy=^<policy.json^>] [-DataSchema=^<schema.json^> -DataSource=^<source.csv^|json^>] [-ConfigRules=^<schema.json^> [-Config=^<config.ini^>]] [extra args...] 1>&2
  exit /b 2
)

set "SCRIPT_DIR=%~dp0"
set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=!EXTRA_ARGS! "%~1"
shift /1
goto collect_args

:run
call "%SCRIPT_DIR%run_commandlet.bat" PrototypeAutomation %EXTRA_ARGS%
exit /b %ERRORLEVEL%
