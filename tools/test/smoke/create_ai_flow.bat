@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..\..") do set "REPO_ROOT=%%~fI"
set "SAMPLE_DIR=%REPO_ROOT%\sample"
set "UE_TOOLS=%REPO_ROOT%\tools\ue"
set "SPEC_FILE=%REPO_ROOT%\specs\examples\guard_ai.json"

if not "%~1"=="" (
  for %%I in ("%~1") do set "SPEC_FILE=%%~fI"
  shift /1
)

set "EXTRA_ARGS="
:collect_args
if "%~1"=="" goto run
set EXTRA_ARGS=!EXTRA_ARGS! "%~1"
shift /1
goto collect_args

:run
echo === CreateAIFlow Smoke Test ===
echo Spec: %SPEC_FILE%
echo.

call "%UE_TOOLS%\create_ai_flow.bat" "%SPEC_FILE%" -nullrhi %EXTRA_ARGS%
if errorlevel 1 exit /b %ERRORLEVEL%

set "REPORT="
for /f "delims=" %%F in ('dir /b /o-d "%SAMPLE_DIR%\Saved\CodexReports\CreateAIFlow_*.json" 2^>nul') do (
  if not defined REPORT set "REPORT=%SAMPLE_DIR%\Saved\CodexReports\%%F"
)

if not defined REPORT (
  echo   FAIL: CreateAIFlow report not found 1>&2
  exit /b 1
)

jq -e ".ok == true and .rollback_available == false and (.steps | length == 5) and all(.steps[]; .ok == true) and (.created_assets | length == 3) and (.modified_assets | length == 1) and .validation.compile_status == \"ok\" and .validation.actor_placed == \"ok\"" "%REPORT%" >nul
if errorlevel 1 (
  echo   FAIL: CreateAIFlow report validation failed: %REPORT% 1>&2
  exit /b 1
)

echo   PASS: CreateAIFlow report validation

powershell -NoProfile -Command "$r=Get-Content -Raw $env:REPORT | ConvertFrom-Json; $m=@(); foreach($p in $r.created_assets){$rel=($p -replace '^/Game/','Content/') -replace '/','\'; $f=Join-Path $env:SAMPLE_DIR ($rel + '.uasset'); if(-not (Test-Path -LiteralPath $f)){$m+=$f}}; foreach($p in $r.modified_assets){$rel=($p -replace '^/Game/','Content/') -replace '/','\'; $f=Join-Path $env:SAMPLE_DIR ($rel + '.umap'); if(-not (Test-Path -LiteralPath $f)){$m+=$f}}; if($m.Count -gt 0){$m | ForEach-Object {Write-Error ('Missing asset: ' + $_)}; exit 1}"
if errorlevel 1 (
  echo   FAIL: CreateAIFlow asset file validation failed 1>&2
  exit /b 1
)

echo   PASS: CreateAIFlow asset files exist
echo Report: %REPORT%
exit /b 0
