@echo off
setlocal EnableExtensions DisableDelayedExpansion
set "SCRIPT_DIR=%~dp0"
if "%~1"=="" (
  echo Usage: %~nx0 ^<spec_file^> [extra args...] 1>&2
  exit /b 2
)
for %%I in ("%~1") do set "SPEC_FILE=%%~fI"
if not exist "%SPEC_FILE%" (
  echo 에러: 지정한 Spec 파일이 디렉토리에 존재하지 않습니다: %SPEC_FILE% 1>&2
  exit /b 1
)
shift /1
set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=%EXTRA_ARGS% "%~1"
shift /1
goto collect_args
:run
call "%SCRIPT_DIR%run_commandlet.bat" CreateBlueprint "-SpecFile=%SPEC_FILE%" %EXTRA_ARGS%
exit /b %ERRORLEVEL%
