# RE4 VR Debug Menu — Redesign Architecture

> Complete takeover of the stock Debug Menu via PAK + Lua API

---

## Overview

The redesigned debug menu has **two layers**:

1. **PAK Layer** — Modifies the stock Blueprint assets (enum + DataTable) to add a "Mods" submenu link on the Main page. The Blueprint's own generic confirm handler reads `OptionType=Link` and automatically calls `NewMenu(ModsPage)`. Zero Kismet bytecode edits needed.

2. **Lua Layer** — Hooks `NewMenu` (post) to dynamically populate the Mods page with registered mod entries. Hooks `BndEvt` (pre+post) to intercept confirm actions on the Mods page and handle them in Lua. All other input (scroll, back, favorites) is handled by the Blueprint natively.

---

## Key Discovery: Generic Confirm Handler

The Ubergraph's `InputActionConfirm` handler has a **generic OptionType dispatch** that works for ALL menus:

```
GetActiveOption(currentOption)            // reads from DataTable
switch (currentOption.OptionType):
  case 0 (Setting):  OptionIncremented → ProcessNewSetting
  case 1 (Link):     if name=="Back" → PreviousMenu(), else → NewMenu(MenuLink)
  case 2 (Link2):    same as case 1
  case 3 (Toggle):   OptionIncremented → ProcessNewSetting  
  case 4 (Action):   DoAction()
```

This means adding a DataTable entry with `OptionType=1 (Link)` and `MenuLink=36` to the Main page automatically navigates to our ModsPage when confirmed. **No Kismet edits needed.**

---

## PAK Modifications

### 1. DebugMenuType Enum
- Add `ModsPage = 36` (before `DebugMenuType_MAX`)
- Update MAX value to 37

### 2. DebugMenuOptions DataTable
- Add `Main_Mods` row:
  - `Menu` = `NewEnumerator0` (Main page, value 1)
  - `OptionName` = `"Mods"`
  - `OptionType` = `NewEnumerator1` (Link/Submenu)
  - `MenuLink` = `ModsPage` (value 36)
  - `OptionList` = `[]` (empty)
  - `ConsoleCommand` = `""` (empty)

### 3. Assets Rebuilt
All 16 debug menu assets are included in the PAK (both modified and unmodified) to ensure the complete set is loaded at priority 1000+.

---

## Lua API Architecture (v17)

### Hook Strategy (Minimal Interference)

| Hook | Type | Purpose |
|------|------|---------|
| `DebugMenu_C:NewMenu` | post | Populate ModsPage, detect page changes, enhance display |
| `BndEvt__DebugInput_*` (×14) | pre+post | Block confirm on ModsPage, handle custom input on sub-pages |
| `DebugOptionWidget_C:OnInitialized` | post | Detect new DebugMenu instances |

### Page States

| State | Scroll | Confirm | Back | Description |
|-------|--------|---------|------|-------------|
| **Normal page** | Blueprint | Blueprint | Blueprint | No interference |
| **ModsPage (top)** | Blueprint ✅ | Lua (BLOCK) | Blueprint ✅ | Confirm intercepted, scroll/back native |
| **Sub-page** | Lua (BLOCK) | Lua (BLOCK) | Lua (BLOCK) | Full Lua control |

### Key Insight: Top-Level ModsPage

On the top-level ModsPage, **scrolling and back navigation are handled entirely by the Blueprint**:
- `InputActionScrollUp/Down` → changes `CurrentIndex`, calls `UpdateOptionHighlight`
- `InputActionBack` → calls `PreviousMenu()` → navigates to Main page

Only **confirm** is intercepted: the Lua hook reads `CurrentIndex` from the Blueprint and calls the registered handler.

### Sub-Pages (Lua-Managed)

When a mod opens a sub-page via `NavigateTo()`:
- Page stack saves current state
- All input is blocked at BndEvt level
- Lua manages scroll cursor, confirm, and back
- `PopPage()` restores previous state

### Dynamic Height

After populating the ModsPage:
1. Set `MaxVisible = 50` on VBoxList
2. Set `DrawSize.Y = 2000` on WidgetComponent
3. Call `UpdateListView` to apply visibility
4. Multi-layer highlighting: `UpdateOptionHighlight` + Switcher + Opacity

### Fallback Mode (No PAK)

If the PAK is not installed:
1. Lua injects "=== MODS ===" entry via `CreateActiveOption` on Main page
2. BndEvt confirm hook detects selection and manually shows ModsPage
3. All functionality works, just less cleanly integrated

---

## SharedAPI (Backward-Compatible)

```lua
SharedAPI.DebugMenu = {
  -- Registration (existing, backward-compatible)
  RegisterToggle(mod, label, getter, setter)
  RegisterAction(mod, label, callback)
  RegisterSubMenu(mod, label, openFn)
  
  -- Navigation (existing)
  NavigateTo(opts)          -- Push to sub-page with populate function
  AddItem(label, handler)   -- Add item during populate callback
  Refresh()                 -- Refresh current sub-page
  
  -- Query (existing + new)
  GetInstance()             -- Get DebugMenu_C instance
  IsVisible()              -- Menu actor exists
  IsOnModsPage()           -- Currently on Mods page
  GetEntries()             -- Get registered entries table
  GetEntryCount()          -- Total registered entries
}
```

### Registration Examples

```lua
-- Toggle (most common)
SharedAPI.DebugMenu.RegisterToggle("GodMode", "God Mode",
    function() return godModeEnabled end,
    function(v) godModeEnabled = v end
)

-- Action (one-shot)
SharedAPI.DebugMenu.RegisterAction("TyrantAI", "Tyrant Status",
    function() Log("Status: " .. getStatus()) end
)

-- Submenu (custom sub-page)
SharedAPI.DebugMenu.RegisterSubMenu("EnemySpawner", "Enemy Spawner", function()
    SharedAPI.DebugMenu.AddItem("Spawn Ganado", function() spawnEnemy("Ganado") end)
    SharedAPI.DebugMenu.AddItem("Spawn Chainsaw", function() spawnEnemy("Chainsaw") end)
end)
```

---

## File Structure

```
PAKS_extracted/DebugMenu_mod.pak    ← Built by tools/build_debug_pak.py
mods/DebugMenuAPI/main.lua          ← v17 Lua API
docs/DEBUG_MENU_REDESIGN.md         ← This document
```

## Build & Deploy

```bash
# Build the PAK
python tools/build_debug_pak.py

# Deploy PAK + mods
python tools/deploy.py all

# Or just mods (if PAK already on device)
python tools/deploy.py mods
```
