@echo off
setlocal
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build mkdir build
cl /nologo /std:c++latest /utf-8 /W4 /WX /EHsc /O2 /MT tools\test_profiles.cpp src\State.cpp src\Profiles.cpp src\ModelCatalog.cpp src\Transform.cpp src\TransformSlots.cpp src\PlayerState.cpp src\Multiplayer.cpp /Fo:build\ /Fe:build\test_profiles.exe /link user32.lib
exit /b %errorlevel%
