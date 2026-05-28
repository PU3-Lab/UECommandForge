@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%ue_env.bat"
if errorlevel 1 exit /b %ERRORLEVEL%

if "%~1"=="" (
  echo Usage: %~nx0 ^<CommandletName^> [extra args...] 1>&2
  exit /b 2
)

set "COMMANDLET=%~1"
shift /1

set "REPORT_DIR=%REPO_ROOT%\sample\Saved\CodexReports"
if not exist "%REPORT_DIR%" mkdir "%REPORT_DIR%"

for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "Get-Date -AsUTC -Format yyyyMMddTHHmmssZ" 2^>nul`) do set "STAMP=%%I"
if not defined STAMP set "STAMP=%RANDOM%%RANDOM%"

set "OUTPUT_JSON=%REPORT_DIR%\%COMMANDLET%_%STAMP%.json"

set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run_commandlet
set EXTRA_ARGS=!EXTRA_ARGS! "%~1"
shift /1
goto collect_args

:run_commandlet
echo [run_commandlet] %COMMANDLET% 시작 1>&2
"%UNREAL_EDITOR_CMD%" "%PROJECT_FILE%" ^
  -run="%COMMANDLET%" ^
  -Output="%OUTPUT_JSON%" ^
  -unattended -nop4 -nosplash -log -stdout -FullStdOutLogOutput ^
  %EXTRA_ARGS%

set "UE_EXIT=%ERRORLEVEL%"

if not exist "%OUTPUT_JSON%" (
  echo [run_commandlet] Result JSON이 없습니다: %OUTPUT_JSON% 1>&2
  exit /b 5
)

echo %OUTPUT_JSON%
exit /b %UE_EXIT%
