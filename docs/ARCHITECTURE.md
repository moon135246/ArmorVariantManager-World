# 模块说明

| 模块 | 职责 |
|---|---|
| Runtime / RenderPolicy | 绘制目标校验、按实例应用显隐、发布只读渲染策略 |
| Overlay / Dx12Backend / NativeInput | 游戏内界面、图形后端、原生中文输入与按键采样 |
| Panel / TransformPanel / HotkeyPanel / ReorderList | 面板、变身、预设快捷键和拖动交互 |
| State / Storage / Profiles | 分组预设、当前外观跟随、文件迁移与备份、模板映射 |
| ModelCatalog | MOD3／MRL3 结构读取、材质及网格身份 |
| PlayerState | 本地角色、存档预览、玩家容器、状态及文字输入识别 |
| Multiplayer | 队友实例生命周期、只读预设匹配和独立条件计算 |
| Transform / TransformSlots | 条件求值及临时预设接管 |
| ArmorNames / ArmorNamesData | 装备编号与中文名称索引 |
| UpdateNotes | 手动更新日志的有界读取 |

本地面板始终跟随本地主角；队友不会成为编辑与保存目标。角色身份不能由玩家槽位推断。共享模型资源的角色仍使用独立实例显隐策略。

快捷键方案属于当前装备 WDP，变身与快捷键总开关保存在 Settings，二者互斥。Settings 122 可读 117／121；WDP 122 可读历史支持格式。仅安装不提前重写原配置，相关保存才升级格式，回退需要使用备份。

内存偏移和结构依赖已验证的游戏版本，更新游戏后需要重新核对。公开源码没有附带可独立替代游戏或 MOD 的资源。
