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
if not "%COMMANDLET:&=%"=="%COMMANDLET%" goto unsafe_commandlet
if not "%COMMANDLET:|=%"=="%COMMANDLET%" goto unsafe_commandlet
if not "%COMMANDLET:<=%"=="%COMMANDLET%" goto unsafe_commandlet
if not "%COMMANDLET:>=%"=="%COMMANDLET%" goto unsafe_commandlet
call :reject_unreal_python_arg "%COMMANDLET%"
if errorlevel 1 exit /b %ERRORLEVEL%
shift /1

if not exist "%PROJECT_FILE%" (
  echo [run_commandlet] Project file을 찾을 수 없습니다: %PROJECT_FILE% 1>&2
  exit /b 2
)

for %%I in ("%PROJECT_FILE%") do set "PROJECT_DIR=%%~dpI"
if "!PROJECT_DIR:~-1!"=="\" set "PROJECT_DIR=!PROJECT_DIR:~0,-1!"

if defined UECF_REPORT_DIR (
  set "REPORT_DIR=%UECF_REPORT_DIR%"
) else (
  set "REPORT_DIR=!PROJECT_DIR!\Saved\CodexReports"
)
if not exist "%REPORT_DIR%" mkdir "%REPORT_DIR%"

for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "Get-Date -AsUTC -Format yyyyMMddTHHmmssZ" 2^>nul`) do set "STAMP=%%I"
if not defined STAMP set "STAMP=%RANDOM%%RANDOM%"

set "OUTPUT_JSON=%REPORT_DIR%\%COMMANDLET%_%STAMP%.json"

setlocal DisableDelayedExpansion
set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run_commandlet
set "ARG=%~1"
set "NEXT_ARG=%~2"
set "ARG_NEEDS_VALUE="
set "ARG_IS_COMMA_LIST="
for %%K in (-ApplyBuildCs -Asset -AssetPaths -AssetPolicy -AssetRootPaths -BuildCsPolicy -Config -ConfigRules -DataSchema -DataSource -ExecCmds -Format -Log -Markdown -Plan -Policy -RollbackPlan -RootPaths -Schema -Source -Spec -SpecFile) do (
  if /I "%ARG%"=="%%K" set "ARG_NEEDS_VALUE=1"
)
for %%K in (-AssetPaths -AssetRootPaths -RootPaths) do (
  if /I "%ARG%"=="%%K" set "ARG_IS_COMMA_LIST=1"
)
if not "%NEXT_ARG%"=="" (
  if defined ARG_NEEDS_VALUE (
    if not "%NEXT_ARG:~0,1%"=="-" (
      shift /1
      shift /1
      set "COMBINED_ARG=%ARG%=%NEXT_ARG%"
      if defined ARG_IS_COMMA_LIST goto collect_comma_tail
      goto append_combined_arg
    )
  )
)
set "ARG_KEY="
set "ARG_VALUE="
for /f "tokens=1* delims==" %%A in ("%ARG%") do (
  set "ARG_KEY=%%A"
  set "ARG_VALUE=%%B"
)
set "COMBINED_ARG=%ARG%"
if defined ARG_VALUE (
  for %%K in (-AssetPaths -AssetRootPaths -RootPaths) do (
    if /I "%ARG_KEY%"=="%%K" set "ARG_IS_COMMA_LIST=1"
  )
)
shift /1
if defined ARG_IS_COMMA_LIST goto collect_comma_tail
goto append_combined_arg

:collect_comma_tail
if "%~1"=="" goto append_combined_arg
set "NEXT_ARG=%~1"
if "%NEXT_ARG:~0,1%"=="-" goto append_combined_arg
if not "%NEXT_ARG:~0,5%"=="/Game" if not "%NEXT_ARG:~0,7%"=="/Plugin" if not "%NEXT_ARG:~0,7%"=="/Engine" goto append_combined_arg
set "COMBINED_ARG=%COMBINED_ARG%,%NEXT_ARG%"
shift /1
goto collect_comma_tail

:append_combined_arg
if not "%COMBINED_ARG:&=%"=="%COMBINED_ARG%" goto unsafe_arg
if not "%COMBINED_ARG:|=%"=="%COMBINED_ARG%" goto unsafe_arg
if not "%COMBINED_ARG:<=%"=="%COMBINED_ARG%" goto unsafe_arg
if not "%COMBINED_ARG:>=%"=="%COMBINED_ARG%" goto unsafe_arg
call :reject_unreal_python_arg "%COMBINED_ARG%"
if errorlevel 1 exit /b %ERRORLEVEL%
set "EXTRA_ARGS=%EXTRA_ARGS% "%COMBINED_ARG%""
goto collect_args

:run_commandlet
echo [run_commandlet] %COMMANDLET% 시작 1>&2
if defined UECF_DEBUG_ARGS echo [run_commandlet] extra args:%EXTRA_ARGS% 1>&2
"%UNREAL_EDITOR_CMD%" "%PROJECT_FILE%" ^
  -run="%COMMANDLET%" ^
  -Output="%OUTPUT_JSON%" ^
  %EXTRA_ARGS% ^
  -unattended -nop4 -nosplash -nullrhi -log -stdout -FullStdOutLogOutput

set "UE_EXIT=%ERRORLEVEL%"

if not exist "%OUTPUT_JSON%" (
  echo [run_commandlet] Result JSON이 없습니다: %OUTPUT_JSON% 1>&2
  echo [run_commandlet] Unreal이 크래시했으면 아래 로그를 확인하세요. 1>&2
  echo [run_commandlet]   Project log: %PROJECT_DIR%\Saved\Logs 1>&2
  echo [run_commandlet]   Crash report: %LOCALAPPDATA%\CrashReportClient\Saved\Crashes 1>&2
  echo [run_commandlet]   Project crash report: %PROJECT_DIR%\Saved\Crashes 1>&2
  exit /b 5
)

echo %OUTPUT_JSON%
exit /b %UE_EXIT%

:unsafe_arg
echo [run_commandlet] Unsafe cmd metacharacter in argument. 1>&2
exit /b 2

:unsafe_commandlet
echo [run_commandlet] Unsafe cmd metacharacter in commandlet name. 1>&2
exit /b 2

:reject_unreal_python_arg
set "UECF_ARG=%~1"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -Command "$v=$env:UECF_ARG; $n=$v.Replace([char]34,' '); $n=$n -replace '[;,\s'']+',' '; if ($v -match '(?i)(executepythonscript|executepythoncommand|pythonscriptplugin|import\s+unreal|unreal\.|\.py)' -or $n -match '(?i)(^| )py($| )') { exit 2 }; exit 0" >nul
if errorlevel 2 goto unsafe_python_arg
if errorlevel 1 goto python_guard_failed
exit /b 0

:unsafe_python_arg
echo [run_commandlet] Unreal Python access is not allowed; use UECommandForge commandlets/wrappers instead. 1>&2
exit /b 2

:python_guard_failed
echo [run_commandlet] Failed to inspect commandlet argument for Unreal Python access. 1>&2
exit /b 2
