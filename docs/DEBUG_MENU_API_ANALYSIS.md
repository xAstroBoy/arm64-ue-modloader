# RE4 VR Debug Menu — Deep API Analysis

> Complete analysis of every property and function in the debug menu class hierarchy.  
> Read this BEFORE writing any debug menu mod code.

---

## Table of Contents
1. [DebugMenu_C (Main Actor)](#1-debugmenu_c)
2. [DebugMenuWidget_C (Widget Blueprint)](#2-debugmenuwidget_c)
3. [DebugVBoxList_C (Scrollable List)](#3-debugvboxlist_c)
4. [DebugOptionWidget_C (Option Entry)](#4-debugoptionwidget_c)
5. [DebugMenuInterface_C (Input Interface)](#5-debugmenuinterface_c)
6. [Object Hierarchy & Access Patterns](#6-object-hierarchy)
7. [What Works, What Doesn't](#7-what-works-what-doesnt)

---

## 1. DebugMenu_C

**Parent**: `VR4DebugScreenActor`  
**Access**: `FindFirstOf("DebugMenu_C")`  
**SDK File**: `Current Modloader SDK/Classes/DebugMenu_C.lua` (2787 lines)

### Properties

| Property | Type | Offset | Description |
|---|---|---|---|
| `Widget` | WidgetComponent | 0x0228 | **The 3D widget component** — renders the menu in world space. Has `DrawSize` (IntPoint). |
| `Root` | SceneComponent | 0x0230 | Root scene component |
| `Player` | VR4Bio4PlayerPawn | 0x0238 | Reference to player pawn |
| `ActiveOptionList` | TArray\<FName\> | 0x0240 | **Current menu option names** (FNames). Length = number of options in current menu page. |
| `ActiveOptionsWidgets` | TArray\<UObject*\> | 0x0250 | **Current option widget instances** (DebugOptionWidget_C). 1:1 with ActiveOptionList. |
| `CurrentIndex` | int32 | 0x0260 | **Currently highlighted option index** (0-based). Write this to move highlight. |
| `ActiveMenu` | uint8 | 0x0264 | **Current menu page ID**. Stock values: 0=closed?, 1=main, 2+=submenus. We use 99 for MODS. |
| `BoolStrings` | TArray\<FString\> | 0x0268 | Pre-built strings for bool display ("On"/"Off" etc) |
| `ParentWidget` | DebugMenuWidget_C | 0x0278 | **The widget blueprint instance** — gives access to DebugVBoxList, TitleText, etc. |
| `LevelSelectActive` | bool | 0x0280 | Whether level select submenu is active |
| `Stage1Map` / `Stage2Map` / `Stage3Map` | ObjectProperty | 0x0288-0x0298 | Map widget references |
| `ScrollTimer` | TimerHandle | 0x02A0 | Timer handle for continuous scroll |
| `HackLevelName` | FName | 0x02A8 | Level name for debug teleport |
| `PIckupItemQuickMenu` | TArray\<enum\> | 0x02B0 | Item pickup enum list |
| `ShowAllItems` | bool | 0x02C0 | Whether to show all items in menu |
| `PreviousMenus` | TArray\<uint8\> | 0x02C8 | **Menu navigation stack** — pushed when entering submenus, popped on Back. |
| `FogIsEnabled` | bool | 0x02D8 | Fog toggle state |
| `DebugSettings` | VR4DebugSettings | 0x02E0 | **Struct** — game debug settings (struct userdata, NOT indexable) |
| `FinishGameDifficulty` | uint8 | 0x0380 | Difficulty setting |
| `Favorites` | TArray\<FName\> | 0x0388 | Favorited option names |
| `DoubleClickTimer` | TimerHandle | 0x0398 | Double-click detection timer |
| `OptionMenuType` | uint8 | 0x03A0 | Type of current option menu |
| `ActiveShortcut` | FName | 0x03A4 | Currently active shortcut |
| `EcheckBool` | bool | 0x03AC | Internal check flag |
| `FavShowAllItems` | bool | 0x03AD | Show all items in favorites view |
| `UnlockTooltipText` | TMap\<FName, FString\> | 0x03B0 | Tooltip text map |

### Functions — Core Menu Operations

#### `ClearWidgets()`
- **What it does**: Iterates `ActiveOptionsWidgets`, removes/destroys each `DebugOptionWidget_C`, then clears both `ActiveOptionsWidgets` and `ActiveOptionList` arrays.
- **Side effects**: Empties the VBoxList visually. The menu becomes blank.
- **Call from Lua**: `dm:Call("ClearWidgets")` ✅ WORKS
- **Use case**: Call before populating a custom menu page.

#### `CreateActiveOption(Option: FString)`
- **What it does**: Creates a new `DebugOptionWidget_C`, sets its `DebugOptionName` from the string, adds it to `ActiveOptionList` (as FName) and `ActiveOptionsWidgets`, and adds the widget to the VBoxList's VerticalBox.
- **Parameter**: `Option` — the display name string for the option.
- **Call from Lua**: `dm:Call("CreateActiveOption", "MyOption")` ✅ WORKS
- **Use case**: Add individual options to a custom menu page after ClearWidgets.

#### `UpdateOptionHighlight()`
- **What it does**: Iterates `ActiveOptionsWidgets`, calls `UpdateLook(Active, Favorite, Shortcut)` on each widget. The widget at `CurrentIndex` gets `Active=true`, others get `Active=false`. Also handles favorite and shortcut visual indicators.
- **Side effects**: Updates visual highlighting only. Does NOT change `CurrentIndex`.
- **Call from Lua**: `dm:Call("UpdateOptionHighlight")` ✅ WORKS
- **Use case**: Call after changing `CurrentIndex` to update visuals.

#### `NewMenu(NewMenu: uint8, DontResetIndex: bool)`
- **What it does**: The MAIN menu navigation function. Calls `ClearWidgets()`, sets `ActiveMenu` to the new page ID, then builds the options for that page from data tables. If `DontResetIndex` is false, resets `CurrentIndex` to 0.
- **Parameters**: `NewMenu` = page ID (uint8), `DontResetIndex` = whether to keep current selection.
- **IMPORTANT**: This builds from the game's DataTable — it does NOT know about our custom MODS page. Calling `NewMenu(99)` would build an empty/broken page.
- **Call from Lua**: `dm:Call("NewMenu", pageId, false)` — use for stock pages only.
- **Use case**: Navigate to stock menu pages. Do NOT use for custom pages.

#### `PreviousMenu()`
- **What it does**: Pops the last entry from `PreviousMenus` TArray and calls `NewMenu()` with that value.
- **Call from Lua**: `dm:Call("PreviousMenu")` ✅ WORKS
- **Use case**: Go back in menu hierarchy.

#### `GetActiveOption() → DebugMenuOption`
- **What it does**: Returns a `DebugMenuOption` struct for the currently selected option (at `CurrentIndex`). Looks up the option name in a DataTable.
- **Return**: Struct (raw userdata — NOT indexable from Lua)
- **Call from Lua**: `dm:Call("GetActiveOption")` — returns struct userdata.
- **WARNING**: Return value is struct userdata, you can't read fields from it.

#### `DoAction(...)`
- **What it does**: The MAIN "confirm" handler. Processes the currently selected option based on its DataTable entry. Handles all stock menu actions (level select, settings changes, teleports, etc.).
- **IMPORTANT**: This is the game's confirm logic — it only works for stock options that have DataTable entries. Custom MODS page options won't match anything.
- **Call from Lua**: Generally don't call this for custom pages.

### Functions — Input Handlers

#### `InputActionScrollUp(Pressed: bool)` / `InputActionScrollDown(Pressed: bool)`
- **What they do**: Called when VR controller scroll input fires. Increments/decrements `CurrentIndex` (clamped to valid range), calls `SelectionIncremented()`/`SelectionDecremented()` on VBoxList, calls `UpdateOptionHighlight()`.
- **Parameter**: `Pressed` — true on button down, false on button up.
- **Call from Lua**: `dm:Call("InputActionScrollUp", true)` / `dm:Call("InputActionScrollDown", true)` ✅ WORKS
- **IMPORTANT**: These call the game's scroll logic. They read `ActiveOptionList` to determine bounds.

#### `InputActionConfirm(Pressed: bool)`
- **What it does**: On press, calls `DoAction()` for the currently highlighted option.
- **Call from Lua**: `dm:Call("InputActionConfirm", true)` — triggers the stock confirm logic.

#### `InputActionBack(Pressed: bool)`
- **What it does**: On press, calls `PreviousMenu()` to go back one level in the menu stack.
- **Call from Lua**: `dm:Call("InputActionBack", true)` — triggers stock back logic.

#### `InputActionFavorite(Pressed: bool)`
- **What it does**: Toggles favorite status for the currently selected option.

#### `InputActionSetShortcutAX()` / `InputActionSetShortcutBY()` / `InputActionDoShortcut(AX: bool)`
- **What they do**: Shortcut binding system for quick-access actions.

### Functions — Scroll System

#### `StartScrollDown()` / `StartScrollUp()`
- **What they do**: Initiate continuous scrolling. Set up a timer (`ScrollTimer`) that repeatedly calls `TriggerScrollDown()`/`TriggerScrollUp()`.

#### `TriggerScrollDown()` / `TriggerScrollUp()`
- **What they do**: Single scroll step. Move `CurrentIndex` by 1, update VBoxList and highlight.

#### `ContinueScrollDown()` / `ContinueScrollUp()`
- **What they do**: Continue continuous scrolling (called by timer).

#### `TimerExpired()`
- **What it does**: Scroll timer callback — stops continuous scroll when button released.

### Functions — Menu Page Builders

These are stock page builders — each populates the menu for a specific game page:
- `DrawMercenariesUnlocks()` — Mercenaries mode unlocks
- `DrawCharacterUnlocks()` — Character unlocks
- `DrawAchievements()` — Achievement list
- `MaxOutPlayerInventory()` — Max inventory items
- `RefreshInventory()` — Refresh inventory display
- `RefreshKeysTreasures()` — Refresh keys/treasures
- `AddTreasureItems(Start, End)` — Add treasure item range

### Functions — Settings Processors

Called by `DoAction()` when confirm is pressed on a settings option:
- `ProcessUnlocks(OptionName)` — Toggle unlock state
- `ProcessCutsceneSetting(NewSetting)` — Change cutscene mode
- `ProcessFinishGameDifficulty(NewSetting)` — Change difficulty
- `ProcessNewRaiseWeaponSetting(Setting)` — Weapon raise mode
- `ProcessPixelDensitySetting(NewSetting)` — Pixel density
- `ProcessParticleSetting(NewSetting)` — Particle effects
- `ProcessFogSetting(NewSetting)` — Fog settings
- `GetRenderingOptionIndex(Index)` — Get current rendering option index

### Functions — Lifecycle
- `ReceiveBeginPlay()` — Blueprint BeginPlay event
- `IsActive(Active)` — Check/set menu visibility
- `OpenDebugMenu(Callback)` — Show the debug menu
- `HideDebugMenu(Callback)` — Hide the debug menu
- `ExecuteUbergraph_DebugMenu(EntryPoint)` — Nativized blueprint graph (do NOT call)

---

## 2. DebugMenuWidget_C

**Parent**: `UserWidget`  
**Access**: `dm:Get("ParentWidget")`  
**SDK File**: `Current Modloader SDK/Classes/DebugMenuWidget_C.lua`

### Properties

| Property | Type | Offset | Description |
|---|---|---|---|
| `UberGraphFrame` | PointerToUberGraphFrame | 0x0230 | Internal blueprint state |
| `BuildVersion` | TextBlock | 0x0238 | Build version text display widget |
| `DebugVBoxList` | DebugVBoxList_C | 0x0240 | **THE scrollable list widget** — this is how you access VBoxList |
| `ScaleBox_MapS1/S2/S3` | ScaleBox | 0x0248-0x0258 | Scale boxes for map displays |
| `TitleText` | TextBlock | 0x0260 | **Menu title text** — displayed at top of menu |
| `WidgetVBox` | VerticalBox | 0x0268 | Root vertical box container |

### Functions

| Function | Description |
|---|---|
| `Construct()` | Widget construct event (called by engine when widget is created) |
| `ExecuteUbergraph_DebugMenuWidget(EntryPoint)` | Internal graph (do NOT call) |

### Analysis
This is a simple container widget. Its main purpose is to hold:
1. `DebugVBoxList` — the scrollable option list
2. `TitleText` — the page title
3. `WidgetVBox` — the root layout container

**Access pattern**:
```lua
local dm = FindFirstOf("DebugMenu_C")
local pw = dm:Get("ParentWidget")       -- DebugMenuWidget_C
local vbl = pw:Get("DebugVBoxList")     -- DebugVBoxList_C
local title = pw:Get("TitleText")       -- TextBlock
```

---

## 3. DebugVBoxList_C

**Parent**: `UserWidget`  
**Access**: `pw:Get("DebugVBoxList")`  
**SDK File**: `Current Modloader SDK/Classes/DebugVBoxList_C.lua`

### Properties

| Property | Type | Offset | Description |
|---|---|---|---|
| `DownArrow` | Image | 0x0230 | Down scroll arrow indicator |
| `ParentVBox` | VerticalBox | 0x0238 | The UMG VerticalBox that holds child option widgets |
| `UpArrow` | Image | 0x0240 | Up scroll arrow indicator |
| `MaxVisible` | int32 | 0x0248 | **Max visible items at once** — stock = 5. Set to 50 to show all. |
| `FirstVisible` | int32 | 0x024C | **Index of first visible item** — for scrolling window. 0 = top. |
| `Selection` | int32 | 0x0250 | **Currently selected item index** within the VBoxList — 0-based. |

### Functions

#### `SelectionIncremented()`
- **What it does**: Moves selection DOWN by 1. Internally:
  1. Gets all children of `ParentVBox` via `GetAllChildren()`
  2. Checks if `Selection + 1` would exceed child count
  3. If within bounds: increments `Selection`
  4. If `Selection >= FirstVisible + MaxVisible`: scrolls down (increments `FirstVisible`)
  5. Calls `UpdateListView()` internally
- **Call from Lua**: `vbl:Call("SelectionIncremented")` ✅ WORKS
- **Side effects**: Modifies `Selection`, may modify `FirstVisible`, updates visibility.

#### `SelectionDecremented()`
- **What it does**: Moves selection UP by 1. Internally:
  1. Gets all children of `ParentVBox`
  2. Checks if `Selection - 1` would go below 0
  3. If within bounds: decrements `Selection`
  4. If `Selection < FirstVisible`: scrolls up (decrements `FirstVisible`)
  5. Calls `UpdateListView()` internally
- **Call from Lua**: `vbl:Call("SelectionDecremented")` ✅ WORKS
- **Side effects**: Modifies `Selection`, may modify `FirstVisible`, updates visibility.

#### `UpdateListView()`
- **What it does**: The master visibility/scroll update function. Iterates ALL children of `ParentVBox`:
  1. Items in range `[FirstVisible, FirstVisible + MaxVisible)` → set Visible
  2. Items outside range → set Hidden/Collapsed
  3. Updates `UpArrow` visibility (hidden if `FirstVisible == 0`)
  4. Updates `DownArrow` visibility (hidden if all items visible)
  5. Sets color/highlight based on `Selection` index
- **Call from Lua**: `vbl:Call("UpdateListView")` ✅ WORKS
- **Use case**: Call after manually setting `FirstVisible`/`Selection`/`MaxVisible` to refresh the display.

#### `ClearWidgets()`
- **What it does**: Removes all children from `ParentVBox`.
- **Call from Lua**: `vbl:Call("ClearWidgets")` ✅ WORKS (but prefer `dm:Call("ClearWidgets")` which also clears the arrays)

#### `AddWidget(Widget)`
- **What it does**: Creates a `DebugOptionWidget_C` and adds it to `ParentVBox`.
- **Call from Lua**: `vbl:Call("AddWidget", widgetObj)` — requires a valid Widget object.
- **Note**: `dm:Call("CreateActiveOption", name)` is easier — it handles widget creation AND array management.

### IMPORTANT: Selection vs CurrentIndex
- `DebugVBoxList_C.Selection` = the VBoxList's own selection tracking
- `DebugMenu_C.CurrentIndex` = the DebugMenu's selection tracking
- **These MUST be kept in sync!** When scrolling:
  1. Set both `dm.CurrentIndex` and `vbl.Selection` to the same value
  2. Set `vbl.FirstVisible` appropriately for the scroll window
  3. Call `vbl:Call("UpdateListView")` to refresh visibility
  4. Call `dm:Call("UpdateOptionHighlight")` to refresh highlighting

---

## 4. DebugOptionWidget_C

**Parent**: `UserWidget`  
**Access**: `dm:Get("ActiveOptionsWidgets")[i]` (1-indexed in Lua)  
**SDK File**: `Current Modloader SDK/Classes/DebugOptionWidget_C.lua`

### Properties

| Property | Type | Offset | Description |
|---|---|---|---|
| `ActionText` | TextBlock | 0x0238 | Action/label text widget |
| `Button0` / `Button1` | SizeBox | 0x0240/0x0248 | Button containers |
| `CurrentSettingText` | TextBlock | 0x0250 | Current setting value display |
| `Favorite0` / `Favorite1` | SizeBox | 0x0258/0x0260 | Favorite indicator containers |
| `Image_Favorite` / `Image_Favorite0` | Image | 0x0268/0x0270 | Favorite star images |
| `OptionNameText` | TextBlock | 0x0278 | **The option name text display** |
| `Switcher` | WidgetSwitcher | 0x0280 | Active/inactive visual switcher |
| `WidgetSwitcherButton0/1` | WidgetSwitcher | 0x0288/0x0290 | Button state switchers |
| `OptionName` | FString | 0x0298 | **The option name string** |
| `SettingList` | TArray\<FString\> | 0x02A8 | Possible setting values for cycling |
| `CurrentIndex` | int32 | 0x02B8 | Current setting index (for multi-value options) |
| `DebugOptionName` | FName | 0x02BC | **The option FName** — used for DataTable lookups |

### Functions

#### `UpdateLook(Active: bool, Favorite: bool, Shortcut: uint8)`
- **What it does**: Updates the widget's visual appearance:
  - `Active=true` → highlighted color (selected)
  - `Active=false` → normal color (unselected)
  - `Favorite` → shows/hides favorite star
  - `Shortcut` → shows shortcut indicator (0=none, 1=AX, 2=BY)
- **Call from Lua**: `optionWidget:Call("UpdateLook", true, false, 0)` — set active, no favorite, no shortcut.
- **Use case**: Called by `UpdateOptionHighlight()` automatically. Can call manually for custom styling.

#### `Setup()`
- **What it does**: Initializes the widget display. Sets `OptionNameText` from `OptionName`, sets `CurrentSettingText` from `SettingList[CurrentIndex]`.
- **Call from Lua**: `optionWidget:Call("Setup")` — re-initializes display.

#### `OptionReset(NewSetting: FString)`
- **What it does**: Resets the option to a specific setting value. Updates `CurrentIndex` and `CurrentSettingText`.

#### `OptionIncremented(NewSetting: FString)`
- **What it does**: Cycles the option to the next setting. Increments `CurrentIndex`, wraps around via modulo, updates display.
- **Call from Lua**: `optionWidget:Call("OptionIncremented", "")` — cycle to next setting.

#### `Construct()` / `OnInitialized()`
- Widget lifecycle events. Called by engine.

#### `ExecuteUbergraph_DebugOptionWidget(EntryPoint)`
- Internal graph. Do NOT call.

---

## 5. DebugMenuInterface_C

**Parent**: `Interface`  
**SDK File**: `Current Modloader SDK/Classes/DebugMenuInterface_C.lua`

This is an **interface definition** — it defines the function signatures that `DebugMenu_C` implements. These are the functions that the `VR4PlayerController_BP_C` calls via interface dispatch.

### Interface Functions
| Function | Parameters | Description |
|---|---|---|
| `InputActionScrollUp` | `(Pressed: bool)` | Scroll up input |
| `InputActionScrollDown` | `(Pressed: bool)` | Scroll down input |
| `InputActionConfirm` | `(Pressed: bool)` | Confirm/select input |
| `InputActionBack` | `(Pressed: bool)` | Back/cancel input |
| `InputActionFavorite` | `(Pressed: bool)` | Toggle favorite |
| `InputActionSetShortcutAX` | `()` | Set AX shortcut |
| `InputActionSetShortcutBY` | `()` | Set BY shortcut |
| `InputActionDoShortcut` | `(AX: bool)` | Execute shortcut |
| `IsActive` | `(Active: bool)` | Check/set active state |
| `OpenDebugMenu` | `(Callback: bool)` | Open menu |
| `HideDebugMenu` | `(Callback: bool)` | Close menu |

---

## 6. Object Hierarchy

```
DebugMenu_C (Actor in world)
├── Widget (WidgetComponent) ← renders in 3D space, has DrawSize
├── Root (SceneComponent)
├── ParentWidget → DebugMenuWidget_C (UserWidget)
│   ├── DebugVBoxList → DebugVBoxList_C (UserWidget)
│   │   ├── ParentVBox (VerticalBox) ← holds option widgets
│   │   ├── UpArrow (Image)
│   │   └── DownArrow (Image)
│   ├── TitleText (TextBlock) ← menu page title
│   ├── BuildVersion (TextBlock)
│   └── WidgetVBox (VerticalBox) ← root layout
├── ActiveOptionList (TArray<FName>) ← option names
├── ActiveOptionsWidgets (TArray<UObject*>) ← option widget instances
│   └── [i] → DebugOptionWidget_C (UserWidget)
│       ├── OptionNameText (TextBlock)
│       ├── CurrentSettingText (TextBlock)
│       ├── OptionName (FString)
│       ├── SettingList (TArray<FString>)
│       └── DebugOptionName (FName)
├── CurrentIndex (int32) ← highlighted option
├── ActiveMenu (uint8) ← current page
└── PreviousMenus (TArray<uint8>) ← nav stack
```

### Access Pattern from Lua:
```lua
local dm = FindFirstOf("DebugMenu_C")
local widget = dm:Get("Widget")              -- WidgetComponent (3D renderer)
local pw = dm:Get("ParentWidget")            -- DebugMenuWidget_C
local vbl = pw:Get("DebugVBoxList")          -- DebugVBoxList_C
local title = pw:Get("TitleText")            -- TextBlock
local optionList = dm:Get("ActiveOptionList")     -- TArray<FName>
local optionWidgets = dm:Get("ActiveOptionsWidgets") -- TArray<UObject*>
local optWidget = optionWidgets[1]           -- First DebugOptionWidget_C (1-indexed!)
```

---

## 7. What Works, What Doesn't

### ✅ Confirmed Working (Bridge-Tested)

| Operation | Code | Result |
|---|---|---|
| Read int32 property | `dm:Get("CurrentIndex")` | Returns number ✅ |
| Write int32 property | `dm:Set("CurrentIndex", 3)` | Sets value ✅ |
| Read uint8 property | `dm:Get("ActiveMenu")` | Returns number ✅ |
| Write uint8 property | `dm:Set("ActiveMenu", 99)` | Sets value ✅ |
| Read object property | `dm:Get("ParentWidget")` | Returns UObject ✅ |
| Read TArray | `dm:Get("ActiveOptionsWidgets")` | Returns TArray ✅ |
| TArray length | `#arr` | Returns count ✅ |
| TArray index | `arr[1]` | Returns element ✅ (1-indexed) |
| Call no-arg function | `dm:Call("ClearWidgets")` | Executes ✅ |
| Call with string arg | `dm:Call("CreateActiveOption", "Test")` | Creates widget ✅ |
| Call VBoxList functions | `vbl:Call("SelectionIncremented")` | Selection 0→1 ✅ |
| Set VBoxList properties | `vbl:Set("MaxVisible", 50)` | 5→50 confirmed ✅ |
| Set VBoxList Selection | `vbl:Set("Selection", 3)` | Direct write ✅ |
| Set VBoxList FirstVisible | `vbl:Set("FirstVisible", 0)` | Direct write ✅ |
| UpdateListView | `vbl:Call("UpdateListView")` | Refreshes ✅ |
| UpdateOptionHighlight | `dm:Call("UpdateOptionHighlight")` | Refreshes ✅ |
| RegisterHook on BndEvt | `RegisterHook("VR4PlayerController_BP_C:BndEvt__...", fn)` | Hooks fire ✅ |
| Pre-hook BLOCK | `return "BLOCK"` in pre-hook | Blocks original ✅ |
| ReadU8 on hook parms | `ReadU8(parms)` for Pressed bool | Returns 0/1 ✅ |

### ❌ Confirmed BROKEN (Do Not Use)

| Operation | Code | Problem |
|---|---|---|
| Call with table struct arg | `widget:Call("SetDrawSize", {X=500, Y=2000})` | **ZEROES to 0,0** ❌ |
| Set struct property | `widget:Set("DrawSize", {X=500, Y=2000})` | **No effect or zeroes** ❌ |
| Index struct userdata | `ds.X` where `ds = widget:Get("DrawSize")` | **Raw userdata, no metatable** ❌ |
| Read struct fields | `widget:Get("DrawSize").X` | **Cannot chain — raw userdata** ❌ |

### ⚠️ Untested / Use With Caution

| Operation | Code | Notes |
|---|---|---|
| NewMenu for custom page | `dm:Call("NewMenu", 99, false)` | Will try to build from DataTable — no entry for 99 |
| PreviousMenu after custom | `dm:Call("PreviousMenu")` | Should work if PreviousMenus was properly populated |
| GetActiveOption return | `dm:Call("GetActiveOption")` | Returns struct userdata — can't read fields |
| DoAction for custom options | `dm:Call("DoAction")` | Won't match custom option names in DataTable |
| TitleText Set | `title:Set("Text", FText("MODS"))` | Untested — may need SetText() call instead |

### Recommended Approach for Custom MODS Page

1. **Enter MODS page**: `dm:Call("ClearWidgets")` → loop `dm:Call("CreateActiveOption", name)` → set `ActiveMenu=99`
2. **Scroll**: Set `CurrentIndex`, `Selection`, `FirstVisible` → call `UpdateListView()` + `UpdateOptionHighlight()`
3. **Confirm**: Read `CurrentIndex` → match against your mod list → execute action
4. **Back**: Set `ActiveMenu` back to stock value → call `NewMenu(stockPageId, false)` to rebuild stock page
5. **DO NOT** touch DrawSize — there is no working way to set it from Lua
