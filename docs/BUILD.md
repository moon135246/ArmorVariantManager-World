# 编译与测试

## 环境

Windows x64；Visual Studio 2022／Build Tools 的“使用 C++ 的桌面开发”和 Windows SDK。脚本使用 vswhere 查找最新可用 MSVC x64 工具链。

在仓库根目录运行：

```bat
build.cmd
```

生成 `build/WorldDiff.dll` 并复制到 `发布/nativePC/plugins/WorldDiff.dll`。编译不会自动安装到真实游戏。项目已包含实际需要的 ImGui、SafetyHook 和 Zydis 源码，不需要下载第三方二进制或运行重复的 Zydis 目录。

## 不需要游戏资源的检查

下列脚本先编译测试程序，成功后再运行相应 EXE。例如：

```bat
call tools\test-hotkeys.cmd
if errorlevel 1 exit /b 1
build\test_hotkeys.exe
```

可同样运行 `test-variant-profiles`、`test-state`、`test-storage`、`test-transforms`、`test-multiplayer`、`test-scene`、`test-mesh-display`、`test-reorder`、`test-hotkey-ui`、`test-layout`、`test-updates`、`test-appearance` 对应脚本和下划线名称的 EXE。各脚本共用 build/*.obj，必须顺序编译，避免同时运行。

ImGui／原生输入检查需要可交互 Windows 桌面。原生输入检查会创建临时测试窗口。离线预览渲染脚本另外需要 Python、NumPy 和 Pillow；普通 DLL 编译不依赖 Python。

界面测试和插件通过 Windows SDK 的资源编译器嵌入同一份 Noto Sans SC 正文与粗体，原生输入检查会核对实际选中的 GDI 字体。

DX12 代理对象兼容性可运行 `tools\test-render-compat.cmd`，脚本编译后自动执行。它使用 WARP 和隐藏测试窗口，不读取游戏文件或个人配置。覆盖范围见 [MHWSS 兼容性说明](MHWSS_COMPATIBILITY.md)。

## 需要自备夹具的检查

- `test_profiles.exe <isolated-root>`：历史跨装备测试，需要符合其模型／材质约束的隔离夹具；省略参数时旧测试会使用开发期默认游戏路径，不建议这样运行。
- `test_mesh_order.exe <isolated-root>`：真实 MOD3／MRL3 顺序与回退检查。
- `test_variant_profiles.exe [isolated-model-root legacy-mei-wdp]`：不传参数运行合成材质映射检查；可选参数使用本地 Mei 夹具核对全部规则。
- `test_storage.exe [isolated-model-root legacy-cfg]`：可选的真实旧数据迁移分支。
- `test_hotkeys.exe [legacy-wdp [chat-fixture-directory]]`：可选旧 WDP 和聊天开闭字段回放；不传参数使用合成数据。
- `test_layout.exe [isolated-model-root]`：可选真实网格页面交互分支。
- `test_scene.exe --pid ...` 为手动只读诊断入口，普通无参数检查不需要游戏进程。

这些真实模型、个人预设、内存快照及原始采集文件不随仓库分发。不要把隔离夹具写到真实游戏的配置目录，也不能将公开可重跑的合成检查数直接当作原交付的全部 1541 项。

## 发布

源码使用 `src/`，发布支持文件使用 `发布/`。安装 ZIP 仅包含 `nativePC/`；项目 MIT 许可证位于 `nativePC/plugins/WorldDiff/LICENSE.txt`。第三方说明位于 `nativePC/plugins/WorldDiff/THIRD_PARTY_NOTICES.md`，许可原文位于同目录的 `licenses/`，排除 build、测试、个人 CFG／WDP、运行日志和备份。`发布/SHA256.txt` 保留原正式交付记录，重新编译后的 DLL 必须重新计算发布校验值。

每个发布标签对应一份源码快照；安装包及其 SHA256 校验文件见同版本的 GitHub Release。
