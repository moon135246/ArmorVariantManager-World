# Third-party notices

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
