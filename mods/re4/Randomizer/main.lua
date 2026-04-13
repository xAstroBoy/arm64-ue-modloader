-- mods/Randomizer/main.lua v7.1
-- ═══════════════════════════════════════════════════════════════════════
-- UE4SS-enhanced Enemy Randomizer — re-randomizes on EVERY level load
--
-- v7.1 — Fix lightuserdata handling:
--   Native hooks pass pointer/integer args as lightuserdata (void*).
--   Use PtrToInt() for comparisons/formatting, keep raw ptr for ReadU8/WriteU8/Offset.
-- v7.0 — Crash-safe + verbose debug, pcall wrapper, settle period.
-- v6.0 — Per-level-load randomization, emId mismatch detection.
-- v5.0 — Once-per-slot randomization, debug menu submenu
-- v3.0 — RegisterNativeHookAt on readEmList, per-enemy toggle commands
-- ═══════════════════════════════════════════════════════════════════════
local TAG = "Randomizer"
local VERBOSE = true  -- VERBOSE LOGGING — log every hook call for crash diagnosis

local function V(msg)
    if VERBOSE then Log(TAG .. ": [V] " .. msg) end
end

-- ── Pointer helpers ─────────────────────────────────────────────────
-- Native hooks and CallNative with 'p' return pass lightuserdata (void*).
-- PtrToInt (C++) converts to Lua integer for arithmetic/formatting.
-- These helpers provide safe conversion with fallbacks.
local function ptrval(p)
    if p == nil then return 0 end
    if type(p) == "number" then return p end
    if PtrToInt then return PtrToInt(p) end
    -- Fallback: parse tostring output ("userdata: 0xABCD")
    local s = tostring(p)
    local hex = s:match(": 0x(%x+)")
    if hex then return tonumber(hex, 16) end
    return 0
end

local function ptrfmt(p)
    return string.format("0x%X", ptrval(p))
end

-- ── HP presets ──────────────────────────────────────────────────────
local HP = {
    EASY   = { 232, 3 },   NORMAL = { 184, 11 },  HARD = { 76, 29 },
}
local HP_BOSS = {
    EASY   = { 184, 11 },  NORMAL = { 136, 19 },  HARD = { 76, 29 },
}
local HP_GARRADOR = {
    EASY   = { 160, 15 },  NORMAL = { 136, 19 },  HARD = { 124, 21 },
}
local function makeHP(lo, hi) return lo + (hi * 256) end

-- ═══════════════════════════════════════════════════════════════════════
-- ENEMY POOL — Same data as EnemySpawner, with group field
-- Format: { name, bytes[9], hpType, group, removeInvincible }
-- ═══════════════════════════════════════════════════════════════════════
local POOL = {
    -- Villagers
    {"Villager 1500", {21,0,0,0,24,64,0,232,3}, nil, "Villagers"},
    {"Villager 1501", {21,0,0,0,8,20,1,232,3}, nil, "Villagers"},
    {"Villager 1502", {21,0,0,0,2,32,1,232,3}, nil, "Villagers"},
    {"Villager 1503", {21,3,0,0,0,48,32,232,3}, nil, "Villagers"},
    {"Villager 1504", {21,3,0,0,1,52,65,232,3}, nil, "Villagers"},
    {"Villager 1505", {21,3,0,0,68,56,1,232,3}, nil, "Villagers"},
    {"Villager 1506", {21,4,0,0,96,48,0,232,3}, nil, "Villagers"},
    {"Villager 1507", {21,4,0,0,104,32,1,232,3}, nil, "Villagers"},
    {"Villager 1508", {21,4,0,0,105,64,0,232,3}, nil, "Villagers"},
    -- Salvadors
    {"Dr. Salvador 150", {21,0,0,0,96,16,16,232,3}, nil, "Salvadors"},
    {"Dr. Salvador 151", {21,0,0,0,1,0,16,232,3}, nil, "Salvadors"},
    {"Dr. Salvador 152", {21,0,48,0,2,48,16,232,3}, nil, "Salvadors"},
    {"Bella Sister 00", {21,11,0,0,96,0,16,232,3}, nil, "Salvadors"},
    {"Bella Sister 01", {21,11,0,0,97,16,16,232,3}, nil, "Salvadors"},
    {"Bella Sister 02", {21,11,48,0,99,32,16,232,3}, nil, "Salvadors"},
    -- Zealots 1C
    {"Zealot 1C00", {28,7,0,0,0,20,0,232,3}, nil, "Zealots"},
    {"Zealot 1C01", {28,7,0,0,16,18,1,232,3}, nil, "Zealots"},
    {"Zealot 1C02", {28,7,0,0,2,2,1,232,3}, nil, "Zealots"},
    {"Zealot 1C03", {28,7,0,0,8,128,0,232,3}, nil, "Zealots"},
    {"Zealot 1C04", {28,7,0,0,8,4,129,232,3}, nil, "Zealots"},
    {"Zealot 1C08", {28,9,0,0,0,20,0,232,3}, nil, "Zealots"},
    {"Zealot 1C09", {28,9,0,0,16,2,1,232,3}, nil, "Zealots"},
    -- Zealots 1A
    {"Zealot 1A00", {26,7,0,2,0,16,0,232,3}, nil, "Zealots"},
    {"Zealot 1A01", {26,7,0,2,16,2,1,232,3}, nil, "Zealots"},
    {"Zealot 1A02", {26,7,0,2,2,3,1,232,3}, nil, "Zealots"},
    {"Zealot 1A08", {26,8,0,2,0,16,0,232,3}, nil, "Zealots"},
    -- Zealots misc
    {"Zealot 110", {17,8,0,96,2,72,0,232,3}, nil, "Zealots"},
    {"Zealot 140", {20,8,0,50,1,80,0,232,3}, nil, "Zealots"},
    {"Zealot 1B1", {27,7,37,32,32,7,0,232,3}, nil, "Zealots"},
    -- Garradors
    {"Garrador", {28,10,0,0,0,0,0,160,15}, "garrador", "Garradors"},
    {"Armored Garrador", {28,13,0,0,0,0,0,124,21}, "garrador", "Garradors"},
    {"Armored Garrador Plaga", {28,10,48,0,0,0,0,160,15}, "garrador", "Garradors"},
    {"Garrador 1B", {27,10,0,0,0,0,0,232,3}, "garrador", "Garradors"},
    -- Mace Ganados
    {"Mace Soldier 00", {31,24,0,0,24,0,1,232,3}, nil, "Mace Ganados"},
    {"Mace Soldier 01", {31,24,0,0,72,4,1,232,3}, nil, "Mace Ganados"},
    {"Mace Soldier 02", {31,24,0,0,89,16,1,232,3}, nil, "Mace Ganados"},
    {"Mace Soldier 03", {31,24,0,0,10,0,1,232,3}, nil, "Mace Ganados"},
    -- Soldiers
    {"Soldier 1D00", {29,14,0,8,72,1,0,232,3}, nil, "Soldiers"},
    {"Soldier 1D01", {29,14,0,0,2,2,129,232,3}, nil, "Soldiers"},
    {"Soldier 1E00", {30,14,0,8,1,3,160,232,3}, nil, "Soldiers"},
    {"Soldier 1E01", {30,14,0,8,1,0,192,232,3}, nil, "Soldiers"},
    {"Soldier 1E05", {30,15,0,2,16,32,137,232,3}, nil, "Soldiers"},
    {"Soldier 1E11", {30,23,0,2,48,17,8,232,3}, nil, "Soldiers"},
    {"Soldier 1F00", {31,14,0,10,0,80,33,232,3}, nil, "Soldiers"},
    -- JJs
    {"JJ", {29,2,0,0,0,0,0,136,19}, "boss", "JJs"},
    {"JJ Plaga", {29,2,48,0,0,0,0,136,19}, "boss", "JJs"},
    -- Super Salvadors
    {"Super Salvador", {32,22,0,0,0,0,0,172,13}, "boss", "Super Salvadors"},
    {"Super Salvador Plaga", {32,22,0,8,8,176,16,172,13}, "boss", "Super Salvadors"},
    -- Dogs
    {"Colmillos (Dog)", {34,0,0,0,0,0,0,232,3}, nil, "Dogs"},
    -- Armaduras
    {"Armadura 00", {60,0,0,0,0,0,0,232,3}, nil, "Armaduras"},
    {"Armadura 01", {60,1,0,0,0,0,0,232,3}, nil, "Armaduras"},
    {"Armadura 02", {60,2,0,0,0,0,0,232,3}, nil, "Armaduras"},
    -- Drones
    {"Drone", {58,0,0,0,0,0,0,232,3}, nil, "Drones"},
    {"Ground Robot", {58,2,0,0,0,0,0,232,3}, nil, "Drones"},
    -- Animals
    {"Chicken", {40,0,0,0,0,0,0,232,3}, nil, "Animals"},
    {"Crow", {35,0,0,0,0,0,0,232,3}, nil, "Animals"},
    {"Snake", {36,0,1,0,0,0,0,232,3}, nil, "Animals"},
    -- Traps
    {"Walking Parasite", {37,0,0,0,0,0,0,232,3}, nil, "Traps"},
    {"Bear Trap", {42,0,0,0,0,0,0,232,3}, nil, "Traps"},
    -- Novistadores
    {"Novistador", {45,0,0,0,0,0,0,232,3}, nil, "Novistadores"},
    -- Regenerators
    {"Regenerator", {54,0,0,0,0,0,0,232,3}, nil, "Regenerators"},
    {"Iron Maiden", {54,2,0,0,0,0,0,232,3}, nil, "Regenerators"},
    -- Gigantes
    {"Gigante", {43,0,0,0,0,0,0,232,3}, "boss", "Gigantes"},
    -- Verdugos
    {"Verdugo", {55,0,0,0,0,0,0,232,3}, "boss", "Verdugos"},
    {"Verdugo After", {56,0,0,0,0,0,0,232,3}, "boss", "Verdugos"},
    -- Bosses
    {"Mendez Phase 1", {53,0,0,0,0,0,0,232,3}, "boss", "Bosses"},
    {"Mendez Phase 2", {53,1,0,0,0,0,0,232,3}, "boss", "Bosses"},
    {"Knife Krauser", {57,1,0,0,0,0,0,232,3}, "boss", "Bosses"},
    {"Mutant Krauser", {57,2,0,0,0,0,0,232,3}, "boss", "Bosses"},
    {"U3 (It)", {50,1,0,0,0,0,0,232,3}, "boss", "Bosses", true},
}

-- Build processed pool
local ENEMIES = {}
local GROUPS = {}
local groupSet = {}
for _, raw in ipairs(POOL) do
    local e = {
        name   = raw[1],
        bytes  = raw[2],
        hpType = raw[3] or "normal",
        group  = raw[4] or "Other",
        removeInvincible = raw[5] or false,
    }
    ENEMIES[#ENEMIES + 1] = e
    if not groupSet[e.group] then
        groupSet[e.group] = true
        GROUPS[#GROUPS + 1] = e.group
    end
end

-- ═══════════════════════════════════════════════════════════════════════
-- STATE
-- ═══════════════════════════════════════════════════════════════════════
local state = {
    enabled = true,     -- v7.2: default ON (was false, causing no-scramble confusion)
    hpMode  = "MATCH",  -- MATCH, EASY, NORMAL, HARD, RANDOM
    enabledEnemies = {},
    swapCount = 0,
}

-- Default: all enabled
for _, e in ipairs(ENEMIES) do state.enabledEnemies[e.name] = true end

-- ═══════════════════════════════════════════════════════════════════════
-- PER-LEVEL TRACKING (must be before saveConfig for Lua scoping)
-- ═══════════════════════════════════════════════════════════════════════
local currentGen    = 0      -- Increments on each detected level change
local levelSwaps    = 0      -- Swaps since last level change
local lastDetectTime = 0     -- Debounce level-change log messages
local cachedPool    = nil    -- Pre-built enabled pool (rebuilt on config change)
local poolDirty     = true   -- Flag to rebuild cachedPool

local function invalidatePool()
    cachedPool = nil
    poolDirty = true
end

-- ═══════════════════════════════════════════════════════════════════════
-- CONFIG PERSISTENCE
-- ═══════════════════════════════════════════════════════════════════════
local function saveConfig()
    local names = {}
    for name, on in pairs(state.enabledEnemies) do
        if on then names[#names + 1] = name end
    end
    ModConfig.Save("Randomizer", {
        configVersion = 2,
        enabled = state.enabled,
        hpMode = state.hpMode,
        enabledNames = names,
    })
    invalidatePool()  -- Rebuild cached pool on next pickRandom
end

local function loadConfig()
    local cfg = ModConfig.Load("Randomizer")
    if not cfg then
        Log(TAG .. ": No saved config — using defaults (enabled=true)")
        return
    end
    -- v7.2: Config migration — old configs had enabled=false by default
    if cfg.configVersion and cfg.configVersion >= 2 then
        if cfg.enabled ~= nil then state.enabled = cfg.enabled end
    else
        -- Old config without version: ignore saved 'enabled' (was incorrectly false)
        state.enabled = true
        Log(TAG .. ": CONFIG MIGRATION — old config had enabled=false, forcing ON")
    end
    if cfg.hpMode then state.hpMode = cfg.hpMode end
    if cfg.enabledNames then
        for name, _ in pairs(state.enabledEnemies) do state.enabledEnemies[name] = false end
        for _, name in ipairs(cfg.enabledNames) do
            if state.enabledEnemies[name] ~= nil then state.enabledEnemies[name] = true end
        end
    end
end
loadConfig()

-- ═══════════════════════════════════════════════════════════════════════
-- HP CALCULATION
-- ═══════════════════════════════════════════════════════════════════════
local function getRandomHP(enemy)
    local mode = state.hpMode
    if mode == "MATCH" then return { enemy.bytes[8], enemy.bytes[9] } end
    if mode == "RANDOM" then
        local modes = { "EASY", "NORMAL", "HARD" }
        mode = modes[math.random(#modes)]
    end
    local t = enemy.hpType
    if t == "boss" then return HP_BOSS[mode] or HP_BOSS.NORMAL end
    if t == "garrador" then return HP_GARRADOR[mode] or HP_GARRADOR.NORMAL end
    return HP[mode] or HP.NORMAL
end

local function getEnabledPool()
    if cachedPool and not poolDirty then return cachedPool end
    local pool = {}
    for _, e in ipairs(ENEMIES) do
        if state.enabledEnemies[e.name] then pool[#pool + 1] = e end
    end
    cachedPool = pool
    poolDirty = false
    return pool
end

local function pickRandom()
    local pool = getEnabledPool()
    if #pool == 0 then return nil end
    return pool[math.random(#pool)]
end

-- ═══════════════════════════════════════════════════════════════════════
-- NATIVE HOOKS — Randomize enemy spawns (re-randomize on every level load)
-- ═══════════════════════════════════════════════════════════════════════
-- Level-change detection via emId mismatch:
--   After we randomize a slot, we cache the replacement emId.
--   On each readEmList call, we read the current emId from the ESL entry.
--   If it doesn't match our cached value, the game loaded new level data.
--   We clear ALL slot caches to force fresh random picks for every enemy.
--
-- This approach correctly detects level changes regardless of enemy count,
-- unlike the old getEmListNum approach which failed when two levels had
-- the same number of enemies.
local sym_readEmList     = Resolve("readEmList",    0x065E4278)
local sym_getEmListNum   = Resolve("getEmListNum",  0x065E40DC)
local sym_EmListSetAlive = Resolve("EmListSetAlive",0x062EA224)

local slotMap = {}          -- [slotIndex] = {bytes, hp, name, removeInvincible, replEmId}
local lastReadIndex = -1    -- Captured by pre-hook for the post-hook to use

-- Safety: settle period after level change — let ESL memory stabilize before writing
local SETTLE_CALLS = 10     -- Skip this many readEmList calls after level change
local settleCounter = 0     -- Counts down after level change detected (0 = ready)
local hookErrors = 0        -- Consecutive hook/write errors (reset on success)
local MAX_HOOK_ERRORS = 20  -- Auto-disable writes after this many consecutive errors

-- Hook readEmList — pre-hook captures index, post-hook detects level change + applies randomization
if sym_readEmList then
    RegisterNativeHookAt(sym_readEmList, "readEmList",
        function(index)
            -- Pre-hook: capture the slot index argument (pcall for safety)
            -- Native hooks pass X-register values as lightuserdata — convert to number
            pcall(function()
                lastReadIndex = ptrval(index)
                V("PRE readEmList idx=" .. tostring(lastReadIndex))
            end)
        end,
        function(retPtr)
            -- ╔═══════════════════════════════════════════════════════════╗
            -- ║  CRITICAL: Entire body in pcall so a Lua error can NEVER ║
            -- ║  prevent us from returning retPtr to the game engine.    ║
            -- ╚═══════════════════════════════════════════════════════════╝
            local ok, err = pcall(function()
                V("POST retPtr=" .. ptrfmt(retPtr)
                    .. " enabled=" .. tostring(state.enabled)
                    .. " idx=" .. tostring(lastReadIndex)
                    .. " settle=" .. settleCounter
                    .. " errors=" .. hookErrors)

                if not state.enabled then V("SKIP: disabled"); return end
                if retPtr == nil or ptrval(retPtr) == 0 then V("SKIP: retPtr null/0"); return end
                if lastReadIndex < 0 then V("SKIP: lastReadIndex<0"); return end
                if hookErrors >= MAX_HOOK_ERRORS then V("SKIP: too many errors (" .. hookErrors .. ")"); return end

                local idx = lastReadIndex

                -- ── Settling period after level change ──
                if settleCounter > 0 then
                    V("SETTLE: skipping slot " .. idx .. " (" .. settleCounter .. " remaining)")
                    settleCounter = settleCounter - 1
                    return
                end

                local cached = slotMap[idx]

                -- ── Read original bytes at retPtr for diagnostics ──
                local origBytes = ""
                pcall(function()
                    local parts = {}
                    for j = 0, 8 do
                        parts[j+1] = string.format("%02X", ReadU8(Offset(retPtr, j)))
                    end
                    origBytes = table.concat(parts, " ")
                end)
                V("SLOT " .. idx .. " orig=[" .. origBytes .. "] cached=" .. tostring(cached ~= nil))

                -- ── Level-change detection ──
                if cached then
                    local readOk, currentEmId = pcall(ReadU8, retPtr)
                    if not readOk then
                        V("LEVEL-DETECT: retPtr INVALID — clearing all caches")
                        slotMap = {}
                        levelSwaps = 0
                        settleCounter = SETTLE_CALLS
                        return
                    end
                    V("LEVEL-CHECK: slot " .. idx .. " currentEmId=" .. tostring(currentEmId) .. " cachedEmId=" .. cached.replEmId)
                    if currentEmId == cached.replEmId then
                        hookErrors = 0
                        V("CACHE-HIT: slot " .. idx .. " → " .. cached.name .. " (no write needed)")
                        return
                    end
                    -- Mismatch! Game loaded new data → level changed.
                    local now = os.clock()
                    currentGen = currentGen + 1
                    Log(TAG .. ": === LEVEL CHANGE #" .. currentGen
                        .. " === (slot " .. idx
                        .. " emId " .. tostring(currentEmId or "?") .. "≠" .. cached.replEmId
                        .. ") — settling " .. SETTLE_CALLS .. " calls before re-randomizing")
                    lastDetectTime = now
                    slotMap = {}
                    levelSwaps = 0
                    settleCounter = SETTLE_CALLS
                    return  -- Don't write this call — let ESL stabilize first
                end

                -- ── Randomize new slot ──
                local pick = pickRandom()
                if not pick then V("SKIP: empty pool"); return end
                local hp = getRandomHP(pick)

                -- Format bytes for logging
                local pickBytesStr = ""
                for j = 1, #pick.bytes do
                    pickBytesStr = pickBytesStr .. string.format("%02X ", pick.bytes[j])
                end
                V("PICK: slot " .. idx .. " → " .. pick.name
                    .. " bytes=[" .. pickBytesStr .. "]"
                    .. " HP={" .. hp[1] .. "," .. hp[2] .. "}=" .. makeHP(hp[1], hp[2])
                    .. " rmInvinc=" .. tostring(pick.removeInvincible))

                -- Validate retPtr is still readable before committing writes
                local valOk = pcall(ReadU8, retPtr)
                if not valOk then
                    V("WRITE-ABORT: retPtr unreadable before write at slot " .. idx)
                    hookErrors = hookErrors + 1
                    return
                end

                -- Write randomization bytes to ESL entry
                local writeOk = pcall(function()
                    for j = 0, 6 do
                        WriteU8(Offset(retPtr, j), pick.bytes[j + 1])
                    end
                    WriteU8(Offset(retPtr, 7), hp[1])
                    WriteU8(Offset(retPtr, 8), hp[2])
                    if pick.removeInvincible then
                        local flags = ReadU8(Offset(retPtr, 2))
                        local newFlags = flags & ~0x40
                        V("INVINC: slot " .. idx .. " flags 0x" .. string.format("%02X", flags) .. " → 0x" .. string.format("%02X", newFlags))
                        WriteU8(Offset(retPtr, 2), newFlags)
                    end
                end)

                if writeOk then
                    -- Verify what we actually wrote
                    local verifyBytes = ""
                    pcall(function()
                        local parts = {}
                        for j = 0, 8 do
                            parts[j+1] = string.format("%02X", ReadU8(Offset(retPtr, j)))
                        end
                        verifyBytes = table.concat(parts, " ")
                    end)
                    V("WROTE: slot " .. idx .. " → " .. pick.name .. " verify=[" .. verifyBytes .. "]")

                    slotMap[idx] = {
                        bytes = pick.bytes,
                        hp = hp,
                        name = pick.name,
                        removeInvincible = pick.removeInvincible,
                        replEmId = pick.bytes[1],
                    }
                    levelSwaps = levelSwaps + 1
                    state.swapCount = state.swapCount + 1
                    hookErrors = 0
                    Log(TAG .. ": [gen" .. currentGen .. "] Swap #" .. levelSwaps
                        .. " slot " .. idx
                        .. " → " .. pick.name
                        .. " HP=" .. makeHP(hp[1], hp[2])
                        .. " bytes=[" .. pickBytesStr .. "]")
                else
                    V("WRITE-FAIL: slot " .. idx .. " → " .. pick.name .. " — write pcall failed")
                    slotMap[idx] = nil
                    hookErrors = hookErrors + 1
                    Log(TAG .. ": Write error #" .. hookErrors .. " at slot " .. idx .. " for " .. pick.name)
                end
            end)

            if not ok then
                -- Log but NEVER let errors escape to native hook layer
                pcall(Log, TAG .. ": HOOK PCALL ERROR: " .. tostring(err))
                hookErrors = hookErrors + 1
            end

            V("RETURN retPtr=" .. ptrfmt(retPtr))
            -- ALWAYS return retPtr — game MUST get a valid pointer back
            return retPtr
        end)
    Log(TAG .. ": Hooked readEmList — per-level-load re-randomization (settle=" .. SETTLE_CALLS .. ", maxErrors=" .. MAX_HOOK_ERRORS .. ")")
end

-- ═══════════════════════════════════════════════════════════════════════
-- SCRAMBLE — Immediate randomization of all current enemies
-- ═══════════════════════════════════════════════════════════════════════
local function scrambleNow()
    if not sym_getEmListNum or not sym_readEmList then
        LogWarn(TAG .. ": Missing native bindings")
        return 0
    end

    -- Clear slot map and reset settle counter
    slotMap = {}
    levelSwaps = 0
    settleCounter = 0
    hookErrors = 0

    local n = CallNative(sym_getEmListNum, "i")
    V("SCRAMBLE: emListNum=" .. tostring(n))
    local swapped = 0
    for i = 0, n - 1 do
        local ptr = CallNative(sym_readEmList, "pi", i)
        V("SCRAMBLE: slot " .. i .. " ptr=" .. ptrfmt(ptr))
        if ptrval(ptr) ~= 0 then
            local pick = pickRandom()
            if pick then
                local hp = getRandomHP(pick)
                local pickBytes = ""
                for j = 1, #pick.bytes do pickBytes = pickBytes .. string.format("%02X ", pick.bytes[j]) end
                V("SCRAMBLE-WRITE: slot " .. i .. " → " .. pick.name .. " bytes=[" .. pickBytes .. "] HP={" .. hp[1] .. "," .. hp[2] .. "}")
                pcall(function()
                    for j = 0, 6 do
                        WriteU8(Offset(ptr, j), pick.bytes[j + 1])
                    end
                    WriteU8(Offset(ptr, 7), hp[1])
                    WriteU8(Offset(ptr, 8), hp[2])
                    if pick.removeInvincible then
                        local flags = ReadU8(Offset(ptr, 2))
                        WriteU8(Offset(ptr, 2), flags & ~0x40)
                    end
                    if sym_EmListSetAlive then
                        CallNative(sym_EmListSetAlive, "vii", i, 1)
                    end
                end)
                swapped = swapped + 1
            end
        end
    end

    Log(TAG .. ": Scrambled " .. swapped .. "/" .. n .. " enemies")
    Notify(TAG, "Scrambled " .. swapped .. " enemies!")
    return swapped
end

-- ═══════════════════════════════════════════════════════════════════════
-- COMMANDS
-- ═══════════════════════════════════════════════════════════════════════

RegisterCommand("randomizer", function()
    state.enabled = not state.enabled
    slotMap = {}
    levelSwaps = 0
    settleCounter = 0
    hookErrors = 0
    saveConfig()
    V("TOGGLE: enabled=" .. tostring(state.enabled) .. " slotMap cleared")
    Log(TAG .. ": " .. (state.enabled and "ON" or "OFF"))
    Notify(TAG, state.enabled and "ON" or "OFF")
end)

RegisterCommand("rnd_hpmode", function(args)
    local m = (args or "MATCH"):upper()
    local valid = { MATCH=true, EASY=true, NORMAL=true, HARD=true, RANDOM=true }
    if valid[m] then
        state.hpMode = m
        saveConfig()
        Log(TAG .. ": HP mode → " .. m)
        Notify(TAG, "HP: " .. m)
    else
        Log(TAG .. ": Valid modes: MATCH, EASY, NORMAL, HARD, RANDOM")
    end
end)

RegisterCommand("rnd_enable_group", function(args)
    local count = 0
    for _, e in ipairs(ENEMIES) do
        if e.group == args then state.enabledEnemies[e.name] = true; count = count + 1 end
    end
    saveConfig()
    Log(TAG .. ": Enabled " .. count .. " in " .. tostring(args))
end)

RegisterCommand("rnd_disable_group", function(args)
    local count = 0
    for _, e in ipairs(ENEMIES) do
        if e.group == args then state.enabledEnemies[e.name] = false; count = count + 1 end
    end
    saveConfig()
    Log(TAG .. ": Disabled " .. count .. " in " .. tostring(args))
end)

RegisterCommand("rnd_enable_all", function()
    for name, _ in pairs(state.enabledEnemies) do state.enabledEnemies[name] = true end
    saveConfig()
    Log(TAG .. ": All " .. #ENEMIES .. " enemies enabled")
end)

RegisterCommand("rnd_disable_all", function()
    for name, _ in pairs(state.enabledEnemies) do state.enabledEnemies[name] = false end
    saveConfig()
    Log(TAG .. ": All enemies disabled")
end)

RegisterCommand("rnd_scramble", function() scrambleNow() end)

RegisterCommand("rnd_groups", function()
    for _, g in ipairs(GROUPS) do
        local total, on = 0, 0
        for _, e in ipairs(ENEMIES) do
            if e.group == g then
                total = total + 1
                if state.enabledEnemies[e.name] then on = on + 1 end
            end
        end
        Log("  " .. g .. ": " .. on .. "/" .. total)
    end
end)

RegisterCommand("rnd_verbose", function()
    VERBOSE = not VERBOSE
    Log(TAG .. ": Verbose logging " .. (VERBOSE and "ON" or "OFF"))
    Notify(TAG, "Verbose: " .. (VERBOSE and "ON" or "OFF"))
end)

RegisterCommand("rnd_status", function()
    local pool = getEnabledPool()
    Log(TAG .. ": enabled=" .. tostring(state.enabled)
        .. " hpMode=" .. state.hpMode
        .. " pool=" .. #pool .. "/" .. #ENEMIES
        .. " totalSwaps=" .. state.swapCount
        .. " levelGen=" .. currentGen
        .. " levelSwaps=" .. levelSwaps
        .. " settle=" .. settleCounter
        .. " errors=" .. hookErrors
        .. " groups=" .. #GROUPS)
end)

-- ═══════════════════════════════════════════════════════════════════════
-- SHARED API
-- ═══════════════════════════════════════════════════════════════════════

if SharedAPI then
    SharedAPI.Randomizer = {
        isEnabled  = function() return state.enabled end,
        setEnabled = function(v) state.enabled = v; slotMap = {}; levelSwaps = 0; settleCounter = 0; hookErrors = 0; saveConfig() end,
        getHPMode  = function() return state.hpMode end,
        setHPMode  = function(m) state.hpMode = m; saveConfig() end,
        scramble   = scrambleNow,
        getPoolSize = function() return #getEnabledPool() end,
        getSwapCount = function() return state.swapCount end,
        GROUPS = GROUPS,
    }
end

if SharedAPI and SharedAPI.DebugMenu then
    local api = SharedAPI.DebugMenu

    api.RegisterSubMenu("Randomizer", "Randomizer", function()
        api.NavigateTo({ populate = function()
            -- Main toggle
            local st = state.enabled and "ON" or "OFF"
            api.AddItem("[" .. st .. "] Enemy Randomizer", function()
                state.enabled = not state.enabled
                slotMap = {}
                levelSwaps = 0
                settleCounter = 0
                hookErrors = 0
                saveConfig()
                api.Refresh()
            end)

            -- HP Mode cycle
            api.AddItem("HP Mode: " .. state.hpMode, function()
                local modes = {"MATCH", "EASY", "NORMAL", "HARD", "RANDOM"}
                local idx = 1
                for i, m in ipairs(modes) do
                    if m == state.hpMode then idx = i; break end
                end
                state.hpMode = modes[(idx % #modes) + 1]
                saveConfig()
                api.Refresh()
            end)

            -- Stats
            local pool = getEnabledPool()
            api.AddItem("Pool: " .. #pool .. "/" .. #ENEMIES .. " enemies", nil)
            api.AddItem("Total Swaps: " .. state.swapCount, nil)
            api.AddItem("Level #" .. currentGen .. " | Swaps: " .. levelSwaps, nil)

            api.AddItem("--- ACTIONS ---", nil)
            api.AddItem(">> Scramble Now! <<", function() scrambleNow() end)
            api.AddItem(">> Force Re-randomize <<", function()
                slotMap = {}
                levelSwaps = 0
                settleCounter = 0
                hookErrors = 0
                Log(TAG .. ": Forced re-randomization — all slots will re-roll")
                Notify(TAG, "Re-randomizing on next enemy read!")
                api.Refresh()
            end)
            api.AddItem("Enable All Enemies", function()
                for name, _ in pairs(state.enabledEnemies) do
                    state.enabledEnemies[name] = true
                end
                saveConfig()
                api.Refresh()
            end)
            api.AddItem("Disable All Enemies", function()
                for name, _ in pairs(state.enabledEnemies) do
                    state.enabledEnemies[name] = false
                end
                saveConfig()
                api.Refresh()
            end)

            -- Per-group toggles
            api.AddItem("--- GROUPS ---", nil)
            for _, g in ipairs(GROUPS) do
                local groupName = g
                local total, on = 0, 0
                for _, e in ipairs(ENEMIES) do
                    if e.group == groupName then
                        total = total + 1
                        if state.enabledEnemies[e.name] then on = on + 1 end
                    end
                end
                local allOn = (on == total)
                api.AddItem("[" .. (allOn and "ALL" or on .. "/" .. total) .. "] " .. groupName, function()
                    local newState = not allOn
                    for _, e in ipairs(ENEMIES) do
                        if e.group == groupName then
                            state.enabledEnemies[e.name] = newState
                        end
                    end
                    saveConfig()
                    api.Refresh()
                end)
            end
        end })
    end)
end

Log(TAG .. ": v7.1 loaded — " .. #ENEMIES .. " enemies, "
    .. #GROUPS .. " groups, "
    .. (state.enabled and "ON" or "OFF")
    .. " hpMode=" .. state.hpMode
    .. " | Crash-safe native hook (pcall + settle=" .. SETTLE_CALLS .. ")")
