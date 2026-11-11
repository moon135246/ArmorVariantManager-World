@echo off
setlocal
cd /d "%~dp0.."
if "%~2"=="" exit /b 2
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /std:c++latest /utf-8 /O2 /W4 /WX /EHsc /D NOMINMAX tools\test_dissolve_dx12.cpp /Fo:build\test_dissolve_dx12.obj /Fe:build\test_dissolve_dx12.exe /link d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 1
build\test_dissolve_dx12.exe "%~1" "%~2"
exit /b %errorlevel%
