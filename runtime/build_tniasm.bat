@echo off
setlocal
cd /d "%~dp0"
if not defined TNIASM set "TNIASM=tniasm.exe"
"%TNIASM%" compat.asm example_msx.asm ayfx_msx.bin
if errorlevel 1 exit /b 1
"%TNIASM%" compat.asm example_spectrum.asm ayfx_spectrum.bin
if errorlevel 1 exit /b 1
"%TNIASM%" compat.asm example_small_msx.asm ayfx_small_msx.bin
if errorlevel 1 exit /b 1
"%TNIASM%" compat.asm example_small_spectrum.asm ayfx_small_spectrum.bin
if errorlevel 1 exit /b 1
