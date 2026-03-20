-- examples/SimpleToggle/main.lua
-- ═══════════════════════════════════════════════════════════════════════
-- Simple Toggle — Debug Menu integration example
--
-- This demonstrates:
--   - Registering a toggle in the DebugMenuAPI
--   - Using ModConfig for persistent settings
--   - Hooking a UFunction with RegisterPostHook
--   - Conditional behavior based on toggle state
--
-- Requires: DebugMenuAPI mod to be loaded first
-- ═══════════════════════════════════════════════════════════════════════

local TAG = "SimpleToggle"
local state = { enabled = false }

-- ═══════════════════════════════════════════════════════════════════════
-- Persistent Config
-- ═══════════════════════════════════════════════════════════════════════
-- ModConfig saves/loads JSON files in the mod's data directory.
-- Settings persist across game restarts.

local saved = ModConfig.Load(TAG)
if saved and saved.enabled ~= nil then
    state.enabled = saved.enabled
    Log(TAG .. ": Loaded saved state: " .. tostring(state.enabled))
end

local function save_config()
    ModConfig.Save(TAG, state)
end

-- ═══════════════════════════════════════════════════════════════════════
-- Debug Menu Registration
-- ═══════════════════════════════════════════════════════════════════════
-- SharedAPI.DebugMenu is provided by the DebugMenuAPI mod.
-- Always check it exists — your mod should work even without it.

if SharedAPI and SharedAPI.DebugMenu then
    SharedAPI.DebugMenu.RegisterToggle(TAG, "My Feature", state.enabled,
        function(new_state)
            state.enabled = new_state
            Log(TAG .. ": Toggled to " .. tostring(new_state))
            save_config()
        end
    )
    Log(TAG .. ": Registered in debug menu")
else
    Log(TAG .. ": DebugMenuAPI not available (mod works, no menu toggle)")
end

-- ═══════════════════════════════════════════════════════════════════════
-- Game Hook Example
-- ═══════════════════════════════════════════════════════════════════════
-- Replace "/Script/Game.ClassName:FunctionName" with a real UFunction path.
-- This is just a template pattern:

--[[
RegisterPostHook("/Script/Game.SomeClass:SomeFunction", function(self, func, parms)
    if not state.enabled then return end

    -- Access the object this was called on
    local obj = self:get()
    if not obj or not obj:IsValid() then return end

    -- Read/write properties
    local value = obj:Get("SomeProperty")
    Log(TAG .. ": SomeProperty = " .. tostring(value))

    -- Modify behavior
    obj:Set("SomeProperty", 42)
end)
]]

Log(TAG .. ": Loaded (enabled=" .. tostring(state.enabled) .. ")")
