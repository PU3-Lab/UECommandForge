@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "SOURCE_ROOT=%REPO_ROOT%\sample\Plugins\UECommandForge\Source"
set "MODE=run"

if "%~1"=="--check-tools" set "MODE=check-tools"
if "%~1"=="--cppcheck-only" set "MODE=cppcheck-only"
if "%~1"=="--clang-tidy-only" set "MODE=clang-tidy-only"
if "%~1"=="-h" goto :usage
if "%~1"=="--help" goto :usage
if not "%~1"=="" (
  if "%MODE%"=="run" goto :usage_error
)

call :find_clang_tidy
call :find_cppcheck

if "%MODE%"=="check-tools" (
  echo platform: windows
  if defined CLANG_TIDY_BIN (
    echo clang-tidy: %CLANG_TIDY_BIN%
  ) else (
    echo clang-tidy: not found
  )
  if defined CPPCHECK_BIN (
    echo cppcheck: %CPPCHECK_BIN%
  ) else (
    echo cppcheck: not found
  )
  if not defined CLANG_TIDY_BIN exit /b 1
  if not defined CPPCHECK_BIN exit /b 1
  exit /b 0
)

set "FILE_LIST=%TEMP%\uecommandforge_cpp_files_%RANDOM%%RANDOM%.txt"
if exist "%FILE_LIST%" del "%FILE_LIST%" >nul 2>nul
for /r "%SOURCE_ROOT%" %%F in (*.cpp) do echo %%~fF>>"%FILE_LIST%"

if not "%MODE%"=="clang-tidy-only" (
  if not defined CPPCHECK_BIN (
    echo [lint] cppcheck not found. Install it or set CPPCHECK. 1>&2
    del "%FILE_LIST%" >nul 2>nul
    exit /b 127
  )
  "%CPPCHECK_BIN%" --enable=warning,style,performance,portability --inline-suppr --std=c++20 --suppress=missingIncludeSystem --suppress=unknownMacro --suppress=syntaxError --file-list="%FILE_LIST%"
  if errorlevel 1 (
    del "%FILE_LIST%" >nul 2>nul
    exit /b !ERRORLEVEL!
  )
)

if "%MODE%"=="clang-tidy-only" goto :run_clang_tidy
if "%RUN_CLANG_TIDY%"=="1" goto :run_clang_tidy
goto :finish

:run_clang_tidy
  if not defined CLANG_TIDY_BIN (
    echo [lint] clang-tidy not found. Install LLVM or set CLANG_TIDY. 1>&2
    del "%FILE_LIST%" >nul 2>nul
    exit /b 127
  )
  if not defined COMPILE_COMMANDS_DIR set "COMPILE_COMMANDS_DIR=%REPO_ROOT%"
  if not exist "%COMPILE_COMMANDS_DIR%\compile_commands.json" (
    echo [lint] compile_commands.json not found in %COMPILE_COMMANDS_DIR%; skipping clang-tidy.
    echo [lint] Generate one with UnrealBuildTool or set COMPILE_COMMANDS_DIR.
    del "%FILE_LIST%" >nul 2>nul
    exit /b 0
  )
  if not defined CLANG_TIDY_CHECKS set "CLANG_TIDY_CHECKS=clang-analyzer-*"
  for /f "usebackq delims=" %%F in ("%FILE_LIST%") do (
    "%CLANG_TIDY_BIN%" -checks="%CLANG_TIDY_CHECKS%" -p "%COMPILE_COMMANDS_DIR%" "%%F"
    if errorlevel 1 (
      del "%FILE_LIST%" >nul 2>nul
      exit /b !ERRORLEVEL!
    )
  )

:finish
del "%FILE_LIST%" >nul 2>nul
exit /b 0

:find_clang_tidy
if defined CLANG_TIDY if exist "%CLANG_TIDY%" (
  set "CLANG_TIDY_BIN=%CLANG_TIDY%"
  exit /b 0
)
for %%C in (clang-tidy.exe clang-tidy) do (
  for /f "delims=" %%P in ('where %%C 2^>nul') do (
    set "CLANG_TIDY_BIN=%%P"
    exit /b 0
  )
)
if exist "C:\Program Files\LLVM\bin\clang-tidy.exe" (
  set "CLANG_TIDY_BIN=C:\Program Files\LLVM\bin\clang-tidy.exe"
  exit /b 0
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe" (
  set "CLANG_TIDY_BIN=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
  exit /b 0
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\x64\bin\clang-tidy.exe" (
  set "CLANG_TIDY_BIN=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
  exit /b 0
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\x64\bin\clang-tidy.exe" (
  set "CLANG_TIDY_BIN=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
  exit /b 0
)
exit /b 0

:find_cppcheck
if defined CPPCHECK if exist "%CPPCHECK%" (
  set "CPPCHECK_BIN=%CPPCHECK%"
  exit /b 0
)
for %%C in (cppcheck.exe cppcheck) do (
  for /f "delims=" %%P in ('where %%C 2^>nul') do (
    set "CPPCHECK_BIN=%%P"
    exit /b 0
  )
)
if exist "C:\Program Files\Cppcheck\cppcheck.exe" (
  set "CPPCHECK_BIN=C:\Program Files\Cppcheck\cppcheck.exe"
  exit /b 0
)
exit /b 0

:usage
echo Usage: tools\lint\cpp_static_analysis.bat [--check-tools] [--cppcheck-only] [--clang-tidy-only]
exit /b 0

:usage_error
echo Usage: tools\lint\cpp_static_analysis.bat [--check-tools] [--cppcheck-only] [--clang-tidy-only] 1>&2
exit /b 2
