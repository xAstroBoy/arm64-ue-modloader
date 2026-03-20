# Creating Mods

This guide covers everything you need to know to create mods for Quest UE4 Modloader.

## Mod Structure

Every mod is a folder inside `mods/` containing at least a `main.lua` file:

```
mods/
└── MyMod/
    ├── main.lua        # Entry point (required)
    └── *.pak           # Optional PAK files (auto-mounted)
```

The modloader discovers and loads all `mods/*/main.lua` files on game start.

## Mod Lifecycle

1. **Discovery** — Modloader scans `mods/` for folders with `main.lua`
2. **Loading** — Each `main.lua` is executed in its own Lua environment
3. **Globals injected** — `MOD_NAME`, `MOD_DIR`, `SharedAPI`, and the full API
4. **Hooks registered** — Your hooks are installed and active
5. **Runtime** — Hooks fire as the game runs, timers execute, bridge commands available

## Per-Mod Globals

Every mod gets these globals automatically:

| Global | Type | Description |
| --- | --- | --- |
| `MOD_NAME` | string | Folder name (e.g., `"MyMod"`) |
| `MOD_DIR` | string | Full path to mod folder |
| `SharedAPI` | table | Cross-mod communication (shared across all mods) |

## Core Patterns

### Pattern 1: Simple Hook

```lua
-- Hook a game function and modify behavior
RegisterPostHook("/Script/Game.SomeClass:SomeFunction", function(self, func, parms)
    local obj = self:get()
    if obj and obj:IsValid() then
        obj:Set("Health", 100)
    end
end)
```

### Pattern 2: Toggle with Config

```lua
local TAG = "MyMod"
local state = { enabled = true }

-- Load saved config
local saved = ModConfig.Load(TAG)
if saved and saved.enabled ~= nil then
    state.enabled = saved.enabled
end

-- Register in debug menu
if SharedAPI and SharedAPI.DebugMenu then
    SharedAPI.DebugMenu.RegisterToggle(TAG, "My Feature", state.enabled,
        function(new_state)
            state.enabled = new_state
            ModConfig.Save(TAG, state)
        end
    )
end

-- Conditional hook
RegisterPostHook("/Script/Game.SomeClass:SomeFunc", function(self, func, parms)
    if not state.enabled then return end
    -- Do something only when enabled
end)
```

### Pattern 3: Block a Function

```lua
-- PreHook can return "BLOCK" to prevent the original from running
RegisterPreHook("/Script/Game.DamageSystem:ApplyDamage", function(self, func, parms)
    return "BLOCK"  -- Damage never applies
end)
```

### Pattern 4: Spawn Watcher

```lua
-- React when new objects are spawned
NotifyOnNewObject("/Script/Game.EnemyCharacter", function(obj)
    if not obj or not obj:IsValid() then return end
    pcall(function()
        obj:Set("MaxHealth", 1)
        Log("Modified enemy: " .. obj:GetName())
    end)
end)
```

### Pattern 5: Delayed / Periodic Execution

```lua
-- One-shot after 5 seconds
ExecuteWithDelay(5000, function()
    Log("5 seconds have passed!")
end)

-- Repeat every 2 seconds
LoopAsync(2000, function()
    local player = FindFirstOf("PlayerController")
    if player and player:IsValid() then
        -- Periodic check/modification
    end
end)

-- Next frame (game thread)
ExecuteInGameThread(function()
    -- Safe to modify game objects here
end)
```

### Pattern 6: Working with Structs

```lua
-- Read a struct property
local actor = FindFirstOf("SomeActor")
local pos = actor:Get("Position")  -- Returns LuaUStruct

-- Access fields
Log("X=" .. pos.X .. " Y=" .. pos.Y .. " Z=" .. pos.Z)

-- Modify fields (writes to live memory)
pos.X = 100
pos.Y = 200
pos.Z = 300

-- Or set from a table
actor:Set("Position", {X=100, Y=200, Z=300})

-- Or pass struct to Call()
actor:Call("SetActorLocation", {X=100, Y=200, Z=300})

-- Clone a struct (creates an independent copy)
local saved_pos = pos:Clone()
```

## Safety: pcall Everything

**Always** wrap UObject operations in `pcall()`. Objects can become invalid at any time (garbage collected, level unloaded, etc.).

```lua
-- ❌ WRONG: crash if obj is invalid
local health = obj:Get("Health")

-- ✅ RIGHT: safe
local ok, health = pcall(function() return obj:Get("Health") end)
if ok then
    Log("Health: " .. tostring(health))
end
```

**Use separate pcall blocks** for independent operations:

```lua
-- ❌ WRONG: if first fails, second never runs
pcall(function()
    obj:Set("Health", 100)
    obj:Set("Armor", 50)
end)

-- ✅ RIGHT: independent operations in separate blocks
pcall(function() obj:Set("Health", 100) end)
pcall(function() obj:Set("Armor", 50) end)
```

## Debug Menu Integration

The DebugMenuAPI mod provides an in-game menu for toggles, actions, and sub-pages.

### Simple Registration

```lua
local api = SharedAPI and SharedAPI.DebugMenu

if api then
    -- Toggle (on/off switch)
    api.RegisterToggle("MyMod", "Feature Name", false, function(state)
        Log("Feature: " .. tostring(state))
    end)

    -- Action (one-shot button)
    api.RegisterAction("MyMod", "Do Something", function()
        Log("Button pressed!")
    end)

    -- Selector (cycle through options)
    api.RegisterSelector("MyMod", "Difficulty", {"Easy","Normal","Hard"}, function(value, index)
        Log("Selected: " .. value)
    end)

    -- Sub-menu (opens a new page)
    api.RegisterSubMenu("MyMod", "Advanced", function()
        api.NavigateTo({
            name = "Advanced Settings",
            populate = function()
                api.AddItem("Option 1", function() Log("1!") end)
                api.AddItem("Option 2", function() Log("2!") end)
            end
        })
    end)
end
```

See [Debug Menu API](Debug-Menu-API.md) for full documentation.

## Cross-Mod Communication

Use `SharedAPI` to expose functionality to other mods:

```lua
-- In your mod:
SharedAPI.MyMod = {
    GetHealth = function() return current_health end,
    SetGodMode = function(on) god_mode = on end,
}

-- In another mod:
if SharedAPI.MyMod then
    SharedAPI.MyMod.SetGodMode(true)
end
```

Or use shared variables:

```lua
SetSharedVariable("MyMod_Enabled", true)
local val = GetSharedVariable("MyMod_Enabled")
```

## Bridge Commands

Register custom commands accessible from the ADB bridge:

```lua
RegisterBridgeCommand("mymod_status", function()
    return {
        enabled = state.enabled,
        health = current_health,
        version = "1.0"
    }
end)
```

Test from your PC:
```bash
python tools/deploy.py console
> mymod_status
```

## File I/O

```lua
-- Read a file
local content = ReadTextFile(MOD_DIR .. "config.txt")

-- Write a file
WriteTextFile(MOD_DIR .. "output.txt", "Hello!")

-- Check existence
if FileExists(MOD_DIR .. "data.json") then
    -- ...
end
```

For structured config, prefer `ModConfig`:

```lua
-- Save
ModConfig.Save("MyMod", { enabled = true, level = 5 })

-- Load
local cfg = ModConfig.Load("MyMod")
if cfg then
    Log("Level: " .. tostring(cfg.level))
end
```

## Tips & Best Practices

1. **Always use reflection** (`Get`/`Set`/`Call`) — never raw memory offsets for UObject properties
2. **Wrap everything in pcall** — objects can be GC'd at any time
3. **Separate pcall blocks** — don't combine independent operations
4. **TArray is 1-indexed** — `arr[1]` is the first element
5. **Test via bridge first** — validate API calls with `exec_lua` before coding
6. **Log generously** — use `Log()` with your mod's tag for debugging
7. **Save state with ModConfig** — persist settings across game restarts
8. **Register in debug menu** — let users toggle your mod in-game
9. **Check IsValid()** — before using any UObject reference
10. **Use ExecuteWithDelay** — game objects aren't ready at load time
