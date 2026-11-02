@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
if not exist "发布" mkdir "发布"
if /i "%~1"=="--ui-only" goto :ui
cl /nologo /TC /O2 /MT /W3 /c /D ZYDIS_STATIC_BUILD /D ZYCORE_STATIC_BUILD /Ithird_party\safetyhook third_party\safetyhook\Zydis.c /Fo:build\Zydis.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++latest /utf-8 /O2 /MT /W4 /EHsc /LD /D ZYDIS_STATIC_BUILD /D ZYCORE_STATIC_BUILD /Ithird_party\safetyhook src\Runtime.cpp third_party\safetyhook\safetyhook.cpp build\Zydis.obj /Fo:build\ /Fe:build\WorldDiff.dll /link user32.lib
if errorlevel 1 exit /b 1
copy /y build\WorldDiff.dll "发布\WorldDiff.dll" >nul
if errorlevel 1 exit /b 1
:ui
if not exist src\Manager.cpp exit /b 0
cl /nologo /std:c++17 /utf-8 /O2 /MT /W4 /EHsc src\Manager.cpp src\ModelCatalog.cpp /Fo:build\ /Fe:build\WorldDiffManager.exe /link /SUBSYSTEM:WINDOWS comctl32.lib comdlg32.lib shell32.lib user32.lib gdi32.lib advapi32.lib
if errorlevel 1 exit /b 1
copy /y build\WorldDiffManager.exe "发布\WorldDiffManager.exe" >nul
if errorlevel 1 exit /b 1
echo Build successful.
