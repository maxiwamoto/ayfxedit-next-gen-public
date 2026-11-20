@echo off
setlocal
cd /d "%~dp0"
if not defined SDCC if exist native\build\compiler\sdcc\bin\sdcc.exe set "SDCC=%CD%\native\build\compiler\sdcc\bin\sdcc.exe"
python tools\format_asm.py --check
if errorlevel 1 exit /b 1
call native\build\sdk-test\build_tniasm.bat
if errorlevel 1 exit /b 1
cd /d "%~dp0"
call native\build\sdk-test\build_sdcc.bat
if errorlevel 1 exit /b 1
cd /d "%~dp0"
python tools\test_players.py
if errorlevel 1 exit /b 1
