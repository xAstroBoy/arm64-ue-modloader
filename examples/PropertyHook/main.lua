-- examples/PropertyHook/main.lua
-- ═══════════════════════════════════════════════════════════════════════
-- Property Hook — Advanced hooking patterns
--
-- This demonstrates:
--   - RegisterPreHook with BLOCK to prevent original execution
--   - RegisterPostHook to modify return values
--   - NotifyOnNewObject to react to spawned actors
--   - Multiple hook patterns for comprehensive coverage
--
-- These are TEMPLATE patterns — replace class/function names with real ones.
-- ═══════════════════════════════════════════════════════════════════════

local TAG = "PropertyHook"
local state = { enabled = true }

-- Load config
local saved = ModConfig.Load(TAG)
if saved and saved.enabled ~= nil then state.enabled = saved.enabled end

-- Register in debug menu
if SharedAPI and SharedAPI.DebugMenu then
    SharedAPI.DebugMenu.RegisterToggle(TAG, "Property Hook Demo", state.enabled,
        function(s) state.enabled = s; ModConfig.Save(TAG, state) end)
end

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 1: PRE-HOOK — Block a function entirely
-- ═══════════════════════════════════════════════════════════════════════
-- Return "BLOCK" to prevent the original UFunction from executing.
-- Post-hooks still fire even when blocked.

--[[
RegisterPreHook("/Script/Game.DamageSystem:ApplyDamage", function(self, func, parms)
    if not state.enabled then return end
    return "BLOCK"  -- Damage never applies
end)
Log(TAG .. ": PreHook — Block ApplyDamage")
]]

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 2: POST-HOOK — Modify return values
-- ═══════════════════════════════════════════════════════════════════════
-- PostHook fires AFTER the original runs. Use parms to read/write return value.
-- For bool returns: WriteU8(parms, 0) = false, WriteU8(parms, 1) = true

--[[
RegisterPostHook("/Script/Game.Player:IsDead", function(self, func, parms)
    if not state.enabled then return end
    WriteU8(parms, 0)  -- Always return false = never dead
end)
Log(TAG .. ": PostHook — Override IsDead → false")
]]

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 3: POST-HOOK — Read and modify object properties
-- ═══════════════════════════════════════════════════════════════════════
-- Use self:get() to access the UObject the function was called on.

--[[
RegisterPostHook("/Script/Game.WeaponBase:GetDamage", function(self, func, parms)
    if not state.enabled then return end

    local weapon = self:get()
    if not weapon or not weapon:IsValid() then return end

    -- Read current damage
    pcall(function()
        local dmg = weapon:Get("BaseDamage")
        Log(TAG .. ": Weapon damage = " .. tostring(dmg))
    end)

    -- Double the damage (modify return via parms)
    -- Note: return type determines which Write function to use
    -- WriteU8 = bool/byte, WriteS32 = int32, WriteF32 = float
end)
]]

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 4: NotifyOnNewObject — React to spawned objects
-- ═══════════════════════════════════════════════════════════════════════
-- Fires whenever a new instance of the class is created.
-- Great for modifying objects at spawn time.

--[[
NotifyOnNewObject("/Script/Game.EnemyCharacter", function(obj)
    if not state.enabled then return end
    if not obj or not obj:IsValid() then return end

    -- Modify the enemy at spawn time
    pcall(function()
        obj:Set("MaxHealth", 1)        -- One-hit kill enemies
        obj:Set("MovementSpeed", 0.5)  -- Slow enemies
        Log(TAG .. ": Modified enemy: " .. obj:GetName())
    end)
end)
Log(TAG .. ": Watching for enemy spawns")
]]

-- ═══════════════════════════════════════════════════════════════════════
-- Pattern 5: Using RegisterHook (returns both pre and post IDs)
-- ═══════════════════════════════════════════════════════════════════════
-- RegisterHook returns (preHookId, postHookId) for later removal.

--[[
local preId, postId = RegisterHook(
    "/Script/Game.SomeClass:SomeFunc",
    function(self, parms)
        -- This is the pre-hook callback
        Log(TAG .. ": SomeFunc called!")
    end
)
Log(TAG .. ": Hook IDs — pre=" .. tostring(preId) .. " post=" .. tostring(postId))

-- To remove later:
-- UnregisterHook("/Script/Game.SomeClass:SomeFunc", preId, postId)
]]

Log(TAG .. ": Loaded — all patterns are templates, uncomment to use")
