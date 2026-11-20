@echo off
setlocal
cd /d "%~dp0"
if not defined AYFX_EXE set "AYFX_EXE=%CD%\native\dist\ayfxedit-next-gen.exe"
start "" "%AYFX_EXE%" %*
