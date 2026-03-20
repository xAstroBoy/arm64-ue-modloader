# RE4 VR — Debug Menu Blueprint Analysis
> Generated from PAK extraction + .uasset decompilation

---

## Blueprint Class Hierarchy

```
VR4DebugScreenActor (C++)
  └── DebugMenu_C (Blueprint, pakchunk0)
        ├── ParentWidget → DebugMenuWidget_C
        │     └── DebugVBoxList → DebugVBoxList_C
        │           └── children → DebugOptionWidget_C[]
        └── DebugMenuOptions (DataTable, defines ALL menu entries)
```

---

## DebugMenu_C — Full Function List (52 functions, 61 exports)

### Input Handlers (hooked by our DebugMenuAPI mod)
| # | Function | Size | Description |
|---|----------|------|-------------|
| 1 | `InputActionDoShortcut` | 145B | Shortcut activation |
| 2 | `InputActionSetShortcutBY` | 58B | Assign BY shortcut |
| 3 | `InputActionSetShortcutAX` | 58B | Assign AX shortcut |
| 4 | `InputActionScrollUp` | 145B | **Scroll up** (Pressed bool param) |
| 5 | `InputActionScrollDown` | 145B | **Scroll down** (Pressed bool param) |
| 6 | `InputActionBack` | 145B | **Back** (Pressed bool param) |
| 7 | `InputActionConfirm` | 145B | **Confirm** (Pressed bool param) |
| 8 | `InputActionFavorite` | 145B | Favorite toggle |

### Scroll System
| # | Function | Size | Description |
|---|----------|------|-------------|
| 9 | `TimerExpired` | 58B | Timer callback for scroll repeat |
| 10 | `TriggerScrollUp` | 58B | Single scroll up tick |
| 11 | `ContinueScrollUp` | 58B | Repeat scroll up |
| 12 | `StartScrollUp` | 58B | Begin held scroll up |
| 13 | `ContinueScrollDown` | 58B | Repeat scroll down |
| 14 | `TriggerScrollDown` | 58B | Single scroll down tick |
| 15 | `StartScrollDown` | 58B | Begin held scroll down |

### Core Menu Logic
| # | Function | Size | Description |
|---|----------|------|-------------|
| 0 | `ExecuteUbergraph_DebugMenu` | 19.7KB | Main event graph (BeginPlay dispatcher) |
| 16 | `ReceiveBeginPlay` | 58B | Entry point → calls Initialize |
| 17 | `Initialize` | 1.7KB | Setup: get PC, load data tables, init refs |
| 19 | **`NewMenu`** | **56.4KB** | **Page builder** — switch on ActiveMenu enum, populates options |
| 18 | **`ProcessNewSetting`** | **46.3KB** | **Setting handler** — switch on DebugOptionName, applies change |
| 20 | **`GetOptionIndex`** | **86.1KB** | **Index resolver** — maps option FName → current setting index |
| 24 | **`DoAction`** | **89.2KB** | **Action handler** — executes option-specific actions |
| 21 | `GetActiveOption` | 2.3KB | Returns FName of currently highlighted option |
| 22 | `UpdateOptionHighlight` | 4.4KB | Visual highlight: sets Selection, UpdateLook |
| 23 | `ClearWidgets` | 1.4KB | Removes all option widgets from VBoxList |
| 28 | `CreateActiveOption` | 1.1KB | Creates DebugOptionWidget_C, adds to list |
| 32 | `PreviousMenu` | 1.0KB | Go back one menu level |
| 49 | `HideDebugMenu` | 1.3KB | Close/hide the menu |
| 50 | `OpenDebugMenu` | 469B | Open/show the menu |
| 51 | `IsActive` | 254B | Returns whether menu is currently shown |

### Settings Processors
| # | Function | Size | Description |
|---|----------|------|-------------|
| 27 | `ProcessParticleSetting` | 4.5KB | Particle debug option handler |
| 29 | `ProcessPixelDensitySetting` | 1.1KB | Pixel density slider |
| 33 | `ProcessFogSetting` | 1.1KB | Fog toggle |
| 36 | `ProcessFinishGameDifficulty` | 487B | Difficulty selector |
| 37 | `ProcessCutsceneSetting` | 474B | Cutscene mode toggle |
| 40 | `ProcessNewRaiseWeaponSetting` | 490B | Raise weapon mode |
| 41 | `Get Raise Weapon Setting` | 257B | Current raise weapon state |

### Inventory/Items
| # | Function | Size | Description |
|---|----------|------|-------------|
| 31 | `AddTreasureItems` | 2.0KB | Add treasure items to inventory |
| 35 | `MaxOutPlayerInventory` | 6.8KB | Fill inventory with all items |
| 38 | `RefreshInventory` | 2.4KB | Refresh inventory display |
| 39 | `RefreshKeysTreasures` | 1.2KB | Refresh keys/treasures |

### Unlocks/Achievements
| # | Function | Size | Description |
|---|----------|------|-------------|
| 34 | `DrawAchievements` | 2.5KB | Achievement display |
| 42 | `ToggleGoldenGunsUnlock` | 538B | Golden guns cheat |
| 43 | `DrawMercenariesUnlocks` | 4.2KB | Mercenaries unlock UI |
| 44 | `DrawCharacterUnlocks` | 3.0KB | Character unlock UI |
| 45 | `ProcessUnlocks` | 6.4KB | Unlock toggle handler |
| 46 | `IsUnlockEnabled` | 1.9KB | Check unlock state |
| 47 | `BuildUnlockableTooltips` | 393B | Tooltip builder for unlocks |
| 48 | `ToggleMercRainFX` | 284B | Mercenaries rain effect |

### UI
| # | Function | Size | Description |
|---|----------|------|-------------|
| 25 | `UpdateTooltip` | 571B | Update tooltip text |
| 26 | `BuildTooltip` | 55.9KB | Build full tooltip (switch on option) |
| 30 | `GetRenderingOptionIndex` | 1.6KB | Current rendering setting index |

---

## DebugOptionWidget_C — Functions (7 functions, 122 exports)

| # | Function | Size | Description |
|---|----------|------|-------------|
| 0 | `ExecuteUbergraph` | 132B | Event graph |
| 1 | `OnInitialized` | 58B | Widget init |
| 2 | `Construct` | 58B | Widget construction |
| 3 | **`Setup`** | **1.3KB** | **Reads OptionName → sets TextBlock text, button icons** |
| 4 | **`UpdateLook`** | **5.9KB** | **Updates visual state: highlight, active, type** |
| 5 | `OptionIncremented` | 1.3KB | Handles +1 on option value |
| 6 | `OptionReset` | 1.1KB | Resets to default value |

### Key Widget Components (from name table)
- `OptionNameText` — TextBlock for option label
- `CurrentSettingText` — TextBlock for current value
- `ActionText` — TextBlock for action description
- `Switcher` — WidgetSwitcher (toggles between value/action/link modes)
- `Button0`, `Button1` — VR controller button prompts
- `Button_A_Alt`, `Button_B_Alt`, `Button_X_Alt`, `Button_Y_Alt` — Button textures
- `DebugOptionName` — FName identifier for this option
- `OptionName` — FString display name
- `SettingList` — TArray<FString> of possible values

---

## DebugVBoxList_C — Functions (5 functions)

| # | Function | Size | Description |
|---|----------|------|-------------|
| 0 | **`AddWidget`** | **422B** | Add DebugOptionWidget to list |
| 1 | **`ClearWidgets`** | **249B** | Remove all children |
| 2 | **`UpdateListView`** | **5.0KB** | **Core: scrolling, visibility, clamping** |
| 3 | **`SelectionIncremented`** | **1.5KB** | Move selection down |
| 4 | **`SelectionDecremented`** | **1.6KB** | Move selection up |

### Key Properties
- `MaxVisible` (int32) — max visible items (default 5, we set to 50)
- `FirstVisible` (int32) — scroll position
- `Selection` (int32) — highlighted index
- `ParentVBox` — the actual VerticalBox container

---

## DebugMenuWidget_C — Functions (2 functions)

| # | Function | Size | Description |
|---|----------|------|-------------|
| 0 | `ExecuteUbergraph` | 112B | Event graph |
| 1 | `Construct` | 58B | Widget construction |

### Key Properties
- `DebugVBoxList` — reference to DebugVBoxList_C child
- `TitleText` — TextBlock for menu title

---

## DebugMenuOptions DataTable — Complete Option Map

### Main Menu (DebugMenuType = NewEnumerator5 = 0)
| FName | Category |
|-------|----------|
| `Main_Settings` | → Settings submenu |
| `Main_1pOptionLink` | → 1P Options submenu |
| `Main_Display` | → Display/Rendering submenu |
| `Main_ViewMode` | → View Mode submenu |
| `Main_Enemies` | → Enemies submenu |
| `Main_Inventory` | → Inventory submenu |
| `Main_Cheats` | → Cheats submenu |
| `Main_LevelSelectLink` | → Level/Stage Select |
| `Main_FinishGame` | → Finish Game submenu |
| `Main_Mercenaries` | → Mercenaries submenu |
| `Main_ParticleDebug` | → Particle Debug submenu |
| `Main_Profiling` | → Profiling submenu |
| `Main_PickupItems` | → Pickup Items |
| `Main_Achievements` | → Achievements |
| `Main_Unlocks` | → Unlocks submenu |
| `Main_FavoritesLink` | → Favorites/Shortcuts |
| `Main_ReloadLevel` | Action: Reload current level |
| `Main_Restart` | Action: Restart game |
| `Main_ClearNotifications` | Action: Clear notifications |
| `Main_ClearShortcuts` | Action: Clear shortcuts |
| `Main_ClearTutorials` | Action: Clear tutorials |

### 1P Options
| FName | Description |
|-------|-------------|
| `1pOptions_Movement` | Movement mode |
| `1pOptions_TurnMode` | Turn mode (smooth/snap) |
| `1pOptions_QuickTurn` | Quick turn toggle |
| `1pOptions_Strafing` | Strafe toggle |
| `1pOptions_Handedness` | Left/Right handed |
| `1pOptions_SeatedMode` | Seated play mode |
| `1pOptions_Vignette` | Comfort vignette |
| `1pOptions_CameraBobbing` | Camera bob |
| `1pOptions_ViewBobbing` | View bob |
| `1pOptions_StepSound` | Footstep sounds |
| `1pOptions_LaserSight` | Laser sight toggle |
| `1pOptions_EnableScopeUI` | Scope UI toggle |
| `1pOptions_WeaponSelect` | Weapon select mode |
| `1pOptions_RaiseWeaponMode` | Raise weapon mode |
| `1pOptions_DualWield` | Dual wield toggle |
| `1pOptions_3rdPersonModeToggle` | 3rd person mode |
| `1pOptions_StandingBodyAwareness` | Standing body awareness |
| `1pOptions_SittingBodyAwareness` | Sitting body awareness |
| `1pOptions_WatchWrist` | Wrist watch toggle |
| `1pOptions_EnableOnScreenIndicators` | On-screen indicators |
| `1pOptions_UseNewPropAttachment` | Prop attachment mode |

### Cheats (C_*)
| FName | Description |
|-------|-------------|
| `C_GodMode` | Toggle god mode |
| `C_HealPlayer` | Heal player |
| `C_HurtPlayer` | Hurt player |
| `C_HealAshley` | Heal Ashley |
| `C_HurtAshley` | Hurt Ashley |
| `C_InstaKill` | Instant kill toggle |
| `C_KillAllEnemies` | Kill all enemies |
| `C_AmmoCheat` | Infinite ammo |
| `C_Give10k` | Give 10,000 pesetas |
| `C_SaveDog` | Save the dog |
| `C_MerchantFreeForAll` | Free merchant items |
| `C_AutoKillEnemies` | Auto-kill toggle |
| `C_SetWound` | Set wound state |
| `C_ShootingRangeMode` | Shooting range mode |
| `C_SpawnCutsceneBox` | Spawn cutscene debug box |
| `C_ToggleSt3Timers` | Toggle stage 3 timers |
| `C_ClearActionHistory` | Clear action history |
| `C_Cutscenes` | → Cutscenes submenu |

### Debug Display (DD_*)
| FName | Description |
|-------|-------------|
| `DD_HitBoxes` | Show hitboxes |
| `DD_Skeletons` | Show skeletons |
| `DD_ForceSkeletons` | Force skeleton display |
| `DD_JointNumbers` | Show joint numbers |
| `DD_ModelIDs` | Show model IDs |
| `DD_ModelAtari` | Show collision mesh |
| `DD_Items` | Show items debug |
| `DD_Routes` | Show AI routes |
| `DD_Triggers` | Show triggers |
| `DD_Floors` | Show floors |
| `DD_MovementGeo` | Show movement geometry |
| `DD_EffectGeo` | Show effect geometry |
| `DD_PlayerAttacks` | Show player attacks |
| `DD_Fade` | Fade control |
| `DD_ShowCut` | Show cut debug |
| `DD_ShowEvCut` | Show event cut debug |
| `DD_FlatCutsceneScreen` | Flat cutscene screen |
| `DD_HeadLockedCutscenes` | Head-locked cutscenes |
| `DD_ToggleCutsceneTimecode` | Cutscene timecode |
| `DD_TogglePawnMirror` | Pawn mirror toggle |
| `DD_ToggleAnimations` | Animation toggle |
| `DD_ShadowDebug` | Shadow debug |
| `DD_HighlightImportedLights` | Highlight imported lights |
| `DD_ShowImportedBio4Lights` | Show imported RE4 lights |
| `DD_ShowOverriddenBio4Lights` | Show overridden lights |
| `DD_DebugEnemyBehavior` | Enemy behavior debug |
| `DD_LogHeadsetTracking` | Log headset tracking |
| `DD_HideSpeedIndicator` | Hide speed indicator |
| `DD_HideOneShotAudioDebugText` | Hide one-shot audio text |

### Debug Settings (DS_*)
| FName | Description |
|-------|-------------|
| `DS_WorldScale` | World scale slider |
| `DS_DebugPawn` | Debug pawn toggle |
| `DS_DebugInputHandedness` | Input handedness debug |
| `DS_DifficultyOverwrite` | Override difficulty |
| `DS_DisplayAdaptiveDifficulty` | Show adaptive difficulty |
| `DS_OverwriteAdaptiveDifficulty` | Override adaptive difficulty |
| `DS_FogEnable` | Fog toggle |
| `DS_HideHands` | Hide hands |
| `DS_ShowLeonModel` | Show Leon model |
| `DS_HoldToSelect` | Hold-to-select mode |
| `DS_LaserPointer` | Laser pointer |
| `DS_DisconnectButtons` | Disconnect buttons |
| `DS_DisableFramerateCorrection` | Disable framerate correction |
| `DS_ForceMafiaGear` | Force Mafia outfit |
| `DS_MasterVolume` | Master volume |
| `DS_ShowQteInfo` | Show QTE info |
| `DS_PlatformOverride` | Platform override |
| `DS_ResetAllGameStateFlags` | Reset all game state |
| `DS_ShadowOpacity` | Shadow opacity slider |
| `DS_ShadowScale` | Shadow scale slider |
| `DS_ShadowFeatherPower` | Shadow feather power |
| `DS_OverwriteShadowSettings` | Override shadow settings |
| `DS_TargetOverwriteRegion` | Target override region |
| `DS_TestWeaponAccuracy` | Test weapon accuracy |

### Rendering / View Modes
| FName | Description |
|-------|-------------|
| `Rendering_NoAA` | No antialiasing |
| `Rendering_2xAA` | 2x AA |
| `Rendering_4xAA` | 4x AA |
| `Rendering_PixelDensity` | Pixel density |
| `Rendering_ToggleShadow` | Toggle shadows |
| `Rendering_ToggleStaticShadows` | Toggle static shadows |
| `Rendering_ToggleProjectionShadows` | Toggle projection shadows |
| `Rendering_ToggleRimLighting` | Toggle rim lighting |
| `ViewMode_Lit` | Default lit view |
| `ViewMode_Unlit` | Unlit view |
| `ViewMode_Wireframe` | Wireframe view |
| `ViewMode_LightingOnly` | Lighting only |
| `ViewMode_LightComplexity` | Light complexity |
| `ViewMode_ShaderComplexity` | Shader complexity |
| `ViewMode_ToggleFog` | Toggle fog in view mode |

### Inventory (Inv_*)
| FName | Description |
|-------|-------------|
| `Inv_Fill` | Fill inventory |
| `Inv_Clear` | Clear inventory |
| `Inv_Ammo` | Add ammo |
| `Inv_Guns` | Add guns |
| `Inv_GunMods` | Add gun mods |
| `Inv_Grenades` | Add grenades |
| `Inv_Consumables` | Add consumables |
| `Inv_KeyItems` | Add key items |
| `Inv_AttacheCases` | Add attache cases |
| `Inv_TreasureItems` | Add treasure items |
| `Inv_TresureItems2` | Add treasure items (set 2) |
| `Inv_BottleCaps` | Add bottle caps |

### Mercenaries (Merc_*)
| FName | Description |
|-------|-------------|
| `Merc_BypassLocked` | Bypass locked content |
| `Merc_ClearStats` | Clear stats |
| `Merc_ClearUnlocks` | Clear unlocks |
| `Merc_DebugEnemySpawning` | Debug enemy spawning |
| `Merc_IncreaseStars` | Increase stars |
| `Merc_DecreaseStars` | Decrease stars |
| `Merc_SetStarRanking` | Set star ranking |
| `Merc_StarCount` | Star count |
| `Merc_EndMatch` | End match |
| `Merc_ToggleRainFX` | Toggle rain FX |
| `Merc_ResetNewNotifications` | Reset notifications |
| `Merc_DisplayLifetimeStats` | Display lifetime stats |
| `Toggle_Mercenaries_Timer` | Toggle timer |

### Stage Select (StageSelect_*)
| FName | Description |
|-------|-------------|
| `StageSelect_1st` | Stage 1 |
| `StageSelect_2st` | Stage 2 |
| `StageSelect_3st` | Stage 3 |
| `StageSelect_4st` | Stage 4 |
| `StageSelect_Calibration` | Calibration room |
| `StageSelect_OverrideLevel` | Override level |
| `StageSelect_ClearOverrideLevel` | Clear override |

### Other
| FName | Description |
|-------|-------------|
| `Audio_StatSoundMixes` | Sound mix debug |
| `Audio_StatSoundWaves` | Sound wave debug |
| `Enemies_ToggleEnemyMove` | Toggle enemy movement |
| `EventLogging` | Event logging toggle |
| `FinishGame_Difficulty` | Finish game difficulty |
| `FinishGame_SkipToEndGame` | Skip to end |
| `LogLoadedAssets` | Log loaded assets |
| `LogMissingSoft` | Log missing soft refs |
| `Particles_*` | Particle debug options |
| `Profiling_*` | Performance profiling |
| `Spawn_Ashley` | Spawn Ashley |

---

## DebugMenuType Enum Values

The DataTable references these enum values (NewEnumerator0 through NewEnumerator34, plus 37):
```
NewEnumerator5  = 0  (MAIN MENU — mapped via AppendEnumValue)
NewEnumerator0  = 1  (submenu page)
NewEnumerator1  = 2  (submenu page)
...through...
NewEnumerator34 = 34
NewEnumerator37 = 37 (skips 35, 36)
```

Our MODS page uses `AppendEnumValue("DebugMenuType", "ModsPage", 99)`.

---

## DebugOptionType Enum Values

Controls widget display mode in DebugOptionWidget_C's Switcher:
```
NewEnumerator0 = 0  (Setting with value list — shows CurrentSettingText)
NewEnumerator1 = 1  (Action button — shows ActionText) 
NewEnumerator3 = 3  (Link to submenu — shows arrow/link icon)
NewEnumerator5 = 5  (Toggle — shows on/off)
NewEnumerator7 = 7  (???)
```

---

## Key Insights for Modding

### Menu Page Flow
1. `NewMenu(ActiveMenu, bFromPrevious)` — Switch on ActiveMenu enum:
   - Calls `ClearWidgets()` to remove old options
   - Calls `CreateActiveOption(FString)` for each option in that page
   - Each CreateActiveOption creates a DebugOptionWidget_C and adds it to VBoxList

2. `CreateActiveOption(OptionName)` — Creates widget, sets OptionName, calls Setup():
   - Setup() reads OptionName → looks up in DataTable → sets TextBlocks
   - Sets DebugOptionName (FName), fills SettingList if applicable

3. `InputActionConfirm(Pressed)` → if our MODS page, blocked by pre-hook:
   - Normally calls `DoAction()` or `ProcessNewSetting()` based on OptionType

4. `DoAction()` — 89KB switch on DebugOptionName FName:
   - Each case handles a specific action (heal, kill, spawn, etc.)

5. `ProcessNewSetting()` — 46KB switch on DebugOptionName:
   - Each case applies a setting change (toggle, slider, etc.)

### Why CreateActiveOption + Setup() Overwrites Our Text
When we call `dm:Call("CreateActiveOption", "My Text")`:
1. Creates widget, sets OptionName = "My Text"
2. Calls Setup() internally
3. Setup() tries to look up "My Text" in DebugMenuOptions DataTable
4. Lookup fails → falls through to default → overwrites TextBlocks with blank/default

**Solution**: After CreateActiveOption, we must call SetText() on the TextBlocks directly:
```lua
local opt = widgets[i]  -- the created widget
local nameText = opt:Get("OptionNameText")
nameText:Call("SetText", "My Option Label")
```

### Scroll System
- `MaxVisible` default = 5 (we override to 50)
- `ScrollTimer` used for held-scroll repeat
- `UpdateListView()` handles visibility window based on FirstVisible + MaxVisible
- `SelectionIncremented/Decremented` update Selection and scroll window

---

## Files Generated
```
PAKS_extracted/DebugMenu_dump.txt         — Full DebugMenu_C analysis
PAKS_extracted/DebugOptionWidget_dump.txt  — Full DebugOptionWidget_C analysis
PAKS_extracted/DebugVBoxList_dump.txt      — Full DebugVBoxList_C analysis
PAKS_extracted/DebugMenuWidget_dump.txt    — Full DebugMenuWidget_C analysis
PAKS_extracted/DebugMenuOptions_names.txt  — Full option FName list
```

## Tools
```bash
# Extract PAKs (debug assets only, fast):
python tools/extract_paks.py

# Dump any .uasset:
python tools/dump_uasset.py PAKS_extracted/path/to/Asset.uasset

# Scan directory for Blueprints:
python tools/dump_uasset.py --scan PAKS_extracted/VR4/Content/Blueprints/

# Search for specific names:
python tools/dump_uasset.py --scan PAKS_extracted/ --search "DebugMenu"
```
