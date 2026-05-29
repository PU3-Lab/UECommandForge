@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "PROFILE=%~1"
if "%PROFILE%"=="" set "PROFILE=core"
if /i not "%PROFILE%"=="core" if /i not "%PROFILE%"=="jq" if /i not "%PROFILE%"=="cppcheck" if /i not "%PROFILE%"=="clang-tidy" if /i not "%PROFILE%"=="lint" if /i not "%PROFILE%"=="ue-build" if /i not "%PROFILE%"=="all" (
  echo [uecf-bootstrap] unknown dependency profile: %PROFILE% 1>&2
  exit /b 2
)

set "UECF_BOOTSTRAP_PATH=%PATH%"
call :append_known_paths
call :scan_required

if "%MISSING_COUNT%"=="0" goto success

if "%UECF_SKIP_DEP_INSTALL%"=="1" (
  call :print_missing
  echo [uecf-bootstrap] UECF_SKIP_DEP_INSTALL=1 is set; dependency install was skipped. 1>&2
  exit /b 2
)

if "%UECF_AUTO_INSTALL_DEPS%"=="0" (
  call :print_missing
  echo [uecf-bootstrap] UECF_AUTO_INSTALL_DEPS=0 is set; dependency install was disabled. 1>&2
  exit /b 2
)

if not "%INSTALL_IDS%"=="" (
  where winget >nul 2>nul
  if errorlevel 1 (
    echo [uecf-bootstrap] winget not found. Install missing dependencies manually or install App Installer. 1>&2
    call :print_missing
    exit /b 2
  )

  echo [uecf-bootstrap] installing missing dependencies with winget: %INSTALL_IDS%
  for %%P in (%INSTALL_IDS%) do (
    echo [uecf-bootstrap] winget install --id %%P
    winget install --id %%P -e --source winget --disable-interactivity --accept-package-agreements --accept-source-agreements
    if errorlevel 1 (
      echo [uecf-bootstrap] failed to install %%P. 1>&2
      exit /b !ERRORLEVEL!
    )
  )
)

if not "%SPECIAL_INSTALLS%"=="" call :install_special_dependencies

set "UECF_BOOTSTRAP_PATH=%PATH%"
call :append_known_paths
call :scan_required
if not "%MISSING_COUNT%"=="0" (
  call :print_missing
  exit /b 2
)

:success
call :find_existing_file UECF_FOUND_BASH "%ProgramFiles%\Git\bin\bash.exe" "%LocalAppData%\Programs\Git\bin\bash.exe"
if not defined UECF_FOUND_BASH call :find_command bash.exe UECF_FOUND_BASH
if not defined UECF_FOUND_BASH call :find_command bash UECF_FOUND_BASH
endlocal & set "PATH=%UECF_BOOTSTRAP_PATH%" & set "UECF_BASH_EXE=%UECF_FOUND_BASH%" & exit /b 0

:scan_required
set "MISSING_COUNT=0"
set "MISSING_LABELS="
set "INSTALL_IDS="
set "SPECIAL_INSTALLS="
if /i "%PROFILE%"=="core" call :scan_core
if /i "%PROFILE%"=="jq" call :require_jq
if /i "%PROFILE%"=="cppcheck" call :require_cppcheck
if /i "%PROFILE%"=="clang-tidy" call :require_clang_tidy
if /i "%PROFILE%"=="ue-build" (
  call :scan_core
  call :require_netfxsdk
)
if /i "%PROFILE%"=="lint" (
  call :require_clang_tidy
  call :require_cppcheck
)
if /i "%PROFILE%"=="all" (
  call :scan_core
  call :require_netfxsdk
  call :require_clang_tidy
  call :require_cppcheck
)
exit /b 0

:scan_core
call :require_tool bash.exe "Git for Windows" "Git.Git" "%ProgramFiles%\Git\bin\bash.exe" "%LocalAppData%\Programs\Git\bin\bash.exe"
call :require_tool unzip.exe "Git for Windows unzip" "Git.Git" "%ProgramFiles%\Git\usr\bin\unzip.exe" "%LocalAppData%\Programs\Git\usr\bin\unzip.exe"
call :require_jq
exit /b 0

:require_jq
call :require_tool jq.exe "jq" "jqlang.jq" "%ProgramFiles%\jq\jq.exe" "%LocalAppData%\Microsoft\WinGet\Packages\jqlang.jq_Microsoft.Winget.Source_8wekyb3d8bbwe\jq.exe"
exit /b 0

:require_clang_tidy
call :require_tool clang-tidy.exe "LLVM clang-tidy" "LLVM.LLVM" "%ProgramFiles%\LLVM\bin\clang-tidy.exe" "" "%CLANG_TIDY%"
exit /b 0

:require_cppcheck
call :require_tool cppcheck.exe "Cppcheck" "Cppcheck.Cppcheck" "%ProgramFiles%\Cppcheck\cppcheck.exe" "" "%CPPCHECK%"
exit /b 0

:require_netfxsdk
if exist "%ProgramFiles(x86)%\Windows Kits\NETFXSDK" exit /b 0
if exist "%ProgramFiles%\Windows Kits\NETFXSDK" exit /b 0
set /a MISSING_COUNT+=1
set "MISSING_LABELS=!MISSING_LABELS! .NET Framework SDK 4.8 (Visual Studio Installer components)"
set "SPECIAL_INSTALLS=!SPECIAL_INSTALLS! netfxsdk"
exit /b 0

:install_special_dependencies
for %%S in (%SPECIAL_INSTALLS%) do (
  if /i "%%S"=="netfxsdk" call :install_netfxsdk
  if errorlevel 1 exit /b !ERRORLEVEL!
)
exit /b 0

:install_netfxsdk
set "UECF_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "UECF_VS_SETUP=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vs_installer.exe"
if not exist "%UECF_VS_SETUP%" set "UECF_VS_SETUP=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\setup.exe"
set "UECF_VS_INSTALL_PATH="

if not exist "%UECF_VSWHERE%" (
  echo [uecf-bootstrap] vswhere not found; trying winget Developer Pack fallback.
  call :install_netfxsdk_winget
  exit /b !ERRORLEVEL!
)
if not exist "%UECF_VS_SETUP%" (
  echo [uecf-bootstrap] Visual Studio Installer not found; trying winget Developer Pack fallback.
  call :install_netfxsdk_winget
  exit /b !ERRORLEVEL!
)

for /f "usebackq delims=" %%P in (`"%UECF_VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
  set "UECF_VS_INSTALL_PATH=%%P"
)
if not defined UECF_VS_INSTALL_PATH (
  for /f "usebackq delims=" %%P in (`"%UECF_VSWHERE%" -latest -products * -property installationPath`) do (
    set "UECF_VS_INSTALL_PATH=%%P"
  )
)
if not defined UECF_VS_INSTALL_PATH (
  echo [uecf-bootstrap] Visual Studio Build Tools installation not found; trying winget Developer Pack fallback.
  call :install_netfxsdk_winget
  exit /b !ERRORLEVEL!
)

echo [uecf-bootstrap] installing .NET Framework 4.8 SDK components with Visual Studio Installer.
echo [uecf-bootstrap] This may open a UAC prompt or require administrator permission.
"%UECF_VS_SETUP%" modify --installPath "%UECF_VS_INSTALL_PATH%" --add Microsoft.Net.Component.4.8.SDK --add Microsoft.Net.Component.4.8.TargetingPack --quiet --norestart --wait
if errorlevel 1 (
  echo [uecf-bootstrap] Visual Studio Installer component install failed; trying winget Developer Pack fallback.
  call :install_netfxsdk_winget
  exit /b !ERRORLEVEL!
)

if exist "%ProgramFiles(x86)%\Windows Kits\NETFXSDK" exit /b 0
if exist "%ProgramFiles%\Windows Kits\NETFXSDK" exit /b 0
echo [uecf-bootstrap] .NET Framework SDK still not found after installer finished. 1>&2
call :install_netfxsdk_winget
exit /b !ERRORLEVEL!

:install_netfxsdk_winget
where winget >nul 2>nul
if errorlevel 1 (
  echo [uecf-bootstrap] winget not found. Open Visual Studio Installer and add .NET Framework 4.8 SDK and Targeting Pack. 1>&2
  exit /b 2
)
echo [uecf-bootstrap] winget install --id Microsoft.DotNet.Framework.DeveloperPack_4
winget install --id Microsoft.DotNet.Framework.DeveloperPack_4 -e --source winget --silent --accept-package-agreements --accept-source-agreements
if errorlevel 1 (
  echo [uecf-bootstrap] failed to install Microsoft.DotNet.Framework.DeveloperPack_4. 1>&2
  echo [uecf-bootstrap] Open Visual Studio Installer and add: .NET Framework 4.8 SDK, .NET Framework 4.8 Targeting Pack. 1>&2
  exit /b !ERRORLEVEL!
)
if exist "%ProgramFiles(x86)%\Windows Kits\NETFXSDK" exit /b 0
if exist "%ProgramFiles%\Windows Kits\NETFXSDK" exit /b 0
echo [uecf-bootstrap] .NET Framework SDK still not found after winget Developer Pack install. 1>&2
exit /b 2

:require_tool
set "UECF_TOOL=%~1"
set "UECF_LABEL=%~2"
set "UECF_PACKAGE=%~3"
if not "%~6"=="" if exist "%~6" (
  for %%D in ("%~6") do call :append_path "%%~dpD"
  exit /b 0
)
if not "%~4"=="" if exist "%~4" (
  for %%D in ("%~4") do call :append_path "%%~dpD"
  exit /b 0
)
if not "%~5"=="" if exist "%~5" (
  for %%D in ("%~5") do call :append_path "%%~dpD"
  exit /b 0
)
call :find_command "%UECF_TOOL%" UECF_TOOL_PATH
if defined UECF_TOOL_PATH exit /b 0
set /a MISSING_COUNT+=1
set "MISSING_LABELS=!MISSING_LABELS! %UECF_LABEL% (%UECF_PACKAGE%)"
set "INSTALL_IDS=!INSTALL_IDS! %UECF_PACKAGE%"
exit /b 0

:find_command
set "%~2="
for /f "delims=" %%P in ('where %~1 2^>nul') do (
  set "%~2=%%P"
  exit /b 0
)
exit /b 1

:find_existing_file
set "%~1="
if not "%~2"=="" if exist "%~2" (
  set "%~1=%~2"
  exit /b 0
)
if not "%~3"=="" if exist "%~3" (
  set "%~1=%~3"
  exit /b 0
)
exit /b 1

:append_known_paths
call :append_path "%ProgramFiles%\Git\bin"
call :append_path "%ProgramFiles%\Git\usr\bin"
call :append_path "%LocalAppData%\Programs\Git\bin"
call :append_path "%LocalAppData%\Programs\Git\usr\bin"
call :append_path "%ProgramFiles%\jq"
call :append_path "%ProgramFiles%\LLVM\bin"
call :append_path "%ProgramFiles%\Cppcheck"
exit /b 0

:append_path
if "%~1"=="" exit /b 0
if not exist "%~1" exit /b 0
set "UECF_PATH_SEARCH=;!UECF_BOOTSTRAP_PATH!;"
echo(!UECF_PATH_SEARCH! | "%SystemRoot%\System32\findstr.exe" /i /l /c:";%~1;" >nul
if errorlevel 1 set "UECF_BOOTSTRAP_PATH=%~1;%UECF_BOOTSTRAP_PATH%"
set "PATH=%UECF_BOOTSTRAP_PATH%"
exit /b 0

:print_missing
echo [uecf-bootstrap] missing dependencies:%MISSING_LABELS% 1>&2
echo [uecf-bootstrap] manual install commands: 1>&2
for %%P in (%INSTALL_IDS%) do echo   winget install --id %%P -e --source winget 1>&2
for %%S in (%SPECIAL_INSTALLS%) do if /i "%%S"=="netfxsdk" echo   Visual Studio Installer: add .NET Framework 4.8 SDK and .NET Framework 4.8 Targeting Pack 1>&2
exit /b 0
