# Debug Menu Rebuild Plan — Complete Blueprint Analysis

> **Generated from**: Decompiled Kismet bytecode of 5 Blueprint classes  
> **Source files**: `PAKS_extracted/decompiled/DebugMenu.txt` (7748 lines), `DebugOptionWidget.txt` (246 lines), `DebugVBoxList.txt` (185 lines), `DebugMenuWidget.txt` (54 lines), `DebugMenuInterface.txt` (129 lines)  
> **Companion doc**: `docs/DEBUG_MENU_API_ANALYSIS.md` (runtime Lua API reference)

---

## Table of Contents

1. [Complete Architecture Map](#1-complete-architecture-map)
2. [Function-by-Function Analysis](#2-function-by-function-analysis)
3. [Data Flow Analysis](#3-data-flow-analysis)
4. [Proven Modification Pipeline](#4-proven-modification-pipeline)
5. [Rebuild Strategy](#5-rebuild-strategy)
6. [Specific Modifications Possible](#6-specific-modifications-possible)

---

## 1. Complete Architecture Map

### 1.1 Class Hierarchy

```
UInterface
 └─ DebugMenuInterface_C              (Interface — 11 stub functions)

AActor
 └─ VR4DebugScreenActor               (C++ base — screen-attached actor)
     └─ DebugMenu_C                   (Blueprint — 52 functions, 7748 lines)
         ├─ owns → DebugMenuWidget_C  (via ParentWidget property)
         └─ tracks → DebugOptionWidget_C[]  (via ActiveOptionsWidgets array)

UUserWidget
 ├─ DebugMenuWidget_C                 (Blueprint — container widget, 54 lines)
 │   ├─ contains → DebugVBoxList_C    (via DebugVBoxList property)
 │   ├─ contains → TextBlock          (TitleText — page title)
 │   ├─ contains → TextBlock          (BuildVersion — version display)
 │   ├─ contains → VerticalBox        (WidgetVBox — root layout box)
 │   └─ contains → ScaleBox × 3      (ScaleBox_MapS1/S2/S3 — level map overlays)
 │
 ├─ DebugVBoxList_C                   (Blueprint — scrollable list, 185 lines)
 │   ├─ contains → VerticalBox        (ParentVBox — holds option widgets)
 │   ├─ contains → Image              (UpArrow — scroll indicator)
 │   └─ contains → Image              (DownArrow — scroll indicator)
 │
 └─ DebugOptionWidget_C               (Blueprint — single option row, 246 lines)
     ├─ contains → TextBlock          (OptionNameText — option label)
     ├─ contains → TextBlock          (CurrentSettingText — current value)
     ├─ contains → TextBlock          (ActionText — ">" for action items)
     ├─ contains → WidgetSwitcher     (Switcher — toggles action vs setting mode)
     ├─ contains → Image              (FavImage — star indicator)
     └─ contains → Image              (ShortcutImage — button indicator)
```

### 1.2 Widget Tree (Runtime)

```
DebugMenu_C (Actor, attached to VR debug screen)
 └─ ParentWidget: DebugMenuWidget_C
     ├─ TitleText (TextBlock) ← "MAIN MENU", "CHEATS", etc.
     ├─ BuildVersion (TextBlock) ← version string
     ├─ WidgetVBox (VerticalBox) ← root container, render translation shifts per page
     │   └─ DebugVBoxList_C (DebugVBoxList)
     │       ├─ UpArrow (Image) ← visible when scrolled down
     │       ├─ ParentVBox (VerticalBox) ← holds all option widgets
     │       │   ├─ DebugOptionWidget_C[0]  ← "Player Settings"
     │       │   ├─ DebugOptionWidget_C[1]  ← "General Settings"
     │       │   ├─ DebugOptionWidget_C[2]  ← "Cheats"
     │       │   ├─ ...
     │       │   └─ DebugOptionWidget_C[N]  ← "Back" (added last, non-main pages)
     │       └─ DownArrow (Image) ← visible when more items below
     └─ ScaleBox_MapS1/S2/S3 ← level select map images (hidden except on level pages)
```

### 1.3 Object Access from Lua

```lua
-- Actor (top-level)
local dm = FindFirstOf("DebugMenu_C")

-- Widget layer
local pw    = dm:Get("ParentWidget")          -- DebugMenuWidget_C
local vbl   = pw:Get("DebugVBoxList")         -- DebugVBoxList_C
local pvb   = vbl:Get("ParentVBox")           -- VerticalBox (actual container)
local title = pw:Get("TitleText")             -- TextBlock
local wvbox = pw:Get("WidgetVBox")            -- VerticalBox (root, shifted per page)

-- Option widgets
local opts = dm:Get("ActiveOptionsWidgets")   -- TArray<DebugOptionWidget_C>
local opt  = opts[1]                          -- first option (1-indexed)
local name = opt:Get("OptionName")            -- FString
local idx  = opt:Get("CurrentIndex")          -- int32
```

### 1.4 DataTable References

| DataTable | Description | Key Fields |
|---|---|---|
| `DebugMenuOptions` | All menu options, one row per option | `Menu_2` (DebugMenuType), `OptionName_18` (string), `OptionType_14` (DebugOptionType), `OptionList_19` (TArray\<string\>), `MenuLink_22` (DebugMenuType), `ConsoleCommand_25` (string) |
| `DebugLevelList` | Level select entries | `Stage_8` (int, 1-4), `Description_6` (string) |
| `DebugFavorites` | User-saved favorite options | Same structure as DebugMenuOptions rows |

### 1.5 Enum Definitions

#### DebugMenuType (36 values: 0–34 + 37)

| Value | SDK Name | Meaning (from decompiled logic) |
|---|---|---|
| 0 | NewEnumerator5 | Closed / None |
| 1 | NewEnumerator0 | **Main Menu** |
| 2 | NewEnumerator1 | Player Settings |
| 3 | NewEnumerator2 | Level Select — Stage 1 |
| 4 | NewEnumerator3 | Level Select — Stage 2 |
| 5 | NewEnumerator4 | Level Select — Stage 3 |
| 6 | NewEnumerator6 | General Settings |
| 7 | NewEnumerator7 | Debug Visualization |
| 8 | NewEnumerator8 | Debug Dev |
| 9 | NewEnumerator9 | Cheats |
| 10 | NewEnumerator10 | *(unknown / unused)* |
| 11 | NewEnumerator11 | Item Pickup |
| 12 | NewEnumerator12 | Particles |
| 13 | NewEnumerator19 | Pixel Density |
| 14 | NewEnumerator13 | Guns (inventory) |
| 15 | NewEnumerator14 | Gun Mods (inventory) |
| 16 | NewEnumerator15 | Grenades (inventory) |
| 17 | NewEnumerator16 | Ammo (inventory) |
| 18 | NewEnumerator17 | Consumables (inventory) |
| 19 | NewEnumerator18 | Attaché Cases (inventory) |
| 20 | NewEnumerator20 | Keys (inventory) |
| 21 | NewEnumerator23 | Treasures 1 (inventory) |
| 22 | NewEnumerator24 | Treasures 2 (inventory) |
| 23 | NewEnumerator25 | Bottle Caps (inventory) |
| 24 | NewEnumerator21 | *(unknown)* |
| 25 | NewEnumerator22 | Rendering |
| 26 | NewEnumerator26 | Achievements |
| 27 | NewEnumerator27 | Fog |
| 28 | NewEnumerator28 | All Options (search/filter page) |
| 29 | NewEnumerator29 | Favorites |
| 30 | NewEnumerator30 | Sound |
| 31 | NewEnumerator31 | *(unknown / unused)* |
| 32 | NewEnumerator32 | Level Select — Stage 4 |
| 33 | NewEnumerator33 | Audio Debug |
| 34 | NewEnumerator34 | Unlocks |
| 37 | NewEnumerator37 | *(unknown, SDK index 35)* |

#### DebugOptionType (5 values)

| Value | Meaning | Behavior on Confirm |
|---|---|---|
| 0 | Action | Executes `DoAction()` — one-shot command (spawn item, reload level, etc.) |
| 1 | Bool Toggle | Cycles On/Off via `OptionIncremented()` → `ProcessNewSetting()` |
| 2 | Multi-Value | Cycles through `OptionList[]` via `OptionIncremented()` → `ProcessNewSetting()` |
| 3 | SubMenu Link | Navigates to `MenuLink` page via `NewMenu()` |
| 4 | Console Command | Executes `ConsoleCommand` string via `ExecuteConsoleCommand()` |

---

## 2. Function-by-Function Analysis

### 2.1 DebugMenu_C (Main Actor — 52 functions, 7748 lines)

#### Lifecycle Functions

| Function | Expressions | Summary | Lua Callable |
|---|---|---|---|
| `Initialize` | 27 | Gets player pawn via `GetBio4PlayerPawnIfValid`, casts widget to `DebugMenuWidget_C`, stores as `ParentWidget`, sets `BuildVersion` text | ✅ `dm:Call("Initialize")` |
| `OpenDebugMenu` | 13 | `Initialize()` → validate player → clear `PreviousMenus` → `NewMenu(1, false)` → widget `SetVisibility(true)` → `EnableInput` → set Callback=true | ✅ `dm:Call("OpenDebugMenu")` |
| `HideDebugMenu` | 26 | `DisableInput` → clear `ScrollTimer` → `ShowAllItems=false` → `ClearWidgets` → iterate `ActiveOptionsWidgets` calling `RemoveFromParent` → widget `SetVisibility(false)` → `UpdateTooltip` → `SaveGame` → Callback=true | ✅ `dm:Call("HideDebugMenu")` |
| `IsActive` | 3 | Returns `ParentWidget.IsVisible()` | ✅ `dm:Call("IsActive")` |

#### Menu Navigation Functions

| Function | Expressions | Summary | Lua Callable |
|---|---|---|---|
| `NewMenu` | 831 | Core menu builder. ClearWidgets → push ActiveMenu to PreviousMenus → set ActiveMenu → set title from enum name → set WidgetVBox render translation (per-page offsets) → clear arrays → build options per ActiveMenu via DataTable/enum iteration → set MaxVisible → UpdateListView → add "Back" option (non-main pages) → UpdateOptionHighlight | ✅ `dm:Call("NewMenu", menuType, dontResetIndex)` |
| `PreviousMenu` | 10 | Pop last from `PreviousMenus` → `NewMenu(popped, false)` | ✅ `dm:Call("PreviousMenu")` |
| `ClearWidgets` | 34 | Destroys all `ActiveOptionsWidgets`, clears both arrays (`ActiveOptionsWidgets` + `ActiveDebugOptions`), calls `VBoxList.ClearWidgets()` | ✅ `dm:Call("ClearWidgets")` |
| `CreateActiveOption` | 13 | Creates `DebugOptionWidget_C`, sets `OptionName`/`DebugOptionName`, adds to `DebugVBoxList` and tracking arrays | ✅ `dm:Call("CreateActiveOption", "OptionName")` |

#### Option Handling Functions

| Function | Expressions | Summary | Lua Callable |
|---|---|---|---|
| `DoAction` | 1609 | Main confirm handler. Checks `OptionType`: type 0=Action (massive switch on `OptionName` string for ~30+ actions), type 1/2=toggle/cycle (`OptionIncremented` → `ProcessNewSetting`), type 3=SubMenu (`NewMenu(MenuLink)`), type 4=ConsoleCmd. Special: inventory pages spawn items, achievements page clears all, level select does `JumpToRoom` | ✅ `dm:Call("DoAction")` |
| `ProcessNewSetting` | 712 | Massive switch on `ProcessOption.Menu_2` (menu page), then nested switch on `OptionName` string. Reads `NewSetting` index → maps to game property → writes to `LocalDebugSettings` / `LocalPlayerSettings`. Calls `SetGamePlayerSettings` + `SetDebugSettings` + `UpdateBio4Language` at end | Indirect (triggered by DoAction) |
| `GetOptionIndex` | ~1500 | Reverse of ProcessNewSetting — reads current setting values to compute `CurrentIndex` for widget display. Same massive switch structure mirroring ProcessNewSetting | Indirect (used by NewMenu) |
| `GetActiveOption` | 33 | Returns `DebugMenuOption` struct from DataTable lookup for option at `CurrentIndex`. Looks up `ActiveDebugOptions[CurrentIndex]` by name in `DebugMenuOptions` table | ✅ `dm:Call("GetActiveOption")` |
| `UpdateOptionHighlight` | 55 | Iterates `ActiveOptionsWidgets`, calls `UpdateLook(Active=false)` on each, then `UpdateLook(Active=true)` on widget at `CurrentIndex`. Also checks favorites/shortcuts | ✅ `dm:Call("UpdateOptionHighlight")` |
| `BuildTooltip` | 993 | Builds tooltip text string for the currently selected option | ✅ `dm:Call("BuildTooltip")` |

#### Input Handler Functions

| Function | Expressions | Summary | Notes |
|---|---|---|---|
| `InputActionScrollUp` | ~40 | Gate pattern: first press starts timer (0.4s delay, 0.1s repeat). Decrements selection via `SelectionDecremented()`. Updates highlight | Hookable: `RegisterHook("/path:InputActionScrollUp")` |
| `InputActionScrollDown` | ~40 | Same gate pattern as ScrollUp. Increments selection via `SelectionIncremented()`. Updates highlight | Hookable |
| `InputActionConfirm` | ~20 | On Pressed: calls `DoAction()` (the big switch). Plays sound | Hookable |
| `InputActionBack` | ~15 | On Pressed: if ActiveMenu==1 → `HideDebugMenu()`, else `PreviousMenu()` | Hookable |
| `InputActionFavorite` | ~30 | Toggles current option in/out of `DebugFavorites` DataTable | Hookable |
| `InputActionSetShortcutAX` | ~20 | Assigns current option as AX button shortcut (`ActiveShortcut`) | Hookable |
| `InputActionSetShortcutBY` | ~20 | Assigns current option as BY button shortcut | Hookable |
| `InputActionDoShortcut` | ~15 | Executes saved shortcut (looks up `ActiveShortcut` and runs `DoAction`) | Hookable |

#### NewMenu — Render Translation Offsets per Page

The `NewMenu` function applies a `Vector2D` render translation to `WidgetVBox` based on the target page, shifting the widget panel on-screen:

| ActiveMenu | X Offset | Y Offset | Purpose |
|---|---|---|---|
| 3 (Stage 1) | -200.0 | -200.0 | Shift for level map overlay |
| 4 (Stage 2) | 280.0 | 0.0 | Shift for level map overlay |
| 5 (Stage 3) | 280.0 | 0.0 | Shift for level map overlay |
| 32 (Stage 4) | 280.0 | 0.0 | Shift for level map overlay |
| All others | 0.0 | 0.0 | Default centered position |

#### NewMenu — MaxVisible per Page

| ActiveMenu Values | MaxVisible | Context |
|---|---|---|
| 3 (Stage 1) | 8 | Level select, limited for map space |
| 4 (Stage 2) | 10 | Level select |
| 6 (General Settings) | 10 | Settings page |
| 32 (Stage 4) | 4 | Level select, few levels |
| 11 (Item Pickup) with ShowAllItems | 20 | Full item list mode |
| 28 (All Options) | 20 | Search/filter page |
| Default (most pages) | 0 | No limit (show all) |

#### NewMenu — Option Building Strategy per Page

| ActiveMenu | Build Method | Source |
|---|---|---|
| 1–2, 6–9, 12–13, 25, 27, 30, 33–34 | DataTable iteration | `DebugMenuOptions` rows where `Menu_2 == ActiveMenu` |
| 3, 4, 5, 32 | DataTable iteration | `DebugLevelList` rows where `Stage_8` matches page |
| 14–23 | Enum iteration | Game item enums (`EGunItem`, `EGunModItem`, `EGrenade`, `EAmmoItem`, `EConsumableItem`, `EAttacheItem`, `EKeyItem`, `ETreasure`, `EBottleCap`) |
| 28 (All Options) | Full DataTable | All rows from `DebugMenuOptions` |
| 29 (Favorites) | Favorites list | `DebugFavorites` DataTable |
| 26 (Achievements) | Enum iteration | `EAchievementId` enum |

### 2.2 DebugOptionWidget_C (Option Row Widget — 7 functions)

| Function | Expressions | Summary | Lua Callable |
|---|---|---|---|
| `Setup` | 32 | Sets `OptionNameText` from `OptionName`, sets `CurrentSettingText` from `SettingList[CurrentIndex]`. If `SettingList` is empty, switches `Switcher` to index 1 (action mode, shows ">"); otherwise index 0 (setting mode, shows current value) | ✅ `opt:Call("Setup")` |
| `UpdateLook` | 42 | If `Active=true`: text color → red (`LinearColor(1,0,0,1)`). If `Active=false`: text color → white (`LinearColor(1,1,1,1)`). Shows/hides `FavImage` based on Favorite param, shows/hides `ShortcutImage` based on Shortcut param | ✅ `opt:Call("UpdateLook", active, fav, shortcut)` |
| `OptionIncremented` | 18 | `CurrentIndex = (CurrentIndex + 1) % SettingList.Length`. Updates `CurrentSettingText`. Outputs `NewSetting` (the new index) | ✅ `opt:Call("OptionIncremented")` |
| `OptionReset` | 12 | `CurrentIndex = 0`. Updates `CurrentSettingText`. Outputs `NewSetting` (0) | ✅ `opt:Call("OptionReset")` |
| `OptionDecremented` | 18 | `CurrentIndex = (CurrentIndex - 1)`, wraps to end. Updates text. Outputs `NewSetting` | ✅ `opt:Call("OptionDecremented")` |
| `Construct` | 2 | Standard UMG construct → calls ExecuteUbergraph | Auto |
| `ExecuteUbergraph` | ~5 | Entry point dispatcher (minimal) | Auto |

#### Key Properties

| Property | Type | Description |
|---|---|---|
| `OptionName` | FString | Display name of the option |
| `DebugOptionName` | FName | Internal name for DataTable lookup |
| `SettingList` | TArray\<FString\> | Cycle values (e.g., ["Off", "On"] or ["Low", "Med", "High"]) |
| `CurrentIndex` | int32 | Current position in SettingList |
| `OptionNameText` | TextBlock | UMG text widget for option label |
| `CurrentSettingText` | TextBlock | UMG text widget for current value |
| `ActionText` | TextBlock | ">" indicator for action items |
| `Switcher` | WidgetSwitcher | Index 0=setting display, Index 1=action display |
| `FavImage` | Image | Star/favorite indicator |
| `ShortcutImage` | Image | Controller button shortcut indicator |
| `Active` | bool | Whether this option is currently highlighted |

### 2.3 DebugVBoxList_C (Scrollable List — 5 functions)

| Function | Expressions | Summary | Lua Callable |
|---|---|---|---|
| `AddWidget` | 8 | Creates slot, adds widget to `ParentVBox` with `HorizontalAlignment = HAlign_Fill` | ✅ `vbl:Call("AddWidget", widget)` |
| `ClearWidgets` | 12 | `ParentVBox.ClearChildren()`, `Selection=0`, `FirstVisible=0`, hide both arrows | ✅ `vbl:Call("ClearWidgets")` |
| `UpdateListView` | 48 | Iterates ParentVBox children: items in range `[FirstVisible, FirstVisible+MaxVisible)` → visible, all others → collapsed. Updates arrow visibility: UpArrow bright if `FirstVisible > 0` (more above), DownArrow bright if `FirstVisible + MaxVisible < childCount` (more below) | ✅ `vbl:Call("UpdateListView")` |
| `SelectionIncremented` | 28 | `Selection++`. If `Selection >= childCount` → wrap to 0 (and `FirstVisible=0`). If `Selection >= FirstVisible + MaxVisible` → scroll down (`FirstVisible++`). Calls `UpdateListView()` | ✅ `vbl:Call("SelectionIncremented")` |
| `SelectionDecremented` | 28 | `Selection--`. If `Selection < 0` → wrap to last (and `FirstVisible = max(0, childCount - MaxVisible)`). If `Selection < FirstVisible` → scroll up (`FirstVisible--`). Calls `UpdateListView()` | ✅ `vbl:Call("SelectionDecremented")` |

#### Key Properties

| Property | Type | Description |
|---|---|---|
| `ParentVBox` | VerticalBox | The actual container holding option widgets |
| `UpArrow` | Image | Scroll-up indicator (bright when more above) |
| `DownArrow` | Image | Scroll-down indicator (bright when more below) |
| `MaxVisible` | int32 | Max items visible at once (set by NewMenu per page) |
| `FirstVisible` | int32 | Index of first visible item (scroll position) |
| `Selection` | int32 | Currently selected item index (0-based) |

### 2.4 DebugMenuWidget_C (Container — 2 functions)

| Function | Expressions | Summary |
|---|---|---|
| `Construct` | 1 | Standard UMG construct → ExecuteUbergraph |
| `ExecuteUbergraph` | 0 | Empty. This widget is a pure container — all logic is in DebugMenu_C |

#### Key Properties

| Property | Type | Description |
|---|---|---|
| `TitleText` | TextBlock | Page title (set by NewMenu from enum name) |
| `BuildVersion` | TextBlock | Build version string (set by Initialize) |
| `DebugVBoxList` | DebugVBoxList_C | The scrollable list widget |
| `WidgetVBox` | VerticalBox | Root vertical box (render translation shifted per page) |
| `ScaleBox_MapS1` | ScaleBox | Level map image for Stage 1 |
| `ScaleBox_MapS2` | ScaleBox | Level map image for Stage 2 |
| `ScaleBox_MapS3` | ScaleBox | Level map image for Stage 3 |

### 2.5 DebugMenuInterface_C (Interface — 11 stubs)

All functions are empty stubs (return only). They define the contract that `DebugMenu_C` implements:

`OpenDebugMenu`, `HideDebugMenu`, `IsActive`, `InputActionScrollUp`, `InputActionScrollDown`, `InputActionConfirm`, `InputActionBack`, `InputActionFavorite`, `InputActionSetShortcutAX`, `InputActionSetShortcutBY`, `InputActionDoShortcut`

The VR controller input system calls these interface functions on the `DebugMenu_C` actor via `VR4PlayerController_BP_C`'s `BndEvt__DebugInput*` delegates (14 input events).

---

## 3. Data Flow Analysis

### 3.1 Menu Open Flow

```
VR controller trigger → VR4PlayerController_BP_C
    → BndEvt__DebugInputAX_..._OnInputActionPressed (Pressed=true)
    → DebugMenu_C::OpenDebugMenu()
        1. Initialize()
           ├─ GetBio4PlayerPawnIfValid() → validate player exists
           ├─ Cast widget reference → DebugMenuWidget_C
           └─ Set BuildVersion text
        2. Validate player is valid
        3. Clear PreviousMenus array
        4. NewMenu(1, false)  ← builds the Main Menu page
           ├─ ClearWidgets()
           ├─ Push ActiveMenu to PreviousMenus
           ├─ Set ActiveMenu = 1
           ├─ Set TitleText from enum name
           ├─ Set WidgetVBox.RenderTranslation = (0, 0)
           ├─ Clear ActiveOptionsWidgets + ActiveDebugOptions arrays
           ├─ FOR each row in DebugMenuOptions WHERE Menu_2 == 1:
           │   ├─ CreateActiveOption(row.OptionName)
           │   ├─ Set widget.SettingList = row.OptionList
           │   ├─ Set widget.CurrentIndex = GetOptionIndex(row)
           │   └─ widget.Setup()
           ├─ Set MaxVisible = 0 (no limit for main menu)
           ├─ VBoxList.UpdateListView()
           └─ UpdateOptionHighlight()
        5. ParentWidget.SetVisibility(Visible)
        6. EnableInput(PlayerController)
        7. Set Callback = true (signals completion)
```

### 3.2 Option Selection Flow (Confirm)

```
VR controller trigger → InputActionConfirm(Pressed=true)
    → DoAction()
        1. Get option at CurrentIndex from ActiveOptionsWidgets
        2. Get matching DataTable row via GetActiveOption()
        3. Read OptionType from row
        4. SWITCH on OptionType:
        
           Type 0 (Action):
             └─ SWITCH on OptionName string:
                 "Reload Level" → ReloadLevel()
                 "Load Game" → OpenLoadGameMenu() + HideDebugMenu()
                 "Give 10k" → GivePesetas(10000)
                 "Kill All Active Enemies" → KillAllEnemies()
                 "Heal Player" → HealPlayer()
                 "Hurt Player" → HurtPlayer()
                 "Spawn Ashley" → SpawnAshley()
                 "Launch End Game Sequence" → PrepareForRoomTransition + JumpToRoom("r333")
                 "Clear All Achievements" → ClearAllAchievements()
                 "Fill" (inventory) → MaxOutPlayerInventory()
                 "Clear" (inventory) → DebugClearPlayerInventory()
                 ... (~30+ total action commands)
        
           Type 1 (Bool Toggle):
             ├─ OptionIncremented() → cycles On/Off
             └─ ProcessNewSetting(optionData)
                 ├─ SWITCH on Menu_2 (which page):
                 │   Page 2 (Player): "Dual Wield" → LocalPlayerSettings.DualWield = NewSetting
                 │   Page 6 (General): "Language" → UpdateBio4Language(NewSetting)
                 │   Page 7 (Viz): "Show FPS" → LocalDebugSettings.ShowFPS = NewSetting
                 │   Page 9 (Cheats): "God Mode" → LocalDebugSettings.GodMode = NewSetting
                 │   ... (~60+ settings mapped)
                 ├─ SetGamePlayerSettings(LocalPlayerSettings)
                 └─ SetDebugSettings(LocalDebugSettings)
        
           Type 2 (Multi-Value):
             ├─ OptionIncremented() → cycles through SettingList
             └─ ProcessNewSetting(optionData)  ← same as Bool
        
           Type 3 (SubMenu):
             └─ NewMenu(row.MenuLink, false)  ← navigates to sub-page
        
           Type 4 (Console Command):
             └─ ExecuteConsoleCommand(row.ConsoleCommand)
```

### 3.3 Inventory Item Spawn Flow (Pages 14–23)

```
NewMenu(14, false)  ← "Guns" page
    1. ClearWidgets()
    2. Set ActiveMenu = 14
    3. FOR i = 0 to EnumCount(EGunItem):
    │   ├─ value = GetEnumeratorValueFromIndex(EGunItem, i)
    │   ├─ Skip invalid values (switch filters each valid enum value)
    │   ├─ name = GetEnumeratorUserFriendlyName(EGunItem, value)
    │   └─ CreateActiveOption(name)  ← one widget per gun
    4. MaxVisible = 0
    5. UpdateListView() + UpdateOptionHighlight()

DoAction() on inventory page (OptionType == 0, but special path):
    1. Search DebugMenuOptions for matching OptionName
    2. If NOT found in DataTable → it's an enum-generated item
    3. Iterate game enum (EGunItem etc.) to find matching name
    4. GetHeadLocation() - (0, 0, 50) = spawn origin
    5. GetCameraForwardVector() → project 150 units forward (XY only)
    6. RandomPointInBoundingBox(origin, extent=(40, 20, 0))
    7. SpawnItem(enumValue, spawnLocation, count=1, true)
```

### 3.4 Scrolling Flow

```
InputActionScrollDown(Pressed=true):
    1. Open gate (first press)
    2. Start timer: delay=0.4s, looping=true, rate=0.1s
    3. On each tick:
       ├─ VBoxList.SelectionIncremented()
       │   ├─ Selection++
       │   ├─ If Selection >= childCount → wrap to 0, FirstVisible = 0
       │   ├─ If Selection >= FirstVisible + MaxVisible → FirstVisible++
       │   └─ UpdateListView()
       ├─ DebugMenu.CurrentIndex = VBoxList.Selection
       └─ UpdateOptionHighlight()

InputActionScrollDown(Pressed=false):
    1. Close gate
    2. Clear timer
    3. Reset scroll state
```

### 3.5 Data Dependencies

```
DebugMenuOptions (DataTable, in PAK)
    ├── read by NewMenu() → builds option widgets per page
    ├── read by GetActiveOption() → retrieves option metadata on confirm
    ├── read by DoAction() → determines option type and action
    ├── read by ProcessNewSetting() → maps option name → game setting
    └── read by GetOptionIndex() → reads current value for display

DebugLevelList (DataTable, in PAK)
    └── read by NewMenu() → builds level select entries for pages 3,4,5,32

Game Enums (EGunItem, EAmmoItem, etc.)
    └── iterated by NewMenu() → builds inventory pages 14-23

LocalDebugSettings / LocalPlayerSettings (structs)
    ├── read by GetOptionIndex() → current setting values
    ├── written by ProcessNewSetting() → applies new values
    ├── flushed by SetDebugSettings() / SetGamePlayerSettings() → applies to game
    └── saved by SaveGame() → persists to save file (called by HideDebugMenu)
```

---

## 4. Proven Modification Pipeline

### 4.1 Tool Chain

| Tool | Version | Purpose | Command |
|---|---|---|---|
| **repak** | latest | Extract/repack PAK files | `repak unpack file.pak` / `repak pack folder/` |
| **UAssetGUI** | latest | Export/import uasset ↔ JSON | `UAssetGUI tojson file.uasset file.json` / `UAssetGUI fromjson file.json file.uasset` |
| **kismet_decompiler.py** | custom | Decompile Kismet bytecode to readable pseudo-code | `python tools\kismet_decompiler.py --all` |
| **ADB** | latest | Deploy to Quest | `adb push` / `adb forward` |
| **deploy.py** | custom | Automated deployment | `python tools\deploy.py mods` |

### 4.2 Complete Extraction Pipeline

```bash
# 1. Extract PAK files
repak unpack pakchunk0-Android_ETC2.pak

# 2. Locate the Debug Menu assets (inside extracted PAK structure)
#    DebugMenu_C.uasset       → DebugMenu_C Blueprint
#    DebugMenuWidget_C.uasset → Widget Blueprint
#    DebugVBoxList_C.uasset   → VBox list Blueprint  
#    DebugOptionWidget_C.uasset → Option widget Blueprint
#    DebugMenuOptions.uasset  → DataTable
#    DebugLevelList.uasset    → DataTable

# 3. Export to JSON for analysis/editing
UAssetGUI tojson DebugMenu_C.uasset DebugMenu_C.json
UAssetGUI tojson DebugMenuOptions.uasset DebugMenuOptions.json

# 4. Decompile Kismet bytecode for reading
python tools\kismet_decompiler.py --all
# → Produces DebugMenu.txt, DebugOptionWidget.txt, etc.
```

### 4.3 Modification Approaches

#### Approach A: DataTable Modification (Safest)

DataTables are simple structured data — easiest to modify without breaking bytecode.

```bash
# Export DataTable to JSON
UAssetGUI tojson DebugMenuOptions.uasset DebugMenuOptions.json

# Edit JSON: add/remove/modify rows
# Each row has: Menu_2, OptionName_18, OptionType_14, OptionList_19, MenuLink_22, ConsoleCommand_25

# Rebuild
UAssetGUI fromjson DebugMenuOptions.json DebugMenuOptions.uasset

# Repack into PAK with correct mount point
repak pack output_folder/
```

**What DataTable edits can do:**
- Add new options to existing menu pages
- Change option names, types, setting lists
- Add new SubMenu links (type 3) pointing to existing pages
- Add console commands (type 4)
- Remove options

**What DataTable edits CANNOT do:**
- Add new menu pages (requires Kismet bytecode changes in NewMenu)
- Add new action handlers (requires Kismet bytecode changes in DoAction)
- Change widget layout or appearance
- Modify scroll behavior

#### Approach B: Kismet Bytecode Patching (Advanced)

Direct modification of compiled Blueprint bytecode in JSON form.

```bash
# Export Blueprint to JSON
UAssetGUI tojson DebugMenu_C.uasset DebugMenu_C.json

# The JSON contains Kismet bytecode as serialized instruction arrays
# Modifications require understanding the bytecode format:
# - Each instruction has an opcode, operands, and jump targets
# - String comparisons use EX_SwitchString / EX_CallMath
# - Enum values are EX_ByteConst or EX_IntConst
# - Function calls are EX_CallFunction / EX_FinalFunction

# After editing:
UAssetGUI fromjson DebugMenu_C.json DebugMenu_C.uasset
repak pack output_folder/
```

**Risks:**
- One wrong byte offset breaks the entire function
- Jump target offsets must be recalculated if instructions are added/removed
- String table indices must match the Name Table in the uasset
- No decompiler → compiler roundtrip (manual bytecode editing only)

#### Approach C: Lua Runtime Hooks (Recommended for Most Changes)

Use the modloader's Lua API to intercept and extend behavior at runtime.

```lua
-- Hook input to add custom behavior
RegisterHook("/Game/Blueprints/DebugMenu.DebugMenu_C:InputActionConfirm", function(self, parms)
    local dm = self:get()
    local activeMenu = dm:Get("ActiveMenu")
    if activeMenu == 99 then  -- custom MODS page
        -- Handle confirm on our custom page
        HandleModsPageConfirm(dm)
        return "BLOCK"  -- prevent original DoAction
    end
end)

-- Hook NewMenu to inject custom page
RegisterHook("/Game/Blueprints/DebugMenu.DebugMenu_C:NewMenu", function(self, parms)
    -- Post-hook: if navigating to our custom page, rebuild content
end)
```

### 4.4 Deployment Pipeline

```bash
# For PAK modifications:
# 1. Place modified .pak in /sdcard/Android/obb/com.Armature.VR4/files/paks/
# 2. Game loads modded PAK with higher priority than base PAKs

# For Lua mods:
python tools\deploy.py mods          # Push mod scripts
python tools\deploy.py launch        # Restart game

# For combined (PAK + Lua):
# Deploy PAK manually via ADB, then:
python tools\deploy.py all           # Push modloader + mods
python tools\deploy.py launch
```

---

## 5. Rebuild Strategy

### 5.1 Strategy Comparison

| Aspect | DataTable-Only | Kismet Bytecode Patch | Lua Runtime Hooks | Hybrid (DataTable + Lua) |
|---|---|---|---|---|
| **Difficulty** | Easy | Very Hard | Medium | Medium |
| **Risk** | Low | High (one byte = crash) | Low | Low |
| **New Options** | ✅ Add to existing pages | ✅ Full control | ✅ Via ClearWidgets + CreateActiveOption | ✅ Both static + dynamic |
| **New Pages** | ❌ Can't add switch cases | ✅ Add cases to NewMenu | ✅ Hook NewMenu, build custom | ✅ Add enum values + hook |
| **New Actions** | ❌ Can't add DoAction cases | ✅ Add cases to DoAction | ✅ Hook InputActionConfirm + BLOCK | ✅ DataTable for display, Lua for logic |
| **New Settings** | ⚠️ Can add options but ProcessNewSetting won't handle them | ✅ Add cases | ✅ Hook ProcessNewSetting | ✅ DataTable for UI, Lua for application |
| **Layout Changes** | ❌ | ✅ Modify widget setup | ✅ Set properties at runtime | ✅ |
| **Persistence** | ✅ Saved in PAK | ✅ Saved in PAK | ❌ Must rebuild each session | ⚠️ Mixed |
| **Updateability** | Must re-extract on game update | Must re-patch on update | Survives game updates (usually) | Mixed |

### 5.2 Recommended Strategy: Hybrid (DataTable + Lua)

The optimal approach combines static DataTable modifications for option definitions with Lua runtime hooks for custom logic.

#### Phase 1: DataTable Foundation
1. **Add a "Mods" SubMenu option** to the Main Menu page (page 1) in `DebugMenuOptions`
   - `Menu_2 = 1`, `OptionName = "Mods"`, `OptionType = 3` (SubMenu), `MenuLink = 37` (unused enum value)
2. **Add mod option rows** to `DebugMenuOptions` for page 37
   - Each mod gets a row: `Menu_2 = 37`, `OptionName = "ModName"`, `OptionType = 1` (Bool), `OptionList = ["Off", "On"]`

#### Phase 2: Lua Runtime Extension
1. **Hook `NewMenu`** — When `ActiveMenu == 37` (or custom value), the DataTable rows provide the static options. Lua post-hook can add dynamic options (discovered mods not in DataTable)
2. **Hook `ProcessNewSetting`** — When the page is 37, intercept and handle mod toggle logic in Lua instead of letting the Blueprint's switch fall through
3. **Hook `DoAction`** — For custom action types on the mods page, handle in Lua and BLOCK original

#### Phase 3: UEnum Extension (Advanced)
Use `AppendEnumValue("DebugMenuType", "ModsPage", 99)` to add a custom enum value. Then:
- DataTable rows can reference `Menu_2 = 99`
- `NewMenu` switch won't match 99 → falls through to default (builds from DataTable)
- Lua hooks handle the rest

### 5.3 Architecture for Custom Mods Page

```lua
-- === DebugMenuAPI mod architecture ===

-- State
local MODS_PAGE = 99  -- custom enum value (or 37 if using existing)
local modsPageActive = false

-- Extend enum at load time
AppendEnumValue("DebugMenuType", "ModsPage", MODS_PAGE)

-- Hook NewMenu to detect our page
RegisterHook("/Game/.../DebugMenu_C:NewMenu", function(self, parms)
    -- Pre-hook: check if navigating to our page
    local menuType = ReadU8(parms)  -- first param is DebugMenuType enum
    if menuType == MODS_PAGE then
        modsPageActive = true
    else
        modsPageActive = false
    end
end, function(self)
    -- Post-hook: if our page, rebuild with mod options
    if modsPageActive then
        local dm = self:get()
        pcall(function() dm:Call("ClearWidgets") end)
        
        -- Set title
        local pw = dm:Get("ParentWidget")
        pcall(function() pw:Get("TitleText"):Set("Text", "MODS") end)
        
        -- Add mod entries
        for i, mod in ipairs(GetDiscoveredMods()) do
            pcall(function()
                dm:Call("CreateActiveOption", mod.name)
                local opts = dm:Get("ActiveOptionsWidgets")
                local widget = opts[#opts]
                -- Configure as toggle
                widget:Set("SettingList", {"Off", "On"})
                widget:Set("CurrentIndex", mod.enabled and 1 or 0)
                widget:Call("Setup")
            end)
        end
        
        -- Add "Back" option
        pcall(function() dm:Call("CreateActiveOption", "Back") end)
        
        -- Configure scrolling
        local vbl = pw:Get("DebugVBoxList")
        pcall(function() vbl:Set("MaxVisible", 15) end)
        pcall(function() vbl:Call("UpdateListView") end)
        pcall(function() dm:Call("UpdateOptionHighlight") end)
    end
end)

-- Hook InputActionConfirm to handle our page
RegisterHook("/Game/.../DebugMenu_C:InputActionConfirm", function(self, parms)
    if not modsPageActive then return end
    local pressed = ReadU8(parms)  -- BndEvt Pressed boolean
    if pressed == 0 then return end
    
    local dm = self:get()
    local idx = dm:Get("CurrentIndex")
    local opts = dm:Get("ActiveOptionsWidgets")
    local widget = opts[idx + 1]  -- 1-indexed
    local name = widget:Get("OptionName")
    
    if name == "Back" then
        pcall(function() dm:Call("PreviousMenu") end)
    else
        -- Toggle mod
        pcall(function() widget:Call("OptionIncremented") end)
        ToggleMod(name, widget:Get("CurrentIndex"))
    end
    
    return "BLOCK"  -- prevent original DoAction
end)
```

---

## 6. Specific Modifications Possible

### 6.1 Proven via Lua (No PAK Modification Required)

| Modification | Method | Difficulty | Verified |
|---|---|---|---|
| **Add custom MODS page** | Hook NewMenu + ClearWidgets + CreateActiveOption loop | Medium | ✅ Partially (individual calls verified) |
| **Add options to existing pages** | Post-hook NewMenu, add more CreateActiveOption calls | Easy | ✅ CreateActiveOption works |
| **Change MaxVisible** | `vbl:Set("MaxVisible", N)` | Easy | ✅ Confirmed |
| **Change selection** | `vbl:Set("Selection", N)` + `dm:Set("CurrentIndex", N)` | Easy | ✅ Confirmed |
| **Change scroll position** | `vbl:Set("FirstVisible", N)` | Easy | ✅ Confirmed |
| **Change page title** | `pw:Get("TitleText"):Set("Text", "New Title")` | Easy | ✅ FText write works |
| **Intercept confirm** | Pre-hook InputActionConfirm + return "BLOCK" | Easy | ✅ Pre-hook BLOCK works |
| **Intercept scrolling** | Pre-hook InputActionScrollUp/Down | Easy | ✅ Hookable |
| **Change option text** | `widget:Set("OptionName", "New Name")` + `widget:Call("Setup")` | Easy | ✅ FString + Setup works |
| **Highlight manipulation** | `dm:Call("UpdateOptionHighlight")` | Easy | ✅ Confirmed |
| **Trigger existing actions** | `dm:Call("NewMenu", pageId, false)` / `dm:Call("HideDebugMenu")` | Easy | ✅ Confirmed |
| **Read current state** | `dm:Get("ActiveMenu")`, `dm:Get("CurrentIndex")` | Easy | ✅ Confirmed |
| **Widget render translation** | `pw:Get("WidgetVBox"):Set("RenderTranslation", {X=0, Y=0})` | Easy | ✅ Struct Set works |
| **Widget draw size** | `widget:Call("SetDrawSize", {X=500, Y=2000})` | Easy | ✅ Table→struct works |

### 6.2 Possible via DataTable Modification (PAK Required)

| Modification | Method | Difficulty |
|---|---|---|
| **Add new options to any existing page** | Add rows to DebugMenuOptions with matching Menu_2 | Easy |
| **Add console command options** | Add row with OptionType=4, ConsoleCommand="cmd" | Easy |
| **Add SubMenu links** | Add row with OptionType=3, MenuLink=target page | Easy |
| **Change option setting lists** | Modify OptionList_19 array in row JSON | Easy |
| **Rename options** | Change OptionName_18 in row JSON | Easy |
| **Remove options** | Delete rows from DataTable JSON | Easy |
| **Add entries to DebugLevelList** | Add level select entries | Easy |
| **Add/modify favorites entries** | Edit DebugFavorites DataTable | Easy |

### 6.3 Possible via Kismet Bytecode Patching (PAK Required, High Risk)

| Modification | Method | Difficulty |
|---|---|---|
| **Add new switch case to NewMenu** | Insert bytecode case for new DebugMenuType value | Very Hard |
| **Add new action handler to DoAction** | Insert string comparison + action bytecode | Very Hard |
| **Add new setting handler to ProcessNewSetting** | Insert case for new Menu_2 + OptionName | Very Hard |
| **Change widget layout** | Modify Setup() or Construct() bytecode | Very Hard |
| **Change scroll behavior** | Modify SelectionIncremented/Decremented | Hard |
| **Change render translation offsets** | Find FVector2D constants in bytecode, change values | Medium |
| **Change MaxVisible defaults** | Find IntConst values in NewMenu switch, change | Medium |

### 6.4 Key Action Commands Available (from DoAction decompilation)

These are all the action commands that fire when OptionType=0 and the user confirms:

| OptionName String | Action | Page |
|---|---|---|
| `"Reload Level"` | `ReloadLevel()` | 8 (Debug Dev) |
| `"Load Game"` | `OpenLoadGameMenu()` + `HideDebugMenu()` | 8 |
| `"Restart"` | Restart game | 8 |
| `"Calibration"` | `OpenLevel("Calibration")` | 8 |
| `"Give 10k"` | `GivePesetas(10000)` | 9 (Cheats) |
| `"Heal Player"` | `HealPlayer()` | 9 |
| `"Hurt Player"` | `HurtPlayer()` | 9 |
| `"Heal Ashley"` | Heal Ashley | 9 |
| `"Hurt Ashley"` | Hurt Ashley | 9 |
| `"Kill All Active Enemies"` | `KillAllEnemies()` | 9 |
| `"Spawn Ashley"` | Spawn Ashley at player location | 9 |
| `"Fill"` | `MaxOutPlayerInventory()` | 9 |
| `"Clear"` | `DebugClearPlayerInventory()` | 9 |
| `"Cheat_GetAllFiles"` | Unlock all files | 9 |
| `"Launch End Game Sequence"` | `JumpToRoom("r333")` + end game | 8 |
| `"Clear Override Level"` | Clear debug level override, save | 8 |
| `"Reset All Game State Flags"` | `ResetScenarioAndRoomFlags()` | 8 |
| `"Clear All Achievements"` | `ClearAllAchievements()` | 26 |
| `"Clear Tutorial Progress"` | Clear tutorial flags | 8 |
| `"Clear Shortcuts"` | Clear saved shortcuts | 8 |
| `"Clear Notifications"` | Clear notification flags | 8 |
| `"Clear Action History"` | Clear action history | 9 |
| `"Toggle Costume Special 2 Unlock"` | Toggle costume unlock flag | 34 (Unlocks) |
| `"Toggle Chicago Typewriter Unlock"` | Toggle weapon unlock flag | 34 |
| *(Item enum names)* | `SpawnItem(enumValue, location)` | 14-23 (Inventory) |
| *(Level names)* | `JumpToRoom(levelName)` | 3-5, 32 (Level Select) |

### 6.5 Modification Recipes

#### Recipe 1: Add a Mod Toggle to Main Menu

```lua
-- In DebugMenuAPI/main.lua
RegisterHook("/Game/.../DebugMenu_C:ExecuteUbergraph_DebugMenu_C", function(self)
    -- After NewMenu builds page 1, add our option
    local dm = self:get()
    if dm:Get("ActiveMenu") ~= 1 then return end
    
    -- Post-process: insert before "Back"
    pcall(function()
        dm:Call("CreateActiveOption", ">>> MODS <<<")
    end)
    pcall(function()
        dm:Call("UpdateOptionHighlight")
    end)
end)
```

#### Recipe 2: Override Scroll Speed

```lua
-- Double the scroll repeat rate
RegisterHook("/Game/.../DebugMenu_C:InputActionScrollDown", function(self, parms)
    -- Modify the timer rate from 0.1 to 0.05 for faster scrolling
    -- (Must be done by manipulating timer properties or replacing scroll logic)
end)
```

#### Recipe 3: Custom Action on Confirm

```lua
RegisterHook("/Game/.../DebugMenu_C:InputActionConfirm", function(self, parms)
    local dm = self:get()
    local opts = dm:Get("ActiveOptionsWidgets")
    local idx = dm:Get("CurrentIndex")
    local widget = opts[idx + 1]
    if not widget then return end
    
    local name = widget:Get("OptionName")
    if name == ">>> MODS <<<" then
        -- Navigate to custom mods page
        dm:Call("NewMenu", MODS_PAGE, false)
        return "BLOCK"
    end
end)
```

#### Recipe 4: Inject Options into Existing Page via DataTable

```json
// Add to DebugMenuOptions.json (DataTable row)
{
    "Menu_2_FC14ABF34A3BEEF2A2BA5EB6F8817E07": 9,
    "OptionName_18_B5CB955F4DCD6593D3C178A7B5B6E209": "Super Speed",
    "OptionType_14_..hash..": 1,
    "OptionList_19_..hash..": ["Off", "On"],
    "MenuLink_22_..hash..": 0,
    "ConsoleCommand_25_..hash..": ""
}
// This adds "Super Speed" toggle to Cheats page (9)
// But ProcessNewSetting won't know how to handle it — needs Lua hook
```

#### Recipe 5: Full Custom Page via Hybrid Approach

1. **DataTable**: Add `Menu_2=37, OptionName="Mods", OptionType=3, MenuLink=37` row (SubMenu link on main page)
2. **DataTable**: Add rows for page 37 with mod names
3. **Lua**: Hook ProcessNewSetting to handle page 37 options
4. **Lua**: Hook DoAction for page 37 action items
5. **UEnum**: `AppendEnumValue("DebugMenuType", "ModsPage", 37)` for title display

---

## Appendix A: Property Quick Reference

### DebugMenu_C Properties (Key Subset)

| Property | Type | Access |
|---|---|---|
| `ActiveMenu` | DebugMenuType (byte/enum) | `dm:Get("ActiveMenu")` / `dm:Set("ActiveMenu", val)` |
| `CurrentIndex` | int32 | `dm:Get("CurrentIndex")` / `dm:Set("CurrentIndex", val)` |
| `ParentWidget` | DebugMenuWidget_C* | `dm:Get("ParentWidget")` |
| `ActiveOptionsWidgets` | TArray\<DebugOptionWidget_C\> | `dm:Get("ActiveOptionsWidgets")` |
| `ActiveDebugOptions` | TArray\<FName\> | `dm:Get("ActiveDebugOptions")` |
| `PreviousMenus` | TArray\<DebugMenuType\> | `dm:Get("PreviousMenus")` |
| `Player` | Bio4PlayerPawn* | `dm:Get("Player")` |
| `ShowAllItems` | bool | `dm:Get("ShowAllItems")` |
| `LevelSelectActive` | bool | `dm:Get("LevelSelectActive")` |
| `ActiveShortcut` | FName | `dm:Get("ActiveShortcut")` |
| `EcheckBool` | bool | Internal temp (enum check flag) |
| `OptionMenuType` | DebugMenuType | Internal temp (option's source page) |

### DebugMenuWidget_C Properties

| Property | Type | Access |
|---|---|---|
| `TitleText` | TextBlock* | `pw:Get("TitleText")` |
| `BuildVersion` | TextBlock* | `pw:Get("BuildVersion")` |
| `DebugVBoxList` | DebugVBoxList_C* | `pw:Get("DebugVBoxList")` |
| `WidgetVBox` | VerticalBox* | `pw:Get("WidgetVBox")` |

---

## Appendix B: Function Path Reference (for RegisterHook)

```
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:OpenDebugMenu
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:HideDebugMenu
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:NewMenu
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:DoAction
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:InputActionConfirm
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:InputActionBack
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:InputActionScrollUp
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:InputActionScrollDown
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:InputActionFavorite
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:InputActionSetShortcutAX
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:InputActionSetShortcutBY
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:InputActionDoShortcut
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:ProcessNewSetting
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:GetOptionIndex
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:UpdateOptionHighlight
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:ClearWidgets
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:CreateActiveOption
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:PreviousMenu
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:GetActiveOption
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:Initialize
/Game/Blueprints/DebugMenu/DebugMenu.DebugMenu_C:BuildTooltip
```

> **Note**: Exact paths may vary — verify with `FindSymbol()` or SDK dump. The `/Game/Blueprints/DebugMenu/` prefix is based on typical UE4 project structure for this game.

---

## Appendix C: DebugMenuType Enum — SDK Mapping

The SDK exports enum names as `NewEnumeratorN` which don't match their integer values:

| SDK Name | Integer Value | Meaning |
|---|---|---|
| NewEnumerator5 | 0 | Closed/None |
| NewEnumerator0 | 1 | Main Menu |
| NewEnumerator1 | 2 | Player Settings |
| NewEnumerator2 | 3 | Level Select Stage 1 |
| NewEnumerator3 | 4 | Level Select Stage 2 |
| NewEnumerator4 | 5 | Level Select Stage 3 |
| NewEnumerator6 | 6 | General Settings |
| NewEnumerator7 | 7 | Debug Visualization |
| NewEnumerator8 | 8 | Debug Dev |
| NewEnumerator9 | 9 | Cheats |
| NewEnumerator10 | 10 | *(unknown)* |
| NewEnumerator11 | 11 | Item Pickup |
| NewEnumerator12 | 12 | Particles |
| NewEnumerator19 | 13 | Pixel Density |
| NewEnumerator13 | 14 | Guns |
| NewEnumerator14 | 15 | Gun Mods |
| NewEnumerator15 | 16 | Grenades |
| NewEnumerator16 | 17 | Ammo |
| NewEnumerator17 | 18 | Consumables |
| NewEnumerator18 | 19 | Attaché Cases |
| NewEnumerator20 | 20 | Keys |
| NewEnumerator23 | 21 | Treasures 1 |
| NewEnumerator24 | 22 | Treasures 2 |
| NewEnumerator25 | 23 | Bottle Caps |
| NewEnumerator21 | 24 | *(unknown)* |
| NewEnumerator22 | 25 | Rendering |
| NewEnumerator26 | 26 | Achievements |
| NewEnumerator27 | 27 | Fog |
| NewEnumerator28 | 28 | All Options |
| NewEnumerator29 | 29 | Favorites |
| NewEnumerator30 | 30 | Sound |
| NewEnumerator31 | 31 | *(unknown)* |
| NewEnumerator32 | 32 | Level Select Stage 4 |
| NewEnumerator33 | 33 | Audio Debug |
| NewEnumerator34 | 34 | Unlocks |
| *(35-36 skipped)* | — | — |
| NewEnumerator37 | 37 | *(unused — candidate for custom page)* |

**Safe custom values**: 37 (exists but appears unused), or 99+ via `AppendEnumValue()`.
