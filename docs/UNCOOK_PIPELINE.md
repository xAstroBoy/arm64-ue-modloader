# Blueprint Uncook Pipeline — RE4 VR Debug Menu

> **Full pipeline**: Extract cooked Blueprint bytecode → Decompile Kismet → Modify assets via JSON → Repack into .pak → Modloader mounts at boot

---

## Architecture Overview

```
Stock PAK (RE4 VR)                    Override PAK (modded)
┌──────────────────────┐              ┌──────────────────────┐
│ DebugMenuOptions.uexp│  ──modify──→ │ DebugMenuOptions.uexp│ +Mods entry
│ DebugVBoxList.uexp   │  ──modify──→ │ DebugVBoxList.uexp   │ MaxVisible 5→20
│ DebugMenu.uexp       │  ──copy────→ │ DebugMenu.uexp       │ verbatim
│ ... (13 more)        │  ──copy────→ │ ... (13 more)        │ verbatim
└──────────────────────┘              └──────────────────────┘
                                             ↓ mounted at priority 1000+
                                      ┌──────────────────────┐
                                      │ pak_mounter.cpp       │
                                      │ (Dobby hooks Mount)   │
                                      └──────────────────────┘
                                             ↓
                                      ┌──────────────────────┐
                                      │ DebugMenuAPI v21      │
                                      │ PostHook NewMenu      │
                                      │ PostHook DoAction      │
                                      │ SharedAPI.DebugMenu    │
                                      └──────────────────────┘
```

---

## Step 1: Extract Stock PAK

The game's Blueprints are inside `pakchunk0-Android_ETC2.pak`. Extract with repak:

```bash
tools\repak\repak.exe unpack "path\to\pakchunk0-Android_ETC2.pak"
```

This produces `PAKS_extracted/VR4/Content/...` with all cooked assets.

Debug menu assets live at:
```
PAKS_extracted/VR4/Content/Blueprints/Debug/DebugMenu/
├── DebugMenu.uasset / .uexp          # Main actor (52 Kismet functions)
├── DebugMenuWidget.uasset / .uexp     # HUD widget (2 functions)
├── DebugVBoxList.uasset / .uexp       # Scrollable list (5 functions)
├── DebugOptionWidget.uasset / .uexp   # Per-option widget (7 functions)
├── DebugMenuOptions.uasset / .uexp    # DataTable (235 rows)
├── DebugMenuType.uasset / .uexp       # Enum (36 values)
├── DebugOptionType.uasset / .uexp     # Enum (5 values)
├── DebugMenuOption.uasset / .uexp     # Struct definition
├── DebugMenuInterface.uasset / .uexp  # Interface
├── DebugLevelList.uasset / .uexp      # Level select DataTable
├── DebugRoomKeys_DT.uasset / .uexp    # Room keys DataTable
├── Widget3DNoDepth*.uasset / .uexp    # 3D render materials
└── ... (struct/entry definitions)
```

---

## Step 2: Decompile Kismet Bytecode

UE4 Blueprints contain compiled **Kismet bytecode** — a stack-based VM instruction set. The `.uexp` file contains serialized `UBlueprintGeneratedClass` exports with `FKismetBytecode` arrays.

### Method A: Full decompiler pipeline (preferred)

```bash
# 1. Convert cooked .uasset → JSON (via UAssetGUI)
tools\bin\UAssetGUI.exe tojson DebugMenu.uasset DebugMenu.json VER_UE4_25

# 2. Decompile JSON → pseudo-code (via kismet_decompiler.py)
python tools\kismet_decompiler.py DebugMenu.json > DebugMenu.txt
```

### Method B: Direct binary parser

```bash
# Parse .uasset/.uexp directly → pseudo-code (via dump_uasset.py)
python tools\dump_uasset.py DebugMenu.uasset
```

### Decompiled Output

All decompiled pseudo-code lives in `PAKS_extracted/decompiled/`:

| File | Functions | Expressions | Description |
|------|-----------|-------------|-------------|
| `DebugMenu.txt` | 52 | ~4000+ | Main actor — NewMenu, DoAction, ClearWidgets, etc. |
| `DebugMenuWidget.txt` | 2 | 6 | Widget container — just Construct |
| `DebugVBoxList.txt` | 5 | 112 | Scroll list — AddWidget, UpdateListView, Selection±1 |
| `DebugOptionWidget.txt` | 7 | 123 | Per-option — Setup, UpdateLook, OptionIncremented |

---

## Step 3: Blueprint Analysis — Key Functions

### NewMenu (831 expressions)

The main page-building function. Called with a `byte` argument (page ID).

```
NewMenu(byte NewMenu, bool DontResetIndex):
  ClearWidgets()
  Array_AddUnique(PreviousMenus, NewMenu)
  self.ActiveMenu = NewMenu
  TitleText.SetText(GetEnumeratorUserFriendlyName(ActiveMenu))

  // Switch on ActiveMenu for WidgetVBox render position (36 cases)
  switch(ActiveMenu) {
    case 0..35: WidgetVBox.SetRenderTranslation(Vector2D(...))
  }

  // Clear arrays
  ActiveOptionList.Clear()
  ActiveOptionsWidgets.Clear()

  // Auto-add "Back" for pages != 0, 1, 28
  if (ActiveMenu != 0 && ActiveMenu != 1 && ActiveMenu != 28):
    CreateActiveOption("Back")

  // Page-specific item population:
  //   Pages 3,4,5,32: iterate DebugLevelList DataTable
  //   Pages 14-23: enumerate game item enums (EGunItem, etc.)
  //   Page 28: iterate DebugMenuOptions where Menu==28
  //   Page 29: iterate DebugFavorites
  //   Page 34: unlock toggles
  //   Generic: iterate DebugMenuOptions where Menu==ActiveMenu

  // Set MaxVisible per-page (switch on ActiveMenu)
  switch(ActiveMenu) {
    case 3: MaxVisible=10; case 4: MaxVisible=8; case 5: MaxVisible=8;
    case 32: MaxVisible=4; default: MaxVisible=0  // 0 = unlimited
  }

  UpdateListView()
  UpdateOptionHighlight()
```

**Key insight**: For unknown page bytes (100+), the switch falls through to the generic DataTable-driven path at the bottom. This means:
- Pages with no DataTable rows (like our custom byte 100) get just "Back" auto-added
- MaxVisible defaults to 0 (unlimited) for unknown pages
- Our Lua PostHook NewMenu fires AFTER and appends custom items

### CreateActiveOption (13 expressions)

```
CreateActiveOption(string Option):
  if (Option == "Count"): return  // skip magic row name
  widget = WBL::Create(self, DebugOptionWidget_C, None)
  widget.OptionName = Option
  widget.DebugOptionName = ToName(Option)
  slot = DebugVBoxList.AddWidget(widget)
  slot.SetHorizontalAlignment(2)  // Center
  ActiveOptionsWidgets.Add(widget)
  ActiveOptionList.Add(ToName(Option))
```

### DoAction (1609 expressions)

```
DoAction():
  GetActiveOption(CurrentOption)
  LocalOptionName = CurrentOption.OptionName
  if (!IsActive): return

  switch(ActiveMenu):
    case 1: // Main page
      switch(OptionMenuType from DataTable):
        Link: NewMenu(MenuLink)
        Setting: OptionIncremented()
        ...
    case 2..34: // Page-specific item handlers
      ...
  // Also handles shortcut dispatch (OptionMenuType second switch)
```

**Key insight for mods**: On the Main page (AM=1), the Blueprint looks up each option in the DataTable and dispatches based on `OptionMenuType`. Our "Mods" row uses `Setting` type with empty `SettingList`, so `OptionIncremented()` fires (no-op on empty list). Our PostHook DoAction then detects `OptionName == "Mods"` and navigates to page 100.

### DebugVBoxList — Scroll System

```
UpdateListView():
  for each child in ParentVBox:
    visible = (index >= FirstVisible && index < FirstVisible + MaxVisible)
    child.SetVisibility(visible ? SelfHitTestInvisible : Collapsed)
  UpArrow/DownArrow visibility based on total > MaxVisible
  Arrow color: white if can scroll, dim gray if at boundary

SelectionIncremented():
  if Selection == last: wrap to 0, FirstVisible=0
  else: Selection++; if >= FirstVisible+MaxVisible: FirstVisible++

SelectionDecremented():
  if Selection == 0: wrap to last, FirstVisible = total - MaxVisible
  else: Selection--; if < FirstVisible: FirstVisible--
```

### DebugOptionWidget — Display Modes

```
Setup():
  if SettingList.Length > 0:
    // "Setting" mode: show OptionName + " : " + CurrentSetting
    OptionNameText.SetText(OptionName)
    CurrentSettingText.SetText(" : " + SettingList[CurrentIndex])
    Switcher.SetActiveWidgetIndex(0)  // Setting layout
  else:
    // "Action" mode: show just ActionText
    ActionText.SetText(OptionName)
    Switcher.SetActiveWidgetIndex(1)  // Action layout

UpdateLook(Active, Favorite, Shortcut):
  // Active = red text, inactive = white
  // Favorite = show star icons
  // Shortcut = show controller button icons (A/B/X/Y)
```

---

## Step 4: Modify Assets

### What CAN be modified safely

| Asset Type | Tool | Safe Modifications |
|------------|------|--------------------|
| **DataTable** | UAssetGUI JSON roundtrip | Add/remove/modify rows |
| **CDO defaults** | UAssetGUI JSON roundtrip | Change property default values |
| **Enum** | UAssetGUI JSON roundtrip | Add/rename values (careful!) |
| **Widget properties** | UAssetGUI JSON roundtrip | Change sizes, colors, visibility |

### What CANNOT be modified

| Component | Reason |
|-----------|--------|
| **Kismet bytecode** | Compiled VM instructions — binary-incompatible if edited |
| **Widget hierarchy** | Adding/removing widgets breaks serialization dependencies |
| **Function signatures** | Blueprint calling convention baked into all callers |

### Current Modifications

#### 1. DebugMenuOptions DataTable (+1 row)

Adds `Main_Mods` row to the Main page (byte 1):

```json
{
  "Name": "Main_Mods",
  "Menu": "DebugMenuType::NewEnumerator0",      // Main page
  "OptionName": "Mods",
  "OptionType": "DebugOptionType::NewEnumerator0", // Setting (empty list)
  "OptionList": [],
  "MenuLink": "DebugMenuType::NewEnumerator5",    // None
  "ConsoleCommand": null
}
```

#### 2. DebugVBoxList CDO (MaxVisible 5→20)

The default `MaxVisible` is 5 — absurdly small for VR. Changed to 20 in the CDO:

```
Default__DebugVBoxList_C.MaxVisible: 5 → 20
```

This only affects pages where NewMenu doesn't explicitly set MaxVisible (i.e., custom pages 100+). All stock pages override it in the NewMenu switch.

---

## Step 5: Repack to PAK

```bash
# Build everything
python tools\build_debug_pak.py

# Or build + deploy to device
python tools\build_debug_pak.py --deploy
```

The builder:
1. Loads JSON dumps from `PAKS_extracted/json_dumps/`
2. Modifies DataTable + VBoxList CDO in memory
3. Writes modified JSON → temp file → `UAssetGUI fromjson` → .uasset/.uexp
4. Copies unmodified assets verbatim from extracted PAK
5. Packs all 32 files (16 .uasset + 16 .uexp) into `PAKS/DebugMenu_P.pak`

The `_P` suffix gives the PAK alphabetical priority over the stock `pakchunk0-*.pak`.

---

## Step 6: Modloader Mounts PAK

The C++ modloader (`modloader/src/pak/pak_mounter.cpp`, 965 lines) handles PAK mounting:

1. **Early hooks** — Before engine init, Dobby hooks `FPakPlatformFile::Mount` and `MountAllPakFiles`
2. **Instance capture** — First engine Mount call captures the `FPakPlatformFile*` instance
3. **Custom mount** — After engine mounts stock PAKs, immediately mounts custom PAKs from:
   ```
   /storage/emulated/0/Android/data/com.Armature.VR4/files/paks/
   ```
4. **Priority 1000+** — Custom PAKs override stock PAKs (stock uses priority ~0-10)

### Deployment

```bash
# Push PAK to device
adb push PAKS\DebugMenu_P.pak /storage/emulated/0/Android/data/com.Armature.VR4/files/paks/

# Or use deploy.py
python tools\deploy.py mods   # auto-deploys PAKs + Lua mods
```

---

## Step 7: Lua Cooperation

The `DebugMenuAPI v21` Lua mod cooperates with the PAK-injected "Mods" entry:

### Flow

```
1. Player opens Debug Menu → stock page 1 (Main) loads from DataTable
2. "Mods" appears as last item (from injected DataTable row)
3. Player confirms "Mods" → Blueprint's DoAction fires
4. DoAction: OptionType=Setting → OptionIncremented() (no-op, empty list)
5. PostHook DoAction: detects OptionName=="Mods" on page 1
6. PostHook: calls dm:Call("NewMenu", 100)
7. Blueprint: NewMenu(100) → ClearWidgets → auto-adds "Back" → falls through switch
8. PostHook NewMenu: detects ActiveMenu==100 → build_page() with custom items
9. Custom items appear with full Blueprint scroll/highlight/input support
```

### API for Other Mods

```lua
-- Register items on the Mods page
SharedAPI.DebugMenu.RegisterToggle("MyMod", "God Mode", getGodMode, setGodMode)
SharedAPI.DebugMenu.RegisterAction("MyMod", "Heal Player", healPlayer)
SharedAPI.DebugMenu.RegisterSelector("MyMod", "Speed", {"1x","2x","4x"}, onSpeedChange)
SharedAPI.DebugMenu.RegisterSubMenu("MyMod", "Settings", onEnterSettings)
```

---

## DataTable Reference

### DebugMenuOptions Structure

| Field | Type | FName Suffix | Description |
|-------|------|--------------|-------------|
| Menu | ByteProperty (DebugMenuType) | `_2_FC14ABF3...` | Which page this row appears on |
| OptionName | StrProperty | `_18_B5CB955F...` | Display text |
| OptionType | ByteProperty (DebugOptionType) | `_14_FC7672FD...` | Setting/Link/Toggle/Select/Command |
| OptionList | ArrayProperty\<Str\> | `_19_50B4C553...` | Setting cycle values |
| MenuLink | ByteProperty (DebugMenuType) | `_22_B72C8153...` | Target page for Link type |
| ConsoleCommand | StrProperty | `_25_F5046605...` | Console command for Command type |

### DebugOptionType Values (in NameMap)

| FName | Value | Behavior |
|-------|-------|----------|
| `NewEnumerator0` | Setting | Cycles through OptionList on confirm |
| `NewEnumerator1` | Link | Navigates to MenuLink page |
| `NewEnumerator3` | Toggle | Toggles a boolean |
| `NewEnumerator5` | Select | Selection from list |
| `NewEnumerator7` | Command | Executes ConsoleCommand |

### DebugMenuType Pages (byte values)

| Byte | Enum | Page Name |
|------|------|-----------|
| 0 | NewEnumerator5 | None |
| 1 | NewEnumerator0 | Main/Debug Menu |
| 2 | NewEnumerator1 | Difficulty |
| 3-5 | built-in | Stage 1/2/3 Level Select |
| 6 | NewEnumerator2 | Sounds |
| 7 | NewEnumerator3 | Rendering |
| 8 | NewEnumerator4 | Gameplay |
| 9 | NewEnumerator6 | Player |
| 10 | NewEnumerator7 | Items |
| 11 | built-in | Pickup Item Quick Menu / All Items |
| 12 | NewEnumerator8 | Misc |
| 13 | NewEnumerator9 | Cheats |
| 14-23 | built-in | Item type enums (guns, ammo, etc.) |
| 24 | NewEnumerator10 | 1P Options |
| 25 | NewEnumerator11 | Enemy |
| 26 | built-in | Achievements |
| 27 | NewEnumerator12 | Effects |
| 28 | NewEnumerator13 | Option Menu |
| 29 | built-in | Favorites |
| 30 | NewEnumerator14 | Profiles |
| 31 | NewEnumerator15 | AI |
| 32 | built-in | Stage Select |
| 33 | NewEnumerator16 | Streaming |
| 34 | built-in | Unlocks |
| **100+** | **none (Lua)** | **Custom mod pages** |

---

## Tools Reference

| Tool | Location | Purpose |
|------|----------|---------|
| `repak.exe` | `tools/repak/` | PAK pack/unpack (V4-V11) |
| `UAssetGUI.exe` | `tools/bin/` | .uasset ↔ JSON conversion |
| `kismet_decompiler.py` | `tools/` | UAssetGUI JSON → pseudo-code |
| `dump_uasset.py` | `tools/` | Direct .uasset → pseudo-code |
| `build_debug_pak.py` | `tools/` | Full build pipeline (this doc) |
| `deploy.py` | `tools/` | ADB push mods/PAKs/modloader |

---

## Troubleshooting

### UAssetGUI "dummy FName" error
The `NameMap` array in the JSON contains ALL FNames used by the asset. When adding new data that references FNames not in the NameMap, you MUST append them first. Only use enum values that already exist (e.g., `DebugOptionType` values 0/1/3/5/7 — NOT 2/4/6).

### PAK not loading
- Check mount priority: PAKs named `*_P.pak` get higher priority
- Verify PAK path: `/storage/emulated/0/Android/data/com.Armature.VR4/files/paks/`
- Check modloader log: `python tools\deploy.py log` → search for `[PAK-EARLY]`

### Modified asset crashes game
- UAssetGUI roundtrip isn't lossless for all asset types
- Always keep unmodified assets as verbatim copies from stock
- Test with `--dry-run` first, then build and verify sizes match stock
