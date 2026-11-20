@echo off
setlocal
cd /d "%~dp0"
set "AYFX_VS="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "AYFX_VS=%%i"
if not defined AYFX_VS (
 echo Visual Studio C++ Build Tools with the Windows SDK are required.
 exit /b 1
)
call "%AYFX_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist native\build mkdir native\build
if not exist native\dist mkdir native\dist
if not defined AYFX_EXE set "AYFX_EXE=native\dist\ayfxedit-next-gen.exe"
python tools\embed_runtime.py
if errorlevel 1 exit /b 1
rc /nologo /fo native\build\app.res native\app.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /O2 /MT /W3 /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Fo:native\build\main.obj /Fe:"%AYFX_EXE%" native\main.cpp native\build\app.res /link /STACK:8388608 /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:native\app.manifest
if errorlevel 1 exit /b 1
echo Built %AYFX_EXE%
