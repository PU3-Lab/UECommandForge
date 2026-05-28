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

if not exist "%PROJECT_FILE%" (
  echo [run_commandlet] Project file을 찾을 수 없습니다: %PROJECT_FILE% 1>&2
  exit /b 2
)

if defined UECF_REPORT_DIR (
  set "REPORT_DIR=%UECF_REPORT_DIR%"
) else (
  for %%I in ("%PROJECT_FILE%") do set "PROJECT_DIR=%%~dpI"
  if "!PROJECT_DIR:~-1!"=="\" set "PROJECT_DIR=!PROJECT_DIR:~0,-1!"
  set "REPORT_DIR=!PROJECT_DIR!\Saved\CodexReports"
)
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
