@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "RESOLVED_REPO_ROOT=%%~fI"

if not defined REPO_ROOT set "REPO_ROOT=%RESOLVED_REPO_ROOT%"
if defined UECF_PROJECT_FILE (
  set "PROJECT_FILE=%UECF_PROJECT_FILE%"
) else (
  if not defined PROJECT_FILE set "PROJECT_FILE=%REPO_ROOT%\sample\UECommandForgeSample.uproject"
)

if not defined UNREAL_EDITOR_CMD (
  if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.7"
  set "UNREAL_EDITOR_CMD=%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)

if not exist "%UNREAL_EDITOR_CMD%" (
  echo [ue_env] UnrealEditor-Cmd.exe를 찾을 수 없습니다: %UNREAL_EDITOR_CMD% 1>&2
  echo [ue_env] UE_ROOT 또는 UNREAL_EDITOR_CMD 환경변수로 재정의하세요. 1>&2
  endlocal & exit /b 5
)

endlocal & (
  set "REPO_ROOT=%REPO_ROOT%"
  set "PROJECT_FILE=%PROJECT_FILE%"
  set "UNREAL_EDITOR_CMD=%UNREAL_EDITOR_CMD%"
  set "UE_PLATFORM=Win64"
)
exit /b 0
