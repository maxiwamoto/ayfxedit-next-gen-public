@echo off
setlocal
cd /d "%~dp0"
if not defined SDCC set "SDCC=sdcc"
"%SDCC%" -mz80 --std-c99 -DAYFX_MSX=1 -c ayfx_player.c -o player_msx.rel
if errorlevel 1 exit /b 1
"%SDCC%" -mz80 --std-c99 -c ayfx_player.c -o player_spectrum.rel
if errorlevel 1 exit /b 1
"%SDCC%" -mz80 --std-c99 -c example.c -o example.rel
if errorlevel 1 exit /b 1
"%SDCC%" -mz80 --no-std-crt0 --code-loc 0x8000 --data-loc 0xc000 player_msx.rel example.rel -o ayfx_msx.ihx
if errorlevel 1 exit /b 1
"%SDCC%" -mz80 --no-std-crt0 --code-loc 0x8000 --data-loc 0xc000 player_spectrum.rel example.rel -o ayfx_spectrum.ihx
if errorlevel 1 exit /b 1
