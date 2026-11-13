@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "OUT=build"
set "PROBE_FLAG="
set "PROBE_SOURCE="
set "EFFECT_LIB="
set "STYLE_FLAG="
if "%~1"=="" (
  set "STYLE_FLAG=/DWORLDDIFF_SILVER_FLOW"
  set "PROBE_SOURCE=src\SilverGeometry.cpp src\SilverPipeline.cpp src\SilverBindings.cpp"
  set "EFFECT_LIB=d3dcompiler.lib"
)
if /i "%~1"=="silver-beta" (
  set "OUT=build\silver-beta"
  set "PROBE_FLAG=/DWORLDDIFF_DISSOLVE_PROBE /DWORLDDIFF_SILVER_FLOW"
  set "PROBE_SOURCE=src\SilverGeometry.cpp src\SilverPipeline.cpp src\SilverBindings.cpp"
  set "EFFECT_LIB=d3dcompiler.lib"
)
if /i "%~1"=="height-beta" (
  set "OUT=build\height-beta"
  set "PROBE_FLAG=/DWORLDDIFF_HEIGHT_TRIAL /DWORLDDIFF_SILVER_FLOW"
  set "PROBE_SOURCE=src\HeightTrial.cpp src\SilverGeometry.cpp src\SilverPipeline.cpp src\SilverBindings.cpp"
  set "EFFECT_LIB=d3dcompiler.lib"
)
if /i "%~1"=="dissolve-probe" (
  set "OUT=build\dissolve-probe"
  set "PROBE_FLAG=/DWORLDDIFF_DISSOLVE_PROBE /DWORLDDIFF_HANDOFF_TRIAL"
  set "PROBE_SOURCE=src\SceneHandoff.cpp"
)
if /i "%~1"=="dissolve-beta" (
  set "OUT=build\dissolve-beta"
  set "PROBE_FLAG=/DWORLDDIFF_DISSOLVE_PROBE"
)
if not exist "%OUT%" mkdir "%OUT%"
rc /nologo /fo %OUT%\Fonts.res src\Fonts.rc
if errorlevel 1 exit /b 1
cl /nologo /TC /O2 /MT /W3 /c /D ZYDIS_STATIC_BUILD /D ZYCORE_STATIC_BUILD /Ithird_party\safetyhook third_party\safetyhook\Zydis.c /Fo:%OUT%\Zydis.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++latest %PROBE_FLAG% %STYLE_FLAG% /utf-8 /O2 /MT /W4 /EHsc /LD /D WIN32_LEAN_AND_MEAN /D ZYDIS_STATIC_BUILD /D ZYCORE_STATIC_BUILD /Ithird_party\safetyhook /Ithird_party\imgui ^
%PROBE_SOURCE% src\SceneRenderer.cpp src\Runtime.cpp src\VisibilityQueryHook.cpp src\State.cpp src\Profiles.cpp src\ModelCatalog.cpp src\Overlay.cpp src\NativeInput.cpp src\Panel.cpp src\Transform.cpp src\TransformSlots.cpp src\PlayerState.cpp src\Multiplayer.cpp src\Dx12Backend.cpp ^
third_party\safetyhook\safetyhook.cpp ^
third_party\imgui\imgui.cpp third_party\imgui\imgui_draw.cpp third_party\imgui\imgui_tables.cpp third_party\imgui\imgui_widgets.cpp ^
third_party\imgui\imgui_impl_dx11.cpp third_party\imgui\imgui_impl_dx12.cpp third_party\imgui\imgui_impl_win32.cpp ^
%OUT%\Zydis.obj %OUT%\Fonts.res /Fo:%OUT%\ /Fe:%OUT%\WorldDiff.dll ^
/link %EFFECT_LIB% d3d11.lib d3d12.lib dxgi.lib dinput8.lib dxguid.lib user32.lib gdi32.lib imm32.lib dwmapi.lib ole32.lib
if errorlevel 1 exit /b 1
if defined PROBE_FLAG exit /b 0
if not exist "发布\nativePC\plugins" mkdir "发布\nativePC\plugins"
copy /y %OUT%\WorldDiff.dll "发布\nativePC\plugins\WorldDiff.dll" >nul
if errorlevel 1 exit /b 1
echo [success] WorldDiff 1.4.0 in-game plugin built.
