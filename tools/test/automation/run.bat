@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
call "%SCRIPT_DIR%..\..\ue\ue_env.bat"
if errorlevel 1 exit /b %ERRORLEVEL%

set "REPORT_DIR=%REPO_ROOT%\sample\Saved\AutomationReports"
if not exist "%REPORT_DIR%" mkdir "%REPORT_DIR%"

"%UNREAL_EDITOR_CMD%" "%PROJECT_FILE%" ^
  -unattended -nop4 -nosplash -nullrhi ^
  -ExecCmds="Automation RunTests UECommandForge; Quit" ^
  -ReportExportPath="%REPORT_DIR%" ^
  -log -stdout -FullStdOutLogOutput

if errorlevel 1 exit /b %ERRORLEVEL%

echo.
echo === 자동화 테스트 결과 ===
set "INDEX=%REPORT_DIR%\index.json"
if not exist "%INDEX%" (
  echo   리포트 파일 없음: %INDEX%
  exit /b 1
)

set "SUMMARY_FILE=%TEMP%\uecf_automation_%RANDOM%.txt"
jq -r ". as $r | \"PASS: \($r.succeeded)\", \"FAIL: \($r.failed)\", \"SKIP: \($r.notRun)\"" "%INDEX%" > "%SUMMARY_FILE%"
if errorlevel 1 (
  echo   jq로 자동화 리포트를 파싱할 수 없습니다. 1>&2
  exit /b 1
)

for /f "usebackq delims=" %%A in ("%SUMMARY_FILE%") do echo   %%A
del "%SUMMARY_FILE%" >nul 2>nul
jq -e ".failed == 0" "%INDEX%" >nul
exit /b %ERRORLEVEL%
