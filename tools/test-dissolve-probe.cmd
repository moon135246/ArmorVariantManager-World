@echo off
setlocal
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /utf-8 /std:c++latest /W4 /WX /EHsc tools\test_dissolve_probe.cpp /Fo:build\test_dissolve_probe.obj /Fe:build\test_dissolve_probe.exe
if errorlevel 1 exit /b 1
set "FIXTURE=build\dissolve-fixture-%RANDOM%-%RANDOM%"
mkdir "%FIXTURE%" || exit /b 1
build\test_dissolve_probe.exe "%FIXTURE%"
exit /b %errorlevel%
