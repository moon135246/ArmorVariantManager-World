@echo off
setlocal
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /TC /O2 /MT /W3 /c /D ZYDIS_STATIC_BUILD /D ZYCORE_STATIC_BUILD /Ithird_party\safetyhook third_party\safetyhook\Zydis.c /Fo:build\Zydis.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++latest /utf-8 /O2 /MT /W4 /WX /EHsc /D WIN32_LEAN_AND_MEAN /D ZYDIS_STATIC_BUILD /D ZYCORE_STATIC_BUILD /Ithird_party\safetyhook tools\test_visibility_compat.cpp src\VisibilityQueryHook.cpp third_party\safetyhook\safetyhook.cpp build\Zydis.obj /Fo:build\ /Fe:build\test_visibility_compat.exe
if errorlevel 1 exit /b 1
build\test_visibility_compat.exe early
if errorlevel 1 exit /b 1
build\test_visibility_compat.exe late
if errorlevel 1 exit /b 1
build\test_visibility_compat.exe basic-late
exit /b %errorlevel%
