@echo off
setlocal EnableExtensions
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..\specs\profiles\npc_character.json") do set "SPEC_FILE=%%~fI"
call "%SCRIPT_DIR%create_ai_flow.bat" "%SPEC_FILE%" %*
exit /b %ERRORLEVEL%
