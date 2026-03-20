-- examples/HelloWorld/main.lua
-- ═══════════════════════════════════════════════════════════════════════
-- Hello World — Minimal mod example
--
-- This is the simplest possible mod. It demonstrates:
--   - Basic logging
--   - Finding game objects with FindFirstOf
--   - Reading properties with Get / dynamic access
--   - The mod lifecycle (code runs once on load)
--
-- To install: copy this folder to mods/HelloWorld/ on your device
-- ═══════════════════════════════════════════════════════════════════════

-- Every mod gets these globals automatically:
--   MOD_NAME  = "HelloWorld"     (folder name)
--   MOD_DIR   = "/path/to/mod/"  (full path)
--   SharedAPI = {}               (cross-mod communication)

Log("Hello World mod loaded!")
Log("  Mod name: " .. MOD_NAME)
Log("  Mod dir:  " .. MOD_DIR)

-- ═══════════════════════════════════════════════════════════════════════
-- Finding Game Objects
-- ═══════════════════════════════════════════════════════════════════════
-- FindFirstOf returns the first live instance of a class.
-- Always check IsValid() — the object may not exist yet at load time.

local pc = FindFirstOf("PlayerController")
if pc and pc:IsValid() then
    Log("Found PlayerController: " .. pc:GetName())
    Log("  Class: " .. pc:GetClassName())
    Log("  Full:  " .. pc:GetFullName())
else
    Log("PlayerController not found yet (normal at early load)")
end

-- ═══════════════════════════════════════════════════════════════════════
-- Reading Properties
-- ═══════════════════════════════════════════════════════════════════════
-- Use obj:Get("PropertyName") or the shorthand obj.PropertyName
-- Always wrap in pcall() for safety.

local world = GetWorldContext()
if world and world:IsValid() then
    local ok, name = pcall(function() return world:GetName() end)
    if ok then
        Log("Current world: " .. tostring(name))
    end
end

-- ═══════════════════════════════════════════════════════════════════════
-- Delayed Execution
-- ═══════════════════════════════════════════════════════════════════════
-- Many objects aren't available at mod load time.
-- Use ExecuteWithDelay to run code later.

ExecuteWithDelay(5000, function()
    Log("HelloWorld: 5 seconds have passed!")

    -- Now the game is likely fully loaded
    local player = FindFirstOf("PlayerController")
    if player and player:IsValid() then
        Log("HelloWorld: Player is alive at " .. player:GetName())
    end
end)

Log("HelloWorld: Init complete — check the log for delayed messages")
