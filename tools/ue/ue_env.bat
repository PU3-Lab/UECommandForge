@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "RESOLVED_REPO_ROOT=%%~fI"
set "INSTALLED_ENV=%RESOLVED_REPO_ROOT%\uecommandforge.env"

if exist "%INSTALLED_ENV%" (
  for /f "usebackq tokens=1,* delims==" %%A in ("%INSTALLED_ENV%") do (
    if /i "%%A"=="UECF_PROJECT_FILE" set "UECF_PROJECT_FILE=%%B"
    if /i "%%A"=="UE_ROOT" set "UE_ROOT=%%B"
    if /i "%%A"=="UNREAL_EDITOR_CMD" set "UNREAL_EDITOR_CMD=%%B"
  )
)

if defined UECF_PROJECT_FILE (
  set "UECF_PROJECT_FILE=!UECF_PROJECT_FILE:/=\!"
  if /i "!UECF_PROJECT_FILE:~0,3!"=="\c\" set "UECF_PROJECT_FILE=C:\!UECF_PROJECT_FILE:~3!"
)
if defined UE_ROOT (
  set "UE_ROOT=!UE_ROOT:/=\!"
  if /i "!UE_ROOT:~0,3!"=="\c\" set "UE_ROOT=C:\!UE_ROOT:~3!"
)
if defined UNREAL_EDITOR_CMD (
  set "UNREAL_EDITOR_CMD=!UNREAL_EDITOR_CMD:/=\!"
  if /i "!UNREAL_EDITOR_CMD:~0,3!"=="\c\" set "UNREAL_EDITOR_CMD=C:\!UNREAL_EDITOR_CMD:~3!"
)

if not defined REPO_ROOT set "REPO_ROOT=%RESOLVED_REPO_ROOT%"
if defined UECF_PROJECT_FILE (
  set "PROJECT_FILE=!UECF_PROJECT_FILE!"
) else (
  if not defined PROJECT_FILE set "PROJECT_FILE=%REPO_ROOT%\sample\UECommandForgeSample.uproject"
)

if not defined UNREAL_EDITOR_CMD (
  if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.7"
  set "UNREAL_EDITOR_CMD=!UE_ROOT!\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)

if not exist "!UNREAL_EDITOR_CMD!" goto missing_editor

endlocal & set "REPO_ROOT=%REPO_ROOT%" & set "PROJECT_FILE=%PROJECT_FILE%" & set "UNREAL_EDITOR_CMD=%UNREAL_EDITOR_CMD%" & set "UE_PLATFORM=Win64"
exit /b 0

:missing_editor
echo [ue_env] UnrealEditor-Cmd.exe? ?? ? ????: !UNREAL_EDITOR_CMD! 1>&2
echo [ue_env] UE_ROOT ?? UNREAL_EDITOR_CMD ????? ??????. 1>&2
endlocal & exit /b 5
