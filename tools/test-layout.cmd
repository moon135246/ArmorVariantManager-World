@echo off
setlocal
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if not exist build mkdir build
rc /nologo /fo build\Fonts.res src\Fonts.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++latest /utf-8 /W4 /EHsc /O2 /MT /Ithird_party\imgui /Itools /D IMGUI_USER_CONFIG=\"test_imgui_config.h\" tools\test_layout.cpp src\State.cpp src\Profiles.cpp src\ModelCatalog.cpp src\Transform.cpp src\TransformSlots.cpp src\PlayerState.cpp src\Multiplayer.cpp third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_widgets.cpp third_party\imgui\imgui_tables.cpp build\Fonts.res /Fo:build\ /Fe:build\test_layout.exe /link user32.lib
exit /b %errorlevel%
