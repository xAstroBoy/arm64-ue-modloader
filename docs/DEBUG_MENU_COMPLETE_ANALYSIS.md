# RE4 VR Debug Menu — Complete Reverse Engineering

> **RESEARCH DOCUMENT** — Complete mapping of all menu pages, items, Blueprint handlers,
> and console commands from decompiled `DebugMenu_C` Blueprint and `DebugMenuOptions` DataTable.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Enum Reference](#enum-reference)
3. [How Items Are Handled (Flow)](#how-items-are-handled)
4. [Page-by-Page Analysis](#page-by-page-analysis)
5. [Dynamic Pages (Enum-Generated)](#dynamic-pages)
6. [Cross-Reference: Function → Items](#cross-reference)

---

## Architecture Overview

### Source Files Analyzed
| File | Lines | Description |
|------|-------|-------------|
| `DebugMenu.txt` | 7748 | Decompiled DebugMenu_C Blueprint (52 functions) |
| `DebugMenuOptions.json` | 20307 | DataTable with 235 rows defining static menu items |
| `DebugMenuType.json` | 1643 | UEnum defining 36 menu page types |

### Key Functions
| Function | Lines | Bytecode Expressions | Purpose |
|----------|-------|---------------------|---------|
| `DoAction` | 4099–5750 | 1609 | Handles **confirm/select** on Setting/Action items |
| `ProcessNewSetting` | 677–1463 | 712 | Handles **value changed** on Setting/Toggle/Action items |
| `NewMenu` | 1464–2382 | ~900 | Populates items when navigating to a page |
| `InputActionConfirm` | — | — | Calls DoAction for non-Link/non-Setting items, calls ProcessNewSetting for settings |

### DebugOptionType Enum
| Value | Name | Behavior |
|-------|------|----------|
| 0 | **Setting** | Has OptionList. Cycles through list. Triggers `ProcessNewSetting` on change, `DoAction` on confirm |
| 1 | **Link** | Navigates to another page via `MenuLink` field. No DoAction/ProcessNewSetting |
| 3 | **Toggle** | Bool toggle (On/Off from `BoolStrings`). Triggers `ProcessNewSetting` |
| 5 | **Action** | Has OptionList. Cycles through list. Triggers `ProcessNewSetting` AND `DoAction` |
| 7 | **ConsoleCommand** | Executes `ConsoleCommand` field directly via `ExecuteConsoleCommand()`. NO Blueprint logic needed |

### Item Sources
- **DataTable items**: 235 rows in `DebugMenuOptions` DataTable, loaded via `GetDataTableRowFromName`
- **Dynamic items**: Generated at runtime in `NewMenu()` by iterating game enums (EGunItem, EKeyItem, etc.)
- **Favorites items**: Loaded from `LocalDebugSettings.DebugFavorites` array (user-pinned items)

---

## Enum Reference

### DebugMenuType (Page IDs)
| Value | Enumerator | Display Name | DataTable Items | Dynamic Items |
|-------|-----------|--------------|:---------------:|:-------------:|
| 0 | NewEnumerator5 | None (Root) | 1 | — |
| 1 | NewEnumerator0 | Debug Menu | 24 | — |
| 2 | NewEnumerator1 | Load Level: Select Stage | 7 | — |
| 3 | NewEnumerator2 | Load Level: Stage 1 | — | DebugLevelList filtered |
| 4 | NewEnumerator3 | Load Level: Stage 2 | — | DebugLevelList filtered |
| 5 | NewEnumerator4 | Load Level: Stage 3 | — | DebugLevelList filtered |
| 6 | NewEnumerator6 | First Person Options | 21 | — |
| 7 | NewEnumerator7 | Debug Display | 26 | — |
| 8 | NewEnumerator8 | Debug Settings | 28 | — |
| 9 | NewEnumerator9 | Cheats | 18 | — |
| 10 | NewEnumerator10 | Inventory | 12 | — |
| 11 | NewEnumerator11 | Pickup Item | 1 | EItemId (256 or QuickMenu subset) |
| 12 | NewEnumerator12 | Particle Debug | 6 | — |
| 13 | NewEnumerator19 | Rendering | 8 | — |
| 14 | NewEnumerator13 | Inventory - Guns | — | EGunItem (23 entries) |
| 15 | NewEnumerator14 | Inventory - Gun Mods | — | EGunModItem (7 entries) |
| 16 | NewEnumerator15 | Inventory - Grenades | — | EGrenadeItem (7 entries) |
| 17 | NewEnumerator16 | Inventory - Ammo | — | EAmmoItem (8 entries) |
| 18 | NewEnumerator17 | Inventory - Consumables | — | EConsumableItem (16 entries) |
| 19 | NewEnumerator18 | Inventory - Attache Cases | — | EAttacheCaseItem (5 entries) |
| 20 | NewEnumerator20 | Inventory - Key Items | — | EKeyItem (27 entries) |
| 21 | NewEnumerator23 | Inventory - Treasure Items | — | AddTreasureItems(0, 25) |
| 22 | NewEnumerator24 | Inventory - Treasure Items 2 | — | AddTreasureItems(26, 37) |
| 23 | NewEnumerator25 | Inventory - Bottle Caps | — | EBottleCapItem (25 entries) |
| 24 | NewEnumerator21 | Profiling | 10 | — |
| 25 | NewEnumerator22 | ViewMode | 7 | — |
| 26 | NewEnumerator26 | Achievements | — | EAchievementId (12 entries) + "ClearAllAchievements" |
| 27 | NewEnumerator27 | Finish Game | 2 | — |
| 28 | NewEnumerator28 | Cheats - Cutscenes | 38 | — |
| 29 | NewEnumerator29 | Favorites | — | DebugFavorites array |
| 30 | NewEnumerator30 | Audio | 8 | — |
| 31 | NewEnumerator31 | Debug Fade | 3 | — |
| 32 | NewEnumerator32 | Load Level: Mercenaries | — | DebugLevelList filtered |
| 33 | NewEnumerator33 | Mercenaries | 14 | — |
| 34 | NewEnumerator34 | Unlockables | — | Dynamic: NG+, CharacterUnlocks, MercUnlocks, Costume |
| 35 | NewEnumerator37 | Enemies | 1 | — |

### MaxVisible Settings (from NewMenu switch)
| Page | MaxVisible | Notes |
|------|-----------|-------|
| 3 (Stage 1) | 8 | Level select |
| 4 (Stage 2) | 10 | Level select |
| 5 (Stage 3) | 8 | Level select |
| 6 (1P Options) | 10 | — |
| 32 (Mercenaries levels) | 4 | — |
| All others | 0 | 0 = show all / no scroll limit |

> **Note**: Page 28 (Cutscenes) explicitly sets `MaxVisible = 20` in its dedicated NewMenu block.

---

## How Items Are Handled

### Flow for Type 0 (Setting) / Type 5 (Action)
```
User selects item → InputActionConfirm()
  → If OptionType is Setting/Action with OptionList:
      CurrentIndex cycles through OptionList
      NewSetting = OptionList[CurrentIndex]
      → ProcessNewSetting(NewSetting) is called
  → Then DoAction() is called
```

### Flow for Type 3 (Toggle)
```
User selects item → InputActionConfirm()
  → CurrentIndex toggles 0↔1
  → NewSetting = BoolStrings[CurrentIndex]  ("Off"/"On")
  → ProcessNewSetting(NewSetting) is called
```

### Flow for Type 1 (Link)
```
User selects item → InputActionConfirm()
  → self.ActiveMenu = MenuLink value
  → NewMenu() is called to populate the target page
  (No DoAction or ProcessNewSetting)
```

### Flow for Type 7 (ConsoleCommand)
```
User selects item → InputActionConfirm()
  → ExecuteConsoleCommand(ConsoleCommand field)
  (No DoAction or ProcessNewSetting — purely engine-level)
```

---

## Page-by-Page Analysis

---

### Page 0 — "None (Root)"
**Enum value**: 0 · **MaxVisible**: 0

| Row Key | Option Name | Type | Details |
|---------|-------------|------|---------|
| Back | Back | Link | → Page 0 (self-link, effectively "close menu") |

> This is the default/closed state. The "Back" link loops to itself.

---

### Page 1 — "Debug Menu" (Main Page)
**Enum value**: 1 · **MaxVisible**: 0

#### DataTable Items (24)
| Row Key | Option Name | Type | Target/Details |
|---------|-------------|------|----------------|
| Main_FavoritesLink | Favorites | Link | → Page 29 |
| Main_1pOptionLink | 1st Person Options | Link | → Page 6 |
| Main_LevelSelectLink | Level Select | Link | → Page 2 |
| Main_Display | Debug Display | Link | → Page 7 |
| Main_Settings | Debug Settings | Link | → Page 8 |
| Main_Cheats | Cheats | Link | → Page 9 |
| Main_Inventory | Inventory | Link | → Page 10 |
| Main_PickupItems | Pickup Items | Link | → Page 11 |
| Main_ReloadLevel | Reload Level | Setting | |
| Main_Restart | Restart | Setting | |
| Main_ParticleDebug | Particle Debug | Link | → Page 12 |
| Main_Profiling | Profiling | Link | → Page 24 |
| Main_ViewMode | View Mode | Link | → Page 25 |
| Main_Achievements | Achievements | Link | → Page 26 |
| Spawn_Ashley | Spawn Ashley | Setting | |
| Main_ClearTutorials | Clear Tutorial Progress | Setting | |
| Main_ClearNotifications | Clear Notifications | Setting | |
| Main_ClearShortcuts | Clear Shortcuts | Setting | |
| Rendering | Rendering | Link | → Page 13 |
| Audio | Audio | Link | → Page 30 |
| Main_FinishGame | Finish Game | Link | → Page 27 |
| Main_Mercenaries | Mercenaries | Link | → Page 33 |
| Main_Unlocks | Unlockables | Link | → Page 34 |
| Main_Enemies | Enemies | Link | → Page 35 |

#### DoAction Handlers (case 1)
| OptionName Match | Action |
|-----------------|--------|
| `"Reload Level"` | `ReloadLevel()` |
| `"Restart"` | `OpenLevel("Lobby")` |
| `"Spawn Ashley"` | `SpawnAshley()` |
| `"Clear Tutorial Progress"` | `ClearTutorialProgress()` |
| `"Clear Notifications"` | `ClearNotifications()` |
| `"Clear Shortcuts"` | `ClearShortcuts()` |

> **ProcessNewSetting**: No case for page 1 — settings on this page are action-only (no cycling values).

---

### Page 2 — "Load Level: Select Stage"
**Enum value**: 2 · **MaxVisible**: 0

#### DataTable Items (7)
| Row Key | Option Name | Type | Target/Details |
|---------|-------------|------|----------------|
| StageSelect_1st | Stage 1 | Link | → Page 3 |
| StageSelect_2st | Stage 2 | Link | → Page 4 |
| StageSelect_3st | Stage 3 | Link | → Page 5 |
| StageSelect_4st | Stage Merc | Link | → Page 32 |
| StageSelect_OverrideLevel | Override Level | Action | |
| StageSelect_ClearOverrideLevel | Clear Override Level | Setting | |
| StageSelect_Calibration | Calibration | Setting | |

#### DoAction Handlers (case 2)
| OptionName Match | Action |
|-----------------|--------|
| `"Override Level"` | Sets `self.Override = True`, then calls `ProcessNewSetting()` |
| `"Clear Override Level"` | Sets `self.Override = False`, then calls `ProcessNewSetting()` |
| `"Calibration"` | `OpenLevel("CalibrationRoom")` |

#### ProcessNewSetting Handlers (case 2)
| OptionName Match | Property Set |
|-----------------|-------------|
| `"Override Level"` | `LocalDebugSettings.DebugStartLevel = NewSetting` |

---

### Pages 3, 4, 5, 32 — Level Select Sub-Pages
**Enum values**: 3, 4, 5, 32 · **MaxVisible**: 8, 10, 8, 4

These pages are **dynamically generated** in `NewMenu()` by loading from the `DebugLevelList` DataTable. Items are level identifiers (e.g., "r100", "r101", etc.) filtered by stage.

**DoAction**: Selecting a level calls `OpenLevel(selected_level_name)`.

**No DataTable items** — all items are generated at runtime.

**RenderTranslation**: Pages 3, 4 get `(280, 0)` offset. Page 32 gets `(-200, -200)`.

---

### Page 6 — "First Person Options"
**Enum value**: 6 · **MaxVisible**: 10

#### DataTable Items (21)
| Row Key | Option Name | Type | Options |
|---------|-------------|------|---------|
| 1pOptions_TurnMode | Turn Mode | Action | 15°, 30°, 45°, 90°, Smooth |
| 1pOptions_QuickTurn | Quick Turn | Toggle | |
| 1pOptions_ViewBobbing | View Bobbing | Toggle | |
| 1pOptions_StepSound | Step Sound | Toggle | |
| 1pOptions_SeatedMode | Seated Mode | Toggle | |
| 1pOptions_Vignette | Vignette | Action | None, Light, Medium, Strong |
| 1pOptions_Handedness | Handedness | Action | Left, Right |
| 1pOptions_Strafing | Strafing | Toggle | |
| 1pOptions_LaserSight | Laser Sight | Toggle | |
| 1pOptions_EnableScopeUI | Enable Scope UI | Toggle | |
| 1pOptions_EnableOnScreenIndicators | Enable On Screen Indicators | Toggle | |
| 1pOptions_3rdPersonModeToggle | 3rd Person Mode Toggle | Toggle | |
| 1pOptions_CameraBobbing | Camera Bobbing | Toggle | |
| 1pOptions_WatchWrist | Watch Wrist | Action | Left, Right |
| 1pOptions_WeaponSelect | Weapon Select | Action | Body, Quick |
| 1pOptions_Movement | Movement | Action | Continuous, Teleport, Hybrid |
| 1pOptions_DualWield | Dual Wield | Toggle | |
| 1pOptions_SittingBodyAwareness | Seated Body Awareness | Action | Simple, Smoothed Simple, Hybrid, Complex |
| 1pOptions_StandingBodyAwareness | Standing Body Awareness | Action | Simple, Smoothed Simple, Hybrid, Complex |
| 1pOptions_UseNewPropAttachment | Use New Prop Attachment | Toggle | |
| 1pOptions_RaiseWeaponMode | Raise Weapon On Collision | Action | FullRaise, Nothing, PartialRaise |

#### DoAction: No case 6 handler (all items are Toggle/Action → handled by ProcessNewSetting only)

#### ProcessNewSetting Handlers (case 6)
| OptionName Match | Property / Function |
|-----------------|---------------------|
| `"Turn Mode"` | `LocalPlayerSettings.AvatarTurnMode` = switch {0→15, 1→30, 2→45, 3→90, 4→0(Smooth)} |
| `"Quick Turn"` | `LocalPlayerSettings.EnableQuickTurn` = bool |
| `"View Bobbing"` | `LocalPlayerSettings.EnableCameraBobbing` = bool |
| `"Step Sound"` | `LocalPlayerSettings.EnableStepSound` = bool |
| `"Seated Mode"` | `LocalPlayerSettings.EnableSeatedMode` = bool |
| `"Vignette"` | `LocalPlayerSettings.MovementVignetteStrength` = index (0=None,1=Light,2=Med,3=Strong) |
| `"Handedness"` | `LocalPlayerSettings.Handedness` = index (0=Left, 1=Right) |
| `"Strafing"` | `LocalPlayerSettings.EnableStrafing` = bool |
| `"Laser Sight"` | `LocalPlayerSettings.EnableLaserSight` = bool |
| `"Enable Scope UI"` | `LocalPlayerSettings.EnableScopeUI` = bool |
| `"Enable On Screen Indicators"` | `LocalPlayerSettings.EnableOnScreenIndicators` = bool |
| `"3rd Person Mode Toggle"` | `LocalPlayerSettings.Enable3rdPersonModeToggle` = bool |
| `"Camera Bobbing"` | `LocalPlayerSettings.EnableCameraBobbing` = bool |
| `"Watch Wrist"` | `LocalPlayerSettings.WatchHand` = index + `UpdateArm()` call |
| `"Weapon Select"` | `LocalPlayerSettings.WeaponSelectMode` = index (0=Body, 1=Quick) |
| `"Movement"` | `LocalPlayerSettings.MovementMode` = index (0=Continuous, 1=Teleport, 2=Hybrid) |
| `"Dual Wield"` | `LocalPlayerSettings.EnableDualWielding` = bool |
| `"Seated Body Awareness"` | `LocalPlayerSettings.SittingBodyAwareness` = index (0–3) |
| `"Standing Body Awareness"` | `LocalPlayerSettings.StandingBodyAwareness` = index (0–3) |
| `"Use New Prop Attachment"` | `LocalPlayerSettings.UseNewPropAttachment` = bool |
| `"Raise Weapon On Collision"` | `ProcessNewRaiseWeaponSetting()` (delegated function) |

---

### Page 7 — "Debug Display"
**Enum value**: 7 · **MaxVisible**: 0

#### DataTable Items (26)
| Row Key | Option Name | Type | Details |
|---------|-------------|------|---------|
| DD_Triggers | Triggers | Toggle | |
| DD_Floors | Floors | Toggle | |
| DD_MovementGeo | Movement Geo | Toggle | |
| DD_EffectGeo | Effect Geo | Toggle | |
| DD_Routes | Routes | Toggle | |
| DD_Skeletons | Skeletons | Toggle | |
| DD_ForceSkeletons | Force Skeletons | Toggle | |
| DD_JointNumbers | Joint Numbers | Toggle | |
| DD_ModelIDs | Model IDs | Toggle | |
| DD_HitBoxes | Hit Boxes | Toggle | |
| DD_ModelAtari | Model Atari | Toggle | |
| DD_PlayerAttacks | Player Attacks | Toggle | |
| DD_Items | Items | Toggle | |
| DD_HeadLockedCutscenes | Head-Locked Cutscenes | Toggle | |
| DD_LogHeadsetTracking | Log Headset Tracking | Action | Off, Show Relative, Show World |
| DD_ToggleAnimations | ToggleAnimations | ConsoleCmd | `vr4ToggleAnimDisplay` |
| DD_TogglePawnMirror | TogglePawnMirror | ConsoleCmd | `vr4TogglePawnMirror` |
| DD_ShowCut | Show Room Cut Info | Toggle | |
| DD_ShowEvCut | Show Event Cut Info | Toggle | |
| DD_ShowImportedBio4Lights | Show Default Imported Bio4 Lights | Toggle | |
| DD_ShowOverriddenBio4Lights | Show Overridden Bio4 Light Actors | Toggle | |
| DD_HighlightImportedLights | Highlight Default Imported Bio4 Lights | Action | None, Spot, Point, All |
| DD_ToggleCutsceneTimecode | Toggle Cutscene Timecode | ConsoleCmd | `vr4ToggleCutsceneTimecode` |
| DD_HideSpeedIndicator | Hide Speed Indicator | Toggle | |
| DD_DebugEnemyBehavior | Debug Enemy Behavior | Toggle | |
| DD_ShadowDebug | Shadow Debug | ConsoleCmd | `vr4ShadowDebug` |

#### DoAction: No case 7 (toggles/actions handled by ProcessNewSetting; ConsoleCommands bypass Blueprint)

#### ProcessNewSetting Handlers (case 7)
Iterates all `Bio4` actors in the scene. For each Bio4 actor, applies the toggled setting:

| OptionName Match | Property / Target |
|-----------------|-------------------|
| `"Triggers"` | `LocalDebugSettings.DebugTriggers` = bool |
| `"Floors"` | `LocalDebugSettings.DebugFloors` = bool |
| `"Movement Geo"` | `LocalDebugSettings.DebugMovementGeo` = bool |
| `"Effect Geo"` | `LocalDebugSettings.DebugEffectGeo` = bool |
| `"Routes"` | `LocalDebugSettings.DebugRoutes` = bool |
| `"Skeletons"` | `LocalDebugSettings.DebugSkeletons` = bool |
| `"Force Skeletons"` | `LocalDebugSettings.DebugForceSkeletons` = bool |
| `"Joint Numbers"` | `LocalDebugSettings.DebugJointNumbers` = bool |
| `"Model IDs"` | `LocalDebugSettings.DebugModelIds` = bool |
| `"Hit Boxes"` | `LocalDebugSettings.DebugHitBoxes` = bool |
| `"Model Atari"` | `LocalDebugSettings.DebugModelAtari` = bool |
| `"Player Attacks"` | `LocalDebugSettings.DebugPlayerAttacks` = bool |
| `"Items"` | `LocalDebugSettings.DebugItems` = bool |
| `"Head-Locked Cutscenes"` | `GameInstance.HeadLockedCutscenes` = bool |
| `"Log Headset Tracking"` | `LocalDebugSettings.LogHeadsetTracking` = index |
| `"Show Room Cut Info"` | `LocalDebugSettings.ShowRoomCutInfo` = bool |
| `"Show Event Cut Info"` | `LocalDebugSettings.ShowEvCutInfo` = bool |
| `"Flat Cutscene Screen"` | `GameInstance.UseFlatCutsceneScreen` = bool |
| `"Show Default Imported Bio4 Lights"` | `LocalDebugSettings.ShowImportedBio4Lights` = bool |
| `"Show Overridden Bio4 Light Actors"` | `LocalDebugSettings.ShowOverriddenBio4Lights` = bool |
| `"Highlight Default Imported Bio4 Lights"` | `LocalDebugSettings.ObviousLightTypes` = index (0=None,1=Spot,2=Point,3=All) |
| `"Debug Enemy Behavior"` | `LocalDebugSettings.DebugEnemyBehavior` = bool |
| `"Hide Speed Indicator"` | `PlayerController.HideSpeedIndicator` = bool |

> **Note**: "Flat Cutscene Screen" (DD_FlatCutsceneScreen) is physically in Page 8's DataTable but its ProcessNewSetting handler is in case 7.

---

### Page 8 — "Debug Settings"
**Enum value**: 8 · **MaxVisible**: 0

#### DataTable Items (28)
| Row Key | Option Name | Type | Options |
|---------|-------------|------|---------|
| DS_DebugPawn | Debug Pawn | Toggle | |
| DS_WorldScale | World Scale | Action | 0.5, 1, 2, 4 |
| DS_DisableFramerateCorrection | Disable Framerate Correction | Toggle | |
| DS_MasterVolume | Master Volume | Action | 0, 25, 50, 75, 100 |
| DS_HideHands | Hide Hands | Toggle | |
| DS_LaserPointer | Laser Pointer | Toggle | |
| DS_ShowLeonModel | Show Leon Model | Toggle | |
| DS_HoldToSelect | Hold To Select | Toggle | |
| DS_DisplayAdaptiveDifficulty | Display Adaptive Difficulty | Toggle | |
| DS_OverwriteAdaptiveDifficulty | Overwrite Adaptive Difficulty | Toggle | |
| DS_DifficultyOverwrite | Difficulty Overwrite Level | Action | Level 0–10 |
| DS_TargetOverwriteRegion | Target Override Region | Action | None, JP, US, GB, DE, FR, ES, IT, KR |
| DS_PlatformOverride | Platform Override | Action | None, PC, Quest, Quest2 |
| DS_OverwriteShadowSettings | Overwrite Shadow Settings | Toggle | |
| DS_ShadowFeatherPower | Shadow Feather Power | Action | 1–10 |
| DS_ShadowOpacity | Shadow Opacity | Action | 0.1–1.0 |
| DS_ShadowScale | Shadow Scale | Action | 0.5–1.0 |
| DS_FogEnable | Fog Enable | Toggle | |
| DS_DisconnectButtons | Disconnect Buttons | Toggle | |
| DS_ShowQteInfo | Show Qte Info | Toggle | |
| DS_DebugInputHandedness | Debug Input Handedness | Action | Left, Right |
| DS_ResetAllGameStateFlags | Reset All Game State Flags | Setting | |
| DS_TestWeaponAccuracy | Test Weapon Accuracy | Toggle | |
| DS_ForceMafiaGear | Force Mafia Gear | Toggle | |
| DD_FlatCutsceneScreen | Flat Cutscene Screen | Toggle | |
| DD_Fade | Debug Fade | Link | → Page 31 |
| EventLogging | Event Logging | Action | Off, Simple, Robust · cmd=`Bio4EventLogModelMode` |
| RemoveCutsceneStaleData | Set Clear Stale Cutscene Data | ConsoleCmd | `VR4KeepStaleSceneData false` |

#### DoAction Handlers (case 8)
| OptionName Match | Action |
|-----------------|--------|
| `"Reset All Game State Flags"` | `ResetAllGameStateFlags()` |

#### ProcessNewSetting Handlers (case 8)
| OptionName Match | Property / Function |
|-----------------|---------------------|
| `"Debug Pawn"` | On: `PossessDebugPlayerPawn(VR4DebugPlayerPawn_BP_C)` / Off: `ClearDebugPlayerPawn()` |
| `"World Scale"` | `PlayerController.SetWorldScale(index)` |
| `"Disable Framerate Correction"` | `LocalDebugSettings.DisableFramerateCorrection` = bool |
| `"Master Volume"` | `PlayerController.SetMasterVolume(index)` |
| `"Hide Hands"` | `Player.DebugHideHandsForProps` = bool |
| `"Laser Pointer"` | `Player.DebugLaserPointer` = bool |
| `"Show Leon Model"` | `Player.DebugShowLeonModel` = bool |
| `"Hold To Select"` | `GameInstance.UseNewWeaponWheel` = bool |
| `"Display Adaptive Difficulty"` | `LocalDebugSettings.DisplayAdaptiveDifficulty` = bool |
| `"Overwrite Adaptive Difficulty"` | `LocalDebugSettings.OverwriteAdaptiveDifficulty` = bool |
| `"Difficulty Overwrite Level"` | `LocalDebugSettings.DifficultyOverwriteLevel` = index |
| `"Target Override Region"` | `LocalDebugSettings.TargetOverrideRegion` = index |
| `"Platform Override"` | `GameInstance.PlatformOverride` = index + `PlatformChanged()` |
| `"Overwrite Shadow Settings"` | `LocalDebugSettings.OverwriteShadowSettings` = bool |
| `"Shadow Feather Power"` | `LocalDebugSettings.ShadowFeatherPower` = index |
| `"Shadow Opacity"` | `LocalDebugSettings.ShadowOpacity` = float(NewSetting) |
| `"Shadow Scale"` | `LocalDebugSettings.ShadowScale` = float(NewSetting) |
| `"Fog Enable"` | `LocalDebugSettings.FogEnable` = bool |
| `"Disconnect Buttons"` | `LocalDebugSettings.DisconnectButtons` = bool |
| `"Show Qte Info"` | `LocalDebugSettings.ShowQteInfo` = bool |
| `"Debug Input Handedness"` | `LocalDebugSettings.DebugInputHandedness` = index (0=Left, 1=Right) |
| `"Event Logging"` | `ExecuteConsoleCommand("Bio4EventLogModelMode")` + appends option index |
| `"Test Weapon Accuracy"` | `LocalDebugSettings.TestWeaponAccuracy` = bool |
| `"Force Mafia Gear"` | `LocalDebugSettings.ForceMafiaGear` = bool |

---

### Page 9 — "Cheats"
**Enum value**: 9 · **MaxVisible**: 0

#### DataTable Items (18)
| Row Key | Option Name | Type | Options |
|---------|-------------|------|---------|
| C_GodMode | God Mode | Toggle | |
| C_AmmoCheat | Ammo Cheat | Action | Finite Ammo, Infinite Reloads, Infinite Ammo |
| C_InstaKill | Insta Kill Mode | Toggle | |
| C_AllFiles | Get All Files | Setting | |
| C_KillAllEnemies | Kill All Active Enemies | Setting | |
| C_HurtPlayer | Hurt Player | Setting | |
| C_HealPlayer | Heal Player | Setting | |
| C_HurtAshley | Hurt Ashley | Setting | |
| C_HealAshley | Heal Ashley | Setting | |
| C_AutoKillEnemies | Auto Kill Enemies | Toggle | |
| C_SaveDog | R100 Dog Saved | Toggle | |
| C_SetWound | R317 Face Wound | Toggle | |
| C_MerchantFreeForAll | Merchant Free For All | Toggle | |
| C_Give10k | Give 10k | Setting | |
| C_ShootingRangeMode | Shooting Range Mode | Action | Default, A, B, C, D |
| C_ClearActionHistory | Clear Action History | Setting | |
| C_ToggleSt3Timers | Toggle Stage3 Timers | ConsoleCmd | `VR4ToggleStage3Timers` |
| C_Cutscenes | Cutscenes | Link | → Page 28 |

#### DoAction Handlers (case 9)
| OptionName Match | Action |
|-----------------|--------|
| `"Get All Files"` | `DebugGetAllFiles()` |
| `"Kill All Active Enemies"` | `KillAllActiveEnemies()` |
| `"Hurt Player"` | `HurtPlayer(true, 1)` |
| `"Heal Player"` | `HurtPlayer(true, -100)` |
| `"Hurt Ashley"` | `HurtPlayer(false, 1)` |
| `"Heal Ashley"` | `HurtPlayer(false, -100)` |
| `"Give 10k"` | `DebugGive10k()` |
| `"Clear Action History"` | `ClearActionHistory()` |

#### ProcessNewSetting Handlers (case 9)
| OptionName Match | Property / Function |
|-----------------|---------------------|
| `"God Mode"` | `LocalDebugSettings.EnableGodMode` = bool |
| `"Ammo Cheat"` | `LocalDebugSettings.AmmoCheatState` = {0→0(Finite), 1→1(InfReloads), 2→2(InfAmmo)} |
| `"Insta Kill Mode"` | `LocalDebugSettings.EnableInstaKill` = bool |
| `"Auto Kill Enemies"` | `LocalDebugSettings.EnableAutoKillEnemies` = bool |
| `"R100 Dog Saved"` | `SetR100DogSaved(bool)` |
| `"R317 Face Wound"` | `SetR317FaceWound(bool)` |
| `"Merchant Free For All"` | `LocalDebugSettings.EnableMerchantFreeForAll` = bool |
| `"Shooting Range Mode"` | `LocalDebugSettings.OverrideShootingRangeMode` = index |

---

### Page 10 — "Inventory"
**Enum value**: 10 · **MaxVisible**: 0

#### DataTable Items (12)
| Row Key | Option Name | Type | Target |
|---------|-------------|------|--------|
| Inv_Guns | Guns | Link | → Page 14 |
| Inv_GunMods | Gun Mods | Link | → Page 15 |
| Inv_Grenades | Grenades | Link | → Page 16 |
| Inv_Ammo | Ammo | Link | → Page 17 |
| Inv_Consumables | Consumables | Link | → Page 18 |
| Inv_AttacheCases | Attache Cases | Link | → Page 19 |
| Inv_KeyItems | Key Items | Link | → Page 20 |
| Inv_TreasureItems | Treasure Items | Link | → Page 21 |
| Inv_TresureItems2 | Treasure Items 2 | Link | → Page 22 |
| Inv_BottleCaps | Bottle Caps | Link | → Page 23 |
| Inv_Fill | Fill | Setting | |
| Inv_Clear | Clear | Setting | |

#### DoAction Handlers (case 10)
| OptionName Match | Action |
|-----------------|--------|
| `"Fill"` | `FillInventory()` |
| `"Clear"` | `ClearInventory()` |

---

### Page 11 — "Pickup Item"
**Enum value**: 11 · **MaxVisible**: 0

#### DataTable Items (1) + Dynamic Items
Static: `[Pickup_AllItems] "All Items" type=Setting`

**NewMenu generation**:
- If `self.ShowAllItems == true`: iterates all 256 `EItemId` enum entries → `CreateActiveOption(name)`
- If `self.ShowAllItems == false`: iterates `self.PickupItemQuickMenu` array → `CreateActiveOption(name)`, then adds `"*AllItems"` and `"Room Keys"`

#### DoAction Handlers (case 11)
| OptionName Match | Action |
|-----------------|--------|
| `"*AllItems"` | Toggles `self.ShowAllItems`, calls `NewMenu()` to regenerate |
| `"Room Keys"` | Calls `DebugGivePlayerRoomKeys()` |
| (any other item) | Matches selected name to `EItemId` enum → spawns item in front of player via `DebugSpawnPickupItem(enum_value)` |

---

### Page 12 — "Particle Debug"
**Enum value**: 12 · **MaxVisible**: 0

#### DataTable Items (6)
| Row Key | Option Name | Type | Options |
|---------|-------------|------|---------|
| Particles_ForceOverrideMode | Force Mode | Action | Default, Original, Override, Nothing, Fallback |
| Particles_ResetTracked | Reset Tracked Particles | Setting | |
| Particles_ShowInfo | Show Particle Info | Action | Off, Ids, Ids + Rotation, Ids + Tool Flags |
| Particles_ShowWireframe | Show Bio4 Wireframes | Action | Disabled, Enabled |
| Particles_ToggleRain | Toggle Rain | ConsoleCmd | `VR4ToggleRain` |
| Particles_ParticleDebugMode | Particle Debug Mode | Action | Off, Ids, Ids + Axis |

#### DoAction Handlers (case 12)
| OptionName Match | Action |
|-----------------|--------|
| `"Reset Tracked Particles"` | `ResetTrackedParticles()` |

#### ProcessNewSetting (case 12)
Delegates entirely to `ProcessParticleSetting(NewSetting)`.

---

### Page 13 — "Rendering"
**Enum value**: 13 · **MaxVisible**: 0

#### DataTable Items (8)
| Row Key | Option Name | Type | Console Command |
|---------|-------------|------|-----------------|
| Rendering_PixelDensity | PixelDensity | Action | opts: 0.5, 0.81, 0.9, 0.97, 1, 1.1, 1.2, 1.3, 1.4, 1.5 |
| Rendering_NoAA | NoAA | ConsoleCmd | `r.MobileMSAA 1` |
| Rendering_2xAA | 2xAA | ConsoleCmd | `r.MobileMSAA 2` |
| Rendering_4xAA | 4xAA | ConsoleCmd | `r.MobileMSAA 4` |
| Rendering_ToggleShadow | Toggle Shadow | ConsoleCmd | `vr4ToggleShadow` |
| Rendering_ToggleStaticShadows | Toggle Shadows on Characters | ConsoleCmd | `vr4ToggleStaticShadow` |
| Rendering_ToggleProjectionShadows | Toggle Projected Shadows on Characters | ConsoleCmd | `vr4ToggleProjectedShadow` |
| Rendering_ToggleRimLighting | Toggle Rim Lighting | ConsoleCmd | `vr4ToggleRimLighting` |

#### ProcessNewSetting (case 13)
| OptionName Match | Property / Function |
|-----------------|---------------------|
| `"PixelDensity"` | `ProcessPixelDensitySetting(NewSetting)` + `LocalDebugSettings.VrPixelDensity = float(NewSetting)` |

---

### Pages 14–23 — Inventory Sub-Pages (Dynamic)

These pages have **NO DataTable entries**. Items are generated dynamically in `NewMenu()` by iterating game enums.

| Page | Display Name | Enum Iterated | Entry Count | DoAction Function |
|------|-------------|---------------|:-----------:|-------------------|
| 14 | Inventory - Guns | `EGunItem` | 23 | `DebugGivePlayerGun(matched_enum)` |
| 15 | Inventory - Gun Mods | `EGunModItem` | 7 | `DebugGivePlayerGunMod(matched_enum)` |
| 16 | Inventory - Grenades | `EGrenadeItem` | 7 | `DebugGivePlayerGrenade(matched_enum)` |
| 17 | Inventory - Ammo | `EAmmoItem` | 8 | `DebugGivePlayerAmmo(matched_enum)` |
| 18 | Inventory - Consumables | `EConsumableItem` | 16 | `DebugGivePlayerConsumable(matched_enum)` |
| 19 | Inventory - Attache Cases | `EAttacheCaseItem` | 5 | `DebugGivePlayerAttacheCase(matched_enum)` |
| 20 | Inventory - Key Items | `EKeyItem` | 27 | `DebugGivePlayerKeyItem(matched_enum)` |
| 21 | Inventory - Treasure Items | `AddTreasureItems(0,25)` | ~26 | `DebugGivePlayerTreasureItem(matched_enum)` |
| 22 | Inventory - Treasure Items 2 | `AddTreasureItems(26,37)` | ~12 | `DebugGivePlayerTreasureItem(matched_enum)` |
| 23 | Inventory - Bottle Caps | `EBottleCapItem` | 25 | `DebugGivePlayerBottleCap(matched_enum)` |

**DoAction pattern (cases 14–23)**:
1. Iterates the corresponding enum
2. Gets `GetEnumeratorUserFriendlyName()` for each value
3. Compares against `LocalOptionName` (the selected item's display name)
4. When match found, calls the corresponding `DebugGivePlayer*()` function

**ProcessNewSetting**: No handlers for pages 14–23.

---

### Page 24 — "Profiling"
**Enum value**: 24 · **MaxVisible**: 0

#### DataTable Items (10) — ALL ConsoleCommands
| Row Key | Option Name | Console Command |
|---------|-------------|-----------------|
| Profiling_StatFPS | Stat FPS | `Stat FPS` |
| Profiling_StatParticles | Stat Particles | `Stat Particles` |
| Profiling_StatParticlesOverview | Stat ParticlesOverview | `Stat ParticlesOverview` |
| Profiling_StatUnit | Stat Unit | `Stat Unit` |
| Profiling_StatUnitGraph | Stat UnitGraph | `Stat UnitGraph` |
| Profiling_StatStartFile | Stat StartFile | `Stat StartFile` |
| Profiling_StatStopFile | Stat StopFile | `Stat StopFile` |
| Profiling_ShowPositionToggle | ShowPosition Toggle | `ShowPosition Toggle` |
| LogMissingSoft | Log Missing Soft Objects | `vr4LogMissingSmdTags true` |
| LogLoadedAssets | Log Loaded Assets | `logLoadedAssets` |

> **No DoAction or ProcessNewSetting** — all items are ConsoleCommand type.

---

### Page 25 — "ViewMode"
**Enum value**: 25 · **MaxVisible**: 0

#### DataTable Items (7)
| Row Key | Option Name | Type | Console Command / Options |
|---------|-------------|------|--------------------------|
| ViewMode_Lit | Lit | ConsoleCmd | `Viewmode Lit` |
| ViewMode_Unlit | Unlit | ConsoleCmd | `Viewmode Unlit` |
| ViewMode_Wireframe | Wireframe | ConsoleCmd | `Viewmode Wireframe` |
| ViewMode_LightingOnly | Lighting Only | ConsoleCmd | `Viewmode LightingOnly` |
| ViewMode_LightComplexity | Light Complexity | ConsoleCmd | `Viewmode LightComplexity` |
| ViewMode_ShaderComplexity | Shader Complexity | ConsoleCmd | `Viewmode ShaderComplexity` |
| ViewMode_ToggleFog | Toggle Fog | Action | Disabled, Enabled |

#### ProcessNewSetting (case 25)
| OptionName Match | Function |
|-----------------|----------|
| `"Toggle Fog"` | `ProcessFogSetting(NewSetting)` |

---

### Page 26 — "Achievements" (Dynamic)
**Enum value**: 26 · **MaxVisible**: 0

**No DataTable items.** Generated in `NewMenu()` by `DrawAchievements()` which iterates `EAchievementId` (12 entries).

#### DoAction Handlers (case 26)
| OptionName Match | Action |
|-----------------|--------|
| `"ClearAllAchievements"` | `GetInstance().ClearAllAchievements()` |
| (any achievement name) | Matches to `EAchievementId` enum → `AchievementEarned(enum_value)` |

---

### Page 27 — "Finish Game"
**Enum value**: 27 · **MaxVisible**: 0

#### DataTable Items (2)
| Row Key | Option Name | Type | Options |
|---------|-------------|------|---------|
| FinishGame_Difficulty | Finish Difficulty | Action | Easy, Normal, Professional |
| FinishGame_SkipToEndGame | Launch End Game Sequence | Setting | |

#### DoAction Handlers (case 27)
| OptionName Match | Action |
|-----------------|--------|
| `"Launch End Game Sequence"` | `OpenLevel("r333")` |

#### ProcessNewSetting (case 27)
| OptionName Match | Function |
|-----------------|----------|
| `"Finish Difficulty"` | `ProcessFinishGameDifficulty(NewSetting)` — sets difficulty before end-game |

---

### Page 28 — "Cheats - Cutscenes"
**Enum value**: 28 · **MaxVisible**: 20 (explicit in NewMenu)

#### DataTable Items (38) — ALL ConsoleCommands
| Row Key | Option Name | Console Command |
|---------|-------------|-----------------|
| C_SpawnCutsceneBox | Force Cutscene Box | `ForceStreamTheater` |
| C_Cuts_r101s30 | r101s30 | `cutsceneToPlay r101s30` |
| C_Cuts_r104s00 | r104s00 | `cutsceneToPlay r104s00` |
| C_Cuts_r104showView | r104showView | `cutsceneToPlay r104showView` |
| C_Cuts_r104s10 | r104s10 | `cutsceneToPlay r104s10` |
| C_Cuts_r104s20 | r104s20 | `cutsceneToPlay r104s20` |
| C_Cuts_r105s00 | r105s00 | `cutsceneToPlay r105s00` |
| C_Cuts_r105s10 | r105s10 | `cutsceneToPlay r105s10` |
| C_Cuts_r106s00 | r106s00 | `cutsceneToPlay r106s00` |
| C_Cuts_r10bs10 | r10bs10 | `cutsceneToPlay r10bs10` |
| C_Cuts_r10bs20 | r10bs20 | `cutsceneToPlay r10bs20` |
| C_Cuts_r10bs22 | r10bs22 | `cutsceneToPlay r10bs22` |
| C_Cuts_r11bs00 | r11bs00 | `cutsceneToPlay r11bs00` |
| C_Cuts_r11bem | r11bem (Colmillos) | `cutsceneToPlay r11bem` |
| C_Cuts_r117s00 | r117s00 | `cutsceneToPlay r117s00` |
| C_Cuts_r117s10 | r117s10 | `cutsceneToPlay r117s10` |
| C_Cuts_r11cs10 | r11cs10 | `cutsceneToPlay r11cs10` |
| C_Cuts_r11fs10 | r11fs10 | `cutsceneToPlay r11fs10` |
| C_Cuts_r11fs11 | r11fs11 | `cutsceneToPlay r11fs11` |
| C_Cuts_r206s10 | r206s10 | `cutsceneToPlay r206s10` |
| C_Cuts_r206s20 | r206s20 | `cutsceneToPlay r206s20` |
| C_Cuts_r226ToggleRobot | r226 Toggle Skip to Robot Chase | `VR4ToggleSkipToRobotChase` |
| C_Cuts_r228salazarDeath | r228salazarDeath | `cutsceneToPlay r228salazarDeath` |
| C_Cuts_r30as00 | r30as00 | `cutsceneToPlay r30as00` |
| C_Cuts_r30as10 | r30as10 | `cutsceneToPlay r30as10` |
| C_Cuts_r30fSkipToElevator | r30fSkipToElevator | `cutsceneToPlay r30fSkipToElevator` |
| C_Cuts_r31bsU3Death | r31bsU3Death | `cutsceneToPlay r31bsU3Death` |
| C_Cuts_r31cs01 | r31cs01 | `cutsceneToPlay r31cs01` |
| C_Cuts_r31cs02 | r31cs02 | `cutsceneToPlay r31cs02` |
| C_Cuts_r31csKrauserDeath | r31csKrauserDeath | `cutsceneToPlay r31csKrauserDeath` |
| C_Cuts_r330s00 | r330s00 | `cutsceneToPlay r330s00` |
| C_Cuts_r331s00 | r331s00 | `cutsceneToPlay r331s00` |
| C_Cuts_r331s10 | r331s10 | `cutsceneToPlay r331s10` |
| C_Cuts_r332s00 | r332s00 | `cutsceneToPlay r332s00` |
| C_Cuts_r332s10 | r332s10 | `cutsceneToPlay r332s10` |
| C_Cuts_r332s20 | r332s20 | `cutsceneToPlay r332s20` |
| C_Cuts_r332sSadlerDeath | r332sSadlerDeath | `cutsceneToPlay r332sSadlerDeath` |
| C_Cuts_r332sSadlerDeathByRocket | r332sSadlerDeathByRocket | `cutsceneToPlay r332sSadlerDeathByRocket` |

#### ProcessNewSetting (case 28)
Delegates to `ProcessCutsceneSetting(NewSetting)`.

> **Note**: Despite being ConsoleCommand type, the menu system also fires ProcessNewSetting for page 28.

---

### Page 29 — "Favorites" (Dynamic)
**Enum value**: 29 · **MaxVisible**: 0

**No DataTable items.** Generated in `NewMenu()` from `LocalDebugSettings.DebugFavorites` array.

For each favorited item name:
1. Looks up the row in `DebugMenuOptions` DataTable
2. If found, creates widget with the item's OptionName, OptionList, and current index
3. If it's an "Override Level" type, adds all DebugLevelList entries to options
4. If the favorited name matches a `DebugLevelList` row, creates a level-link widget instead
5. Falls back to a simple text widget for unknown names

#### DoAction Handlers (case 29)
Resolves `self.ActiveShortcut` (the favorited item's original DataTable row name), looks up `OptionMenuType` from the DataTable, then **re-dispatches** to the appropriate page handler (e.g., if the favorited item is from page 9, jumps to case 9's DoAction logic).

---

### Page 30 — "Audio"
**Enum value**: 30 · **MaxVisible**: 0

#### DataTable Items (8)
| Row Key | Option Name | Type | Details |
|---------|-------------|------|---------|
| Audio_StatSoundWaves | Stat SoundWaves | ConsoleCmd | `stat SoundWaves` |
| Audio_StatSoundMixes | Stat SoundMixes | ConsoleCmd | `stat SoundMixes` |
| DD_OverrideAttenuation | Override Attenuation | Toggle | |
| DD_DebugOverrideAttenuation | Debug Attenuation | Toggle | |
| DD_DebugOverrideAttenuationFilter | Debug Attenuation Filter | Action | Core, Player, Weapon, Foot, Room, Door, EnemyMain, AllEnemies, All, None |
| DD_HideOneShotAudioDebugText | Hide One Shot Audio Debug Text | Toggle | |
| DD_FilterSoundDebugTextByTableNumber | Filter Sound DebugText By Table Number | Toggle | |
| DD_SoundTableNumberFilter | Sound Table Number Filter | Action | 0–20, -1 (ue attenuated) |

#### ProcessNewSetting Handlers (case 30)
| OptionName Match | Property / Function |
|-----------------|---------------------|
| `"Override Attenuation"` | `LocalDebugSettings.OverrideAttenuation` = bool |
| `"Debug Attenuation"` | `LocalDebugSettings.DebugOverrideAttenuation` = bool |
| `"Debug Attenuation Filter"` | `LocalDebugSettings.SoundFilter` = bitmask value based on selection |
| `"Hide One Shot Audio Debug Text"` | `LocalDebugSettings.HideOneShotAudioDebugText` = bool |
| `"Filter Sound DebugText By Table Number"` | `LocalDebugSettings.FilterSoundDebugTextByTableNumber` = bool |
| `"Sound Table Number Filter"` | `LocalDebugSettings.SoundTableNumberFilter` = int(NewSetting) |

---

### Page 31 — "Debug Fade"
**Enum value**: 31 · **MaxVisible**: 0

#### DataTable Items (3) — ALL ConsoleCommands
| Row Key | Option Name | Console Command |
|---------|-------------|-----------------|
| DF_StartRedFade | Start Red Fade | `StartFade RiseDuration=1.0 ScaleColor=FF7F0000 OffsetColor=FF7F0000 StickAround=true` |
| DF_StartBlackFade | Start Black Fade | `StartFade RiseDuration=1.0 ScaleColor=FF3F3F3F3F OffsetColor=00000000 StickAround=true` |
| DF_SopFade | Stop Fade | `StopFade` |

> **No DoAction or ProcessNewSetting** — all ConsoleCommand type.

---

### Page 33 — "Mercenaries"
**Enum value**: 33 · **MaxVisible**: 0

#### DataTable Items (14)
| Row Key | Option Name | Type | Details |
|---------|-------------|------|---------|
| Toggle_Mercenaries_Timer | Toggle Mercenaries Timer | ConsoleCmd | `vr4ToggleMercenariesTimer` |
| Merc_SetStarRanking | Force Star Ranking | Action | 0, 1, 2, 3, 4, 5 |
| Merc_EndMatch | End Match | Setting | |
| Merc_ResetNewNotifications | Reset NEW Notifications | Setting | |
| Merc_BypassLocked | Bypass Locked | Toggle | |
| RefillSuper | Fill Krauser Super | ConsoleCmd | `VR4ForceKrauserSuperCharged` |
| Merc_ClearStats | Clear Mercenaries Stats | Setting | |
| Merc_DisplayLifetimeStats | Merc Stats Display Mode | Action | none, current battle-shots, current battle-kills, current battle-miscs, classic lifetime stats, challenge lifetime stats |
| Merc_ClearUnlocks | Clear Mercenaries Unlocks | Setting | |
| Merc_DebugEnemySpawning | Debug Enemy Spawning | ConsoleCmd | `vr4ToggleMercenariesDebugEnemySpawning` |
| Merc_ToggleRainFX | Toggle Rain FX | Toggle | |
| Merc_IncreaseStars | Increase Total Stars | Setting | |
| Merc_DecreaseStars | Decrease Total Stars | Setting | |
| Merc_StarCount | Total Challenge Rank | Action | (read-only display) |

#### DoAction Handlers (case 33)
| OptionName Match | Action |
|-----------------|--------|
| `"End Match"` | `EndMercMatch()` |
| `"Reset NEW Notifications"` | `ResetMercNotifications()` |
| `"Clear Mercenaries Stats"` | `ClearMercStats()` |
| `"Clear Mercenaries Unlocks"` | `ClearMercUnlocks()` |
| `"Increase Total Stars"` | `IncreaseTotalStars()` |
| `"Decrease Total Stars"` | `DecreaseTotalStars()` |

#### ProcessNewSetting Handlers (case 33)
| OptionName Match | Property / Function |
|-----------------|---------------------|
| `"Toggle Rain FX"` | `ToggleMercRainFX()` |
| `"Bypass Locked"` | `SaveGame.DEBUG_BypassLockedMercContent` = bool |
| `"Merc Stats Display Mode"` | `LocalDebugSettings.MercenariesStatsDisplayMode` = index |
| `"Force Star Ranking"` | `MercenariesSetStars(index)` |
| `"Total Challenge Rank"` | Read-only: displays `GetChallengeRankTotal()` |

---

### Page 34 — "Unlockables" (Dynamic)
**Enum value**: 34 · **MaxVisible**: 0

**No DataTable items.** Generated in `NewMenu()`:
1. `"Toggle New Game Plus - [Enabled/Disabled]"` — shows current state from `GetGameCount()`
2. `DrawCharacterUnlocks()` — character unlock entries
3. `DrawMercenariesUnlocks()` — mercenaries unlock entries
4. `"Toggle Costume - [current costume name]"` — shows `GetCostume()` friendly name

#### DoAction Handlers (case 34)
Calls `ProcessUnlocks()` which handles toggling NG+, character unlocks, mercenary unlocks, and costume cycling.

---

### Page 35 — "Enemies"
**Enum value**: 35 · **MaxVisible**: 0

#### DataTable Items (1)
| Row Key | Option Name | Type | Console Command |
|---------|-------------|------|-----------------|
| Enemies_ToggleEnemyMove | Toggle Enemy Updates | ConsoleCmd | `toggleEnemyMove` |

> **No DoAction or ProcessNewSetting** — single ConsoleCommand item.

---

## Cross-Reference

### DoAction Switch Cases → Pages
| Case Value | Page Name | Item Types Handled |
|-----------|-----------|-------------------|
| 1 | Debug Menu | Setting items (Reload Level, Restart, Spawn Ashley, etc.) |
| 2 | Load Level: Select Stage | Override Level, Clear Override, Calibration |
| 8 | Debug Settings | Reset All Game State Flags |
| 9 | Cheats | Get All Files, Kill/Hurt/Heal, Give 10k, etc. |
| 10 | Inventory | Fill, Clear |
| 11 | Pickup Item | *AllItems toggle, item spawning |
| 12 | Particle Debug | Reset Tracked Particles |
| 14 | Inventory - Guns | DebugGivePlayerGun() |
| 15 | Inventory - Gun Mods | DebugGivePlayerGunMod() |
| 16 | Inventory - Grenades | DebugGivePlayerGrenade() |
| 17 | Inventory - Ammo | DebugGivePlayerAmmo() |
| 18 | Inventory - Consumables | DebugGivePlayerConsumable() |
| 19 | Inventory - Attache Cases | DebugGivePlayerAttacheCase() |
| 20 | Inventory - Key Items | DebugGivePlayerKeyItem() |
| 21, 22 | Treasure Items 1 & 2 | DebugGivePlayerTreasureItem() |
| 23 | Bottle Caps | DebugGivePlayerBottleCap() |
| 26 | Achievements | AchievementEarned() / ClearAllAchievements() |
| 27 | Finish Game | OpenLevel("r333") |
| 29 | Favorites | Re-dispatch to original page's handler |
| 33 | Mercenaries | EndMercMatch, ClearStats, etc. |
| 34 | Unlockables | ProcessUnlocks() |

### ProcessNewSetting Switch Cases → Pages
| Case Value | Page Name | Handler Count |
|-----------|-----------|:------------:|
| 2 | Load Level: Select Stage | 1 (Override Level) |
| 6 | First Person Options | 21 |
| 7 | Debug Display | 23 |
| 8 | Debug Settings | 24 |
| 9 | Cheats | 8 |
| 12 | Particle Debug | → ProcessParticleSetting() |
| 13 | Rendering | → ProcessPixelDensitySetting() |
| 25 | ViewMode | → ProcessFogSetting() |
| 27 | Finish Game | → ProcessFinishGameDifficulty() |
| 28 | Cheats - Cutscenes | → ProcessCutsceneSetting() |
| 30 | Audio | 6 |
| 33 | Mercenaries | 5 |

### Pages With NO Blueprint Logic (Pure ConsoleCommand)
| Page | Item Count | Description |
|------|:---------:|-------------|
| 24 (Profiling) | 10 | All `Stat *` console commands |
| 25 (ViewMode) | 6 of 7 | All `Viewmode *` commands (1 Action: Toggle Fog) |
| 28 (Cutscenes) | 38 | All `cutsceneToPlay *` commands |
| 31 (Debug Fade) | 3 | StartFade/StopFade commands |
| 35 (Enemies) | 1 | `toggleEnemyMove` |

### Summary Statistics
| Metric | Count |
|--------|:-----:|
| Total DataTable rows | 235 |
| Total pages defined | 36 (values 0–35, gaps at 36/37) |
| Pages with DataTable items | 19 |
| Pages with dynamic items only | 14 (3,4,5,14–23,26,29,32,34) |
| Unused pages (no items either way) | 3 (24=Profiling has items, all used) |
| Total ConsoleCommand items | 76 |
| Total Link items | 43 |
| Total Toggle items | 59 |
| Total Action items | 34 |
| Total Setting items | 23 |
| DoAction switch cases | 22 pages |
| ProcessNewSetting switch cases | 12 pages |
| Delegated ProcessNewSetting functions | 5 (ProcessParticleSetting, ProcessPixelDensitySetting, ProcessFogSetting, ProcessFinishGameDifficulty, ProcessCutsceneSetting, ProcessNewRaiseWeaponSetting) |

---

*Generated from analysis of decompiled DebugMenu_C Blueprint and DebugMenuOptions DataTable.*
*Enum values verified against SDK dump (DebugMenuType.lua) and DisplayNameMap (DebugMenuType.json).*
