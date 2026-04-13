-- mods/HeadBlocker/main.lua v1.0
-- ═══════════════════════════════════════════════════════════════════════
-- Head Blocker Remover — destroys VR4HeadBlocker actors on spawn
-- via NotifyOnNewObject + K2_DestroyActor.
-- ═══════════════════════════════════════════════════════════════════════
local TAG = "HeadBlocker"
local VERBOSE = false
local function V(...) if VERBOSE then Log(TAG .. " [V] " .. string.format(...)) end end

local state = { enabled = true }

local saved = ModConfig.Load("HeadBlocker")
if saved then
    if saved.enabled ~= nil then state.enabled = saved.enabled end
end

-- ═══════════════════════════════════════════════════════════════════════
-- NOTIFY ON NEW OBJECT — Destroy VR4HeadBlocker on spawn
-- ═══════════════════════════════════════════════════════════════════════

local destroyed = 0

-- Wait for player pawn to exist before destroying anything
local function waitForPlayerThenDestroy(obj)
    V("waitForPlayerThenDestroy called, obj=%s", tostring(obj))
    -- Check if player pawn exists (game is fully initialized)
    local pawn = FindFirstOf("VR4GamePlayerPawn")
    if not pawn or not pawn:IsValid() then
        -- Player not ready yet — retry in 200ms
        ExecuteWithDelay(200, function()
            V("waitForPlayerThenDestroy retry (pawn not ready)")
            if obj and obj:IsValid() then
                waitForPlayerThenDestroy(obj)
            end
        end)
        return
    end
    
    -- Player exists — safe to destroy after one more frame
    ExecuteWithDelay(100, function()
        V("VR4HeadBlocker deferred destroy executing")
        if not obj or not obj:IsValid() then return end
        local ok, err = pcall(function() obj:K2_DestroyActor() end)
        if ok then
            destroyed = destroyed + 1
            Log(TAG .. ": VR4HeadBlocker destroyed (total: " .. destroyed .. ")")
        else
            LogWarn(TAG .. ": K2_DestroyActor failed: " .. tostring(err))
            pcall(function() obj:SetActorHiddenInGame(true) end)
            Log(TAG .. ": HeadBlocker hidden as fallback")
        end
    end)
end

NotifyOnNewObject("VR4HeadBlocker", function(obj)
    V("NotifyOnNewObject VR4HeadBlocker fired, obj=%s", tostring(obj))
    if not state.enabled then return end
    if not obj:IsValid() then return end
    
    -- Hide immediately to prevent visual glitch, then defer destroy
    pcall(function() obj:SetActorHiddenInGame(true) end)
    waitForPlayerThenDestroy(obj)
end)

-- ═══════════════════════════════════════════════════════════════════════
-- COMMAND
-- ═══════════════════════════════════════════════════════════════════════

RegisterCommand("headblocker", function()
    state.enabled = not state.enabled
    ModConfig.Save("HeadBlocker", state)
    Log(TAG .. ": " .. (state.enabled and "OFF" or "ON"))
    Notify(TAG, state.enabled and "Blocker OFF" or "Blocker ON")
end)

-- ═══════════════════════════════════════════════════════════════════════
-- DEBUG MENU
-- ═══════════════════════════════════════════════════════════════════════

if SharedAPI and SharedAPI.DebugMenu then
    SharedAPI.DebugMenu.RegisterToggle("HeadBlocker", "Head Blocker Off",
        function() return state.enabled end,
        function(v) state.enabled = v; ModConfig.Save("HeadBlocker", state) end)
end

Log(TAG .. ": v1.0 loaded — VR4HeadBlocker destroy on spawn")
