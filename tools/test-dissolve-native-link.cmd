@echo off
setlocal
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
rc /nologo /fo build\test_dissolve_native_link.res src\Fonts.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++latest /utf-8 /O2 /W4 /WX /EHsc /D NOMINMAX /DWORLDDIFF_TEST_SILVER tools\test_dissolve_native_link.cpp src\SilverPipeline.cpp src\SilverBindings.cpp build\test_dissolve_native_link.res /Fo:build\ /Fe:build\test_dissolve_native_link.exe /link d3d12.lib dxgi.lib d3dcompiler.lib
if errorlevel 1 exit /b 1
if "%~1"=="" exit /b 0
build\test_dissolve_native_link.exe %*
exit /b %errorlevel%
