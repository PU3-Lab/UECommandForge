@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "PROJECT_FILE=%REPO_ROOT%\sample\UECommandForgeSample.uproject"

if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.7"
set "RUN_UBT=%UE_ROOT%\Engine\Build\BatchFiles\RunUBT.bat"

if not exist "%RUN_UBT%" (
  echo [lint] RunUBT.bat not found: %RUN_UBT% 1>&2
  echo [lint] Set UE_ROOT to your Unreal Engine install path. 1>&2
  exit /b 5
)

if not defined COMPILE_COMMANDS_DIR set "COMPILE_COMMANDS_DIR=%REPO_ROOT%"
if not exist "%COMPILE_COMMANDS_DIR%" mkdir "%COMPILE_COMMANDS_DIR%"

"%RUN_UBT%" ^
  -Mode=GenerateClangDatabase ^
  UnrealEditor Win64 Development ^
  -Project="%PROJECT_FILE%" ^
  -OutputDir="%COMPILE_COMMANDS_DIR%" ^
  -OutputFilename=compile_commands.json ^
  -Include="%REPO_ROOT%\sample\Plugins\UECommandForge\Source\..."
