# Third-party notices

## Extract timer layout verification

The three MHW extract timer offsets were independently checked against HunterPie's
`MHWInsectGlaiveStructure` definition:
https://github.com/HunterPie/HunterPie/blob/main/HunterPie.Integrations/Datasources/MonsterHunterWorld/Definitions/MHWInsectGlaiveStructure.cs

WorldDiff uses its own guarded reader and light-count evaluator. No
HunterPie source implementation or binary is included.

The independent Charge Blade and Switch Axe buff timers used for enhanced-state detection and comparison of
live form samples were also checked against HunterPie's definitions:

- https://github.com/HunterPie/HunterPie/blob/main/HunterPie.Integrations/Datasources/MonsterHunterWorld/Definitions/MHWChargeBladeStructure.cs
- https://github.com/HunterPie/HunterPie/blob/main/HunterPie.Integrations/Datasources/MonsterHunterWorld/Definitions/MHWSwitchAxeStructure.cs
- https://github.com/HunterPie/HunterPie/blob/main/HunterPie.Integrations/Datasources/MonsterHunterWorld/Definitions/MHWSwitchAxeSlamStructure.cs
- https://github.com/HunterPie/HunterPie/blob/main/HunterPie/Address/MonsterHunterWorld.421810.map

Bow current charge (0x23B4), Hammer charge (0x235C), and the sword/axe form byte
(0x20E2) were established with our own read-only live captures for Steam build
421810. The buff timers above are not used as a substitute for the form flag.

Power-axe activity is read from the separate abnormality object. The independent
active flag is checked because its timer can retain a positive value while inactive.
All enhanced-state reads use our own guarded reader and were checked against user-
performed live actions; no HunterPie implementation is distributed.

This plugin includes Dear ImGui, SafetyHook, and the Zydis/Zycore disassembler distribution used by SafetyHook.

The original license texts are distributed in `licenses/ImGui-LICENSE.txt`, `licenses/LICENSE.SafetyHook`, and `licenses/LICENSE.Zydis`. Source copies are retained under the workspace's `third_party` directory.

## MHWI-VisualControllerExtended 1.4.0

The read-only player-state pointer chains and condition semantics were derived from the local
`初次尝试/MHWI-VisualControllerExtended-1.4.0` reference. WorldDiff implements its own snapshot
reader and preset-based transformation evaluator; the reference's Group_ID rule backend and
chat functions are not included. The sheathed/drawn wording was checked against motion values
instead of copying the reference README's conflicting prose.

Copyright (c) 2026 MHWI-VisualControllerExtended contributors.

The original MIT license is preserved in `licenses/MHWI-VisualControllerExtended-LICENSE.txt`.
The reference's attribution chain is retained verbatim in
`licenses/MHWI-VisualControllerExtended-THIRD_PARTY_NOTICES.md`.

## mhw-toolkit compatibility data

Player and weapon compatibility information in the reference was adapted, through
WeaponSoundEnhance, from mhw-toolkit and modified by the reference project. WorldDiff further
uses this information for read-only state collection. No toolkit chat or loader implementation
is included.

Copyright 2024 Eigeen.

Licensed under the Apache License, Version 2.0. The complete license and upstream copyright
notice are preserved in `licenses/MHW_TOOLKIT_LICENSE.txt`. See also the retained reference
notices for WeaponSoundEnhance's MIT attribution and upstream source revisions.
# 装备中文名称索引

简体中文防具系列名称读取自用户运行的 Monster Hunter: World（build 421810）。模型编号清单由用户提供；模型与系列 ID 的关系参照本机“怪物猎人世界数据表15.10.00简中_by冰块&Jodo.xlsx”的防具表，并与当前游戏系列文本对应。Monster Hunter 与装备名称属于各自权利人。

