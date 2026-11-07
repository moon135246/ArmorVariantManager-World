@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist build mkdir build
rc /nologo /fo build\Fonts.res src\Fonts.rc
if errorlevel 1 exit /b 1
cl /nologo /TC /O2 /MT /W3 /c /D ZYDIS_STATIC_BUILD /D ZYCORE_STATIC_BUILD /Ithird_party\safetyhook third_party\safetyhook\Zydis.c /Fo:build\Zydis.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++latest /utf-8 /O2 /MT /W4 /WX /EHsc /D WIN32_LEAN_AND_MEAN /D ZYDIS_STATIC_BUILD /D ZYCORE_STATIC_BUILD /Ithird_party\safetyhook /Ithird_party\imgui ^
tools\test_render_compat.cpp src\State.cpp src\Profiles.cpp src\ModelCatalog.cpp src\NativeInput.cpp src\Transform.cpp src\TransformSlots.cpp src\PlayerState.cpp src\Multiplayer.cpp src\Dx12Backend.cpp ^
third_party\safetyhook\safetyhook.cpp ^
third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp ^
third_party\imgui\imgui_impl_dx11.cpp third_party\imgui\imgui_impl_dx12.cpp third_party\imgui\imgui_impl_win32.cpp ^
build\Zydis.obj build\Fonts.res /Fo:build\ /Fe:build\test_render_compat.exe ^
/link d3d11.lib d3d12.lib dxgi.lib dinput8.lib dxguid.lib user32.lib gdi32.lib imm32.lib dwmapi.lib ole32.lib
if errorlevel 1 exit /b 1
build\test_render_compat.exe
exit /b %errorlevel%
