# Debug Menu API

The DebugMenuAPI mod provides an in-game menu system that other mods can use to register toggles, actions, selectors, and sub-pages.

## Overview

The debug menu is accessed in-game via the VR controller debug input. The DebugMenuAPI mod adds a "Mods" entry to the main debug menu page, which opens a custom page where all registered mod options appear.

## Architecture

```
Main Debug Menu (page 1, Blueprint)
  ├── Settings
  ├── Display
  ├── ...
  └── Mods ──► Mods Root Page (page 100, Lua)
                ├── God Mode [ON]           (toggle)
                ├── No Recoil [OFF]         (toggle)
                ├── Difficulty [Normal]     (selector)
                ├── Spawn Enemy             (action)
                ├── Advanced Settings >>    (submenu)
                │     ├── Speed: 1.0x       (dynamic item)
                │     ├── Reset All         (dynamic item)
                │     └── Back              (auto-added)
                └── Back                    (auto-added)
```

## Simple API

These functions add items to the root "Mods" page. They're the easiest way to integrate.

### RegisterToggle

```lua
SharedAPI.DebugMenu.RegisterToggle(mod_name, display_name, default_state, callback)
```

| Parameter | Type | Description |
| --- | --- | --- |
| `mod_name` | string | Your mod's identifier (e.g., `"GodMode"`) |
| `display_name` | string | Text shown in the menu (e.g., `"God Mode"`) |
| `default_state` | boolean | Initial on/off state |
| `callback` | function | `fn(new_state, item)` — called on toggle |

**Returns:** item table

**Example:**
```lua
SharedAPI.DebugMenu.RegisterToggle("GodMode", "God Mode", false, function(enabled)
    god_mode = enabled
    Log("God Mode: " .. tostring(enabled))
end)
```

Display: `God Mode [OFF]` / `God Mode [ON]`

### RegisterAction

```lua
SharedAPI.DebugMenu.RegisterAction(mod_name, display_name, callback)
```

| Parameter | Type | Description |
| --- | --- | --- |
| `mod_name` | string | Mod identifier |
| `display_name` | string | Button text |
| `callback` | function | `fn(item)` — called on confirm |

**Example:**
```lua
SharedAPI.DebugMenu.RegisterAction("Spawner", "Spawn Enemy", function()
    -- Spawn logic here
    Log("Enemy spawned!")
end)
```

### RegisterSelector

```lua
SharedAPI.DebugMenu.RegisterSelector(mod_name, display_name, options, callback)
```

| Parameter | Type | Description |
| --- | --- | --- |
| `mod_name` | string | Mod identifier |
| `display_name` | string | Label text |
| `options` | table | `{"Easy", "Normal", "Hard"}` |
| `callback` | function | `fn(selected_value, selected_index, item)` |

**Example:**
```lua
SharedAPI.DebugMenu.RegisterSelector("MyMod", "Difficulty",
    {"Easy", "Normal", "Hard", "Nightmare"},
    function(value, index)
        Log("Difficulty: " .. value .. " (index " .. index .. ")")
    end
)
```

Display: `Difficulty [Normal]` — cycles on each confirm.

## Advanced API

For dynamic sub-pages with content that can change at runtime.

### RegisterSubMenu

```lua
SharedAPI.DebugMenu.RegisterSubMenu(mod_name, display_name, callback)
```

Adds a `>>` link on the root page. When selected, `callback` fires — call `NavigateTo` inside it.

**Example:**
```lua
SharedAPI.DebugMenu.RegisterSubMenu("MyMod", "Advanced Settings", function()
    SharedAPI.DebugMenu.NavigateTo({
        name = "Advanced Settings",
        populate = function()
            SharedAPI.DebugMenu.AddItem("Option 1", function()
                Log("Selected option 1")
            end)
            SharedAPI.DebugMenu.AddItem("Option 2", function()
                Log("Selected option 2")
                SharedAPI.DebugMenu.Refresh()  -- Rebuild page
            end)
        end
    })
end)
```

### NavigateTo

```lua
SharedAPI.DebugMenu.NavigateTo({ name = "Title", populate = function() ... end })
```

Creates a new page and navigates to it. The `populate` function is called every time the page is rendered (including on `Refresh()`).

### AddItem

```lua
SharedAPI.DebugMenu.AddItem(display_name, callback_or_nil)
```

Adds an item to the page currently being built inside a `populate` callback. Pass `nil` as callback for a non-interactive label/separator.

### Refresh

```lua
SharedAPI.DebugMenu.Refresh()
```

Re-renders the current custom page. For dynamic pages, re-calls the `populate` function. Use this to update displayed values after state changes.

## Static Sub-Pages

For pages with fixed content that doesn't change.

### AddPage

```lua
local page = SharedAPI.DebugMenu.AddPage(page_id, page_title)
```

Creates a static page and auto-adds a navigation link on the root Mods page.

### AddItemToPage

```lua
SharedAPI.DebugMenu.AddItemToPage(page, mod_name, name, item_type, opts)
```

| Parameter | Type | Description |
| --- | --- | --- |
| `page` | table | Page returned by `AddPage()` |
| `mod_name` | string | Mod identifier |
| `name` | string | Display name |
| `item_type` | string | `"toggle"`, `"action"`, or `"selector"` |
| `opts` | table | `{default, callback, options, default_index}` |

**Example:**
```lua
local page = SharedAPI.DebugMenu.AddPage("settings", "My Settings")

SharedAPI.DebugMenu.AddItemToPage(page, "MyMod", "Verbose Logs", "toggle", {
    default = false,
    callback = function(on) verbose = on end
})

SharedAPI.DebugMenu.AddItemToPage(page, "MyMod", "Reset", "action", {
    callback = function() reset_all() end
})
```

## Utility Functions

```lua
SharedAPI.DebugMenu.GetPages()          -- All custom pages (read-only)
SharedAPI.DebugMenu.IsCustomPage(byte)  -- Is this byte a custom page?
SharedAPI.DebugMenu.VERSION             -- API version string
```

## Complete Example

```lua
-- mods/MyMod/main.lua
local TAG = "MyMod"
local state = {
    enabled = true,
    difficulty = "Normal",
    speed = 1.0,
}

-- Load saved config
local saved = ModConfig.Load(TAG)
if saved then
    if saved.enabled ~= nil then state.enabled = saved.enabled end
    if saved.difficulty then state.difficulty = saved.difficulty end
    if saved.speed then state.speed = saved.speed end
end

local function save() ModConfig.Save(TAG, state) end

-- Register in debug menu
local api = SharedAPI and SharedAPI.DebugMenu
if api then
    -- Toggle on root page
    api.RegisterToggle(TAG, "My Feature", state.enabled, function(on)
        state.enabled = on
        save()
    end)

    -- Selector on root page
    api.RegisterSelector(TAG, "Difficulty",
        {"Easy", "Normal", "Hard"},
        function(val) state.difficulty = val; save() end
    )

    -- Sub-menu with dynamic content
    api.RegisterSubMenu(TAG, "Speed Settings", function()
        api.NavigateTo({
            name = "Speed Settings",
            populate = function()
                local speeds = {0.5, 1.0, 1.5, 2.0, 3.0}
                for _, s in ipairs(speeds) do
                    local label = "Speed " .. s .. "x"
                    if s == state.speed then label = label .. " ✓" end
                    api.AddItem(label, function()
                        state.speed = s
                        save()
                        api.Refresh()
                    end)
                end
            end
        })
    end)
end
```
