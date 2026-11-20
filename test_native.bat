@echo off
setlocal
cd /d "%~dp0"
set "AYFX_VS="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "AYFX_VS=%%i"
if not defined AYFX_VS exit /b 1
call "%AYFX_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not "%errorlevel%"=="0" exit /b 1
if not exist native\build mkdir native\build
python tools\generate_test_fixtures.py
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /O2 /MT /W3 /Fo:native\build\tests.obj /Fe:native\build\tests.exe native\tests.cpp /link /STACK:16777216
if not "%errorlevel%"=="0" exit /b 1
native\build\tests.exe %2
if not "%errorlevel%"=="0" exit /b 1
if not defined AYFX_EXE set "AYFX_EXE=native\dist\ayfxedit-next-gen.exe"
echo RUNNING>native\build\gui-smoke.txt
start "" /wait "%AYFX_EXE%" --smoke-test %*
if not "%errorlevel%"=="0" exit /b 1
type native\build\gui-smoke.txt
set "AYFX_TEST_RESULT="
set /p AYFX_TEST_RESULT=<native\build\gui-smoke.txt
if not "%AYFX_TEST_RESULT%"=="PASS" exit /b 1
exit /b 0
