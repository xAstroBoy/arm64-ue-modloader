-- ============================================================================
-- PFX_Cheats v1 — In-game Cheat Menu + Infinite Ball Save
-- ============================================================================
-- Features:
--   1. Infinite Ball Save — hooks OnGameStateChanged on PFXGameflowObject_PlayTable
--      and calls SkipPrepareGameEnd() to cancel drain. Logs all state transitions
--      so we can tune the exact state number.
--   2. Bridge commands — toggle cheats at runtime via TCP bridge console
--   3. Wrist-menu stub — hooks BP_WristMenu_C:Open to inject cheat entries
-- ============================================================================
local TAG = "PFX_Cheats"
local VERBOSE = true
local function V(...) if VERBOSE then Log(TAG .. " [V] " .. string.format(...)) end end
Log(TAG .. ": Loading v1...")

-- ============================================================================
-- STATE
-- ============================================================================
local cheats = {
    infinite_ball_save  = true,   -- toggle via bridge: cheats_ballsave 0|1
    log_game_states     = true,   -- log every OnGameStateChanged transition
    big_ball            = false,  -- scale active table ball up
    large_flippers      = false,  -- scale flipper actors up
}

local stats = {
    ball_saves          = 0,
    state_changes       = 0,
    game_end_blocked    = 0,
    big_ball_applies    = 0,
    flipper_applies     = 0,
}

-- Ball scales uniformly. Flippers stretch only along their length axis (X)
-- so they cover the gap without tilting or distorting the shape.
local BIG_BALL_SCALE   = 2.50
local FLIPPER_SCALE_X  = 3.50   -- current X stretch (runtime-mutable via slider)
local FLIPPER_SCALE_Y  = 1.0    -- keep width unchanged
local FLIPPER_SCALE_Z  = 1.0    -- keep height unchanged

-- Cache of original BoxComponent half-extents (populated on first scale).
-- Allows restoring the exact original collision shape before each re-scale
-- so actor-level (sx,1,1) scale never compounds with direct BoxExtent calls.
local flipper_box_extents_cache = {}
local flipper_mesh_scales_cache = {}
local flipper_axis_cache = {}

-- FindAllOf in this environment is exact-class only (no subclass matching).
-- To support ALL tables, we dynamically discover flipper BP classes from
-- BlueprintGeneratedClass and then cache active classes with live instances.
local FLIPPER_CLASS_DISCOVERY = {
    all_candidates = nil,   -- all *Flippers* BP class names discovered from BPGC
    active_classes = {},    -- subset that currently has live instances
    last_discovery_t = 0,
}

local function get_active_table_folder_from_ball_class()
    local classes = { "ball_C", "Ball_C", "ball", "Ball" }
    for _, cn in ipairs(classes) do
        local arr = nil
        pcall(function() arr = FindAllOf(cn) end)
        if arr and #arr > 0 then
            for _, b in ipairs(arr) do
                local valid = false
                pcall(function()
                    valid = (b ~= nil) and (b:IsValid() == true)
                end)
                if valid then
                    local cf = ""
                    pcall(function() cf = tostring(b:GetClass():GetFullName() or "") end)
                    local folder = cf:match("/Game/Tables/([^/]+)/")
                    if folder and folder ~= "" then
                        return folder
                    end
                end
            end
        end
    end
    return nil
end

local function is_table_rewards_path(path)
    if not path or path == "" then return false end
    return path:lower():find("/tablerewards/", 1, true) ~= nil
end

local function should_consider_flipper_class(class_name)
    if not class_name or class_name == "" then return false end
    local n = class_name:lower()
    if not n:find("flipper", 1, true) then return false end
    -- avoid unrelated UI/cabinet button classes
    if n:find("button", 1, true) then return false end
    -- reward classes typically follow BP_*_Flippers{0,1,2}_C
    return n:find("_c", 1, true) ~= nil
end

local function discover_all_flipper_classes()
    local now = os.clock()
    if FLIPPER_CLASS_DISCOVERY.all_candidates and (now - FLIPPER_CLASS_DISCOVERY.last_discovery_t) < 30 then
        return FLIPPER_CLASS_DISCOVERY.all_candidates
    end

    local table_folder = get_active_table_folder_from_ball_class()

    local folder_need = nil
    if table_folder and table_folder ~= "" then
        folder_need = ("/game/tables/" .. table_folder:lower() .. "/")
    end

    local out = {}
    local seen = {}

    pcall(function()
        local classes = FindAllOf("BlueprintGeneratedClass")
        if not classes then return end
        for _, cls in ipairs(classes) do
            local cn = ""
            local full = ""
            pcall(function() cn = tostring(cls:GetName()) end)
            if should_consider_flipper_class(cn) and not seen[cn] then
                if folder_need then
                    pcall(function() full = tostring(cls:GetFullName()) end)
                    local lf = full:lower()
                    if not lf:find(folder_need, 1, true) then
                        goto continue_class
                    end
                end
                if full == "" then
                    pcall(function() full = tostring(cls:GetFullName()) end)
                end
                if is_table_rewards_path(full) then
                    goto continue_class
                end
                seen[cn] = true
                out[#out + 1] = cn
            end
            ::continue_class::
        end
    end)

    FLIPPER_CLASS_DISCOVERY.all_candidates = out
    FLIPPER_CLASS_DISCOVERY.last_discovery_t = now
    return out
end


-- Known game state values (discovered via live logging — expand as needed)
-- State 3 = "PrepareBallEnd" / drain in most YUP pinball builds
-- We block on any state > 1 that is not "Paused" (usually 2)
local DRAIN_STATES = { [3]=true, [5]=true, [6]=true, [7]=true }
local PAUSE_STATE  = 2

-- ============================================================================
-- HELPERS
-- ============================================================================
local function is_live(obj)
    if not obj then return false end
    local ok, valid = pcall(function() return obj:IsValid() end)
    if not ok or not valid then return false end
    local ok2, name = pcall(function() return obj:GetName() end)
    if not ok2 or not name then return false end
    return not name:match("^Default__") and not name:match("^REINST_")
end

local function find_live(...)
    for _, cls in ipairs({...}) do
        local all = nil
        pcall(function() all = FindAllOf(cls) end)
        if all then
            for _, obj in ipairs(all) do
                if is_live(obj) then return obj end
            end
        end
    end
    return nil
end

local function get_play_table()
    return find_live("PFXGameflowObject_PlayTable")
end

local function in_table_gameplay()
    local pt = find_live("GFO_PlayTable_C", "PFXGameflowObject_PlayTable")
    if not pt then return false end

    local paused = false
    pcall(function() paused = pt:Call("IsPaused") end)

    local tr = nil
    pcall(function() tr = pt:Get("m_tableReference") end)
    if tr and is_live(tr) then
        local running = false
        pcall(function() running = tr:Call("IsTableGameRunning") end)
        return running == true and not paused
    end

    return not paused
end

local wrist_guard = {
    enabled = true,
    last_open_t = 0,
    blocked_close = 0,
}

local function force_wrist_menu_open_short(wm)
    if not wrist_guard.enabled then return end
    if not is_live(wm) then return end
    if not in_table_gameplay() then return end

    local t0 = os.clock()
    wrist_guard.last_open_t = t0

    for i = 1, 16 do
        ExecuteWithDelay(i * 120, function()
            if not is_live(wm) then return end
            local dt = os.clock() - (wrist_guard.last_open_t or 0)
            if dt > 2.2 then return end
            pcall(function() wm:Call("SetOpenState", true, false) end)
            pcall(function() wm:Call("SetOpenState", true) end)
        end)
    end
end

local function cheats_set_ballsave(enable)
    V("cheats_set_ballsave enable=%s", tostring(enable))
    cheats.infinite_ball_save = not not enable
    Log(TAG .. ": infinite_ball_save = " .. tostring(cheats.infinite_ball_save))
    return cheats.infinite_ball_save
end

local function cheats_toggle_ballsave()
    V("cheats_toggle_ballsave")
    return cheats_set_ballsave(not cheats.infinite_ball_save)
end

local function cheats_set_logstates(enable)
    V("cheats_set_logstates enable=%s", tostring(enable))
    cheats.log_game_states = not not enable
    return cheats.log_game_states
end

local function cheats_toggle_logstates()
    V("cheats_toggle_logstates")
    return cheats_set_logstates(not cheats.log_game_states)
end

local function cheats_saveball()
    V("cheats_saveball")
    local pt = get_play_table()
    if not pt then return false, "not in game" end
    local ok = pcall(function() pt:Call("SkipPrepareGameEnd") end)
    if ok then
        stats.ball_saves = stats.ball_saves + 1
        return true, "ball saved (total=" .. stats.ball_saves .. ")"
    end
    return false, "SkipPrepareGameEnd failed"
end

local function cheats_restartball()
    V("cheats_restartball")
    local pt = get_play_table()
    if not pt then return false, "not in game" end
    local ok = pcall(function() pt:Call("RestartGame") end)
    return ok, "RestartGame ok=" .. tostring(ok)
end

local function cheats_pause_resume()
    V("cheats_pause_resume")
    local pt = get_play_table()
    if not pt then return false, "not in game" end
    local paused = false
    pcall(function() paused = pt:Call("IsPaused") end)
    if paused then
        pcall(function() pt:Call("ResumeGame") end)
        return true, "Game RESUMED"
    else
        pcall(function() pt:Call("PauseGame") end)
        return true, "Game PAUSED"
    end
end

-- ============================================================================
-- SCALE CHEATS (Big Ball / Large Flippers)
-- Collision-safe approach:
--   * Actor scale is applied via SetActorScale3D
--   * Collision is explicitly re-enabled on actor and root primitive
-- ============================================================================

-- scale_vec: {X, Y, Z} table for non-uniform scale, or nil to use {sx, sx, sx}
local function apply_actor_scale_with_collision(actor, sx, scale_vec)
    if not is_live(actor) then return false end
    local sv = scale_vec or { X = sx, Y = sx, Z = sx }

    local ok_scale = false
    pcall(function()
        actor:Call("SetActorScale3D", sv)
        ok_scale = true
    end)

    pcall(function() actor:Call("SetActorEnableCollision", true) end)

    local root = nil
    pcall(function() root = actor:Call("K2_GetRootComponent") end)
    if root and is_live(root) then
        pcall(function() root:Set("CollisionEnabled", 3) end) -- QueryAndPhysics
        pcall(function() root:Call("SetCollisionEnabled", 3) end)
        pcall(function() root:Call("SetGenerateOverlapEvents", true) end)
        pcall(function() root:Call("SetWorldScale3D", sv) end)
    end

    -- Some gameplay visuals do not visibly resize from actor/root scale alone.
    -- Force primitive component scaling as well.
    pcall(function()
        local pcls = FindClass("PrimitiveComponent")
        if not pcls then return end
        local comps = actor:Call("K2_GetComponentsByClass", pcls)
        if not comps then return end
        for i = 1, #comps do
            local comp = comps[i]
            if is_live(comp) then
                pcall(function() comp:Call("SetRelativeScale3D", sv) end)
                pcall(function() comp:Call("SetWorldScale3D", sv) end)
            end
        end
    end)

    return ok_scale
end

local function refresh_collision_components(actor, component_class_name)
    if not is_live(actor) then return 0 end

    local cls = nil
    pcall(function() cls = FindClass(component_class_name) end)
    if not cls then return 0 end

    local comps = nil
    pcall(function() comps = actor:Call("K2_GetComponentsByClass", cls) end)
    if not comps or #comps == 0 then return 0 end

    local count = 0
    for i = 1, #comps do
        local comp = comps[i]
        if is_live(comp) then
            pcall(function() comp:Call("SetCollisionEnabled", 3) end)
            pcall(function() comp:Call("SetGenerateOverlapEvents", true) end)
            count = count + 1
        end
    end
    return count
end

local function find_active_ball()
    local candidates = {
        "ball_C",
        "Ball_C",
        "ball",
        "Ball",
    }

    for _, class_name in ipairs(candidates) do
        local balls = nil
        pcall(function() balls = FindAllOf(class_name) end)
        if balls then
            for _, b in ipairs(balls) do
                if is_live(b) then return b end
            end
        end
    end

    local fallback = nil
    pcall(function()
        local smas = FindAllOf("StaticMeshActor")
        if not smas then return end
        for _, sma in ipairs(smas) do
            if is_live(sma) then
                local n = ""
                pcall(function() n = sma:GetName() end)
                local nn = n:lower()
                if nn:find("ball", 1, true) and not nn:find("flipper", 1, true) then
                    fallback = sma
                    return
                end
            end
        end
    end)
    return fallback
end

local function collect_live_balls()
    local out = {}
    local seen = {}
    local candidates = {
        "ball_C",
        "Ball_C",
        "ball",
        "Ball",
    }

    for _, class_name in ipairs(candidates) do
        local balls = nil
        pcall(function() balls = FindAllOf(class_name) end)
        if balls then
            for _, b in ipairs(balls) do
                if is_live(b) then
                    local n = ""
                    pcall(function() n = b:GetName() end)
                    if n ~= "" and not seen[n] then
                        seen[n] = true
                        out[#out + 1] = b
                    end
                end
            end
        end
    end

    return out
end

local function collect_flipper_actors()
    local out = {}
    local seen = {}
    local active_table_folder = get_active_table_folder_from_ball_class()
    local active_table_need = nil
    if active_table_folder and active_table_folder ~= "" then
        active_table_need = ("/game/tables/" .. active_table_folder:lower() .. "/")
    end

    local function is_likely_non_gameplay_level(path)
        if not path or path == "" then return false end
        local p = path:lower()
        if p:find("/game/hub/", 1, true) then return true end
        if p:find("arcade", 1, true) then return true end
        return false
    end

    local function add_actor(a, bypass_table_path_filter)
        if not is_live(a) then return end
        local cf = ""
        local af = ""
        local n = ""
        pcall(function() cf = tostring(a:GetClass():GetFullName()) end)
        pcall(function() af = tostring(a:GetFullName()) end)
        pcall(function() n = tostring(a:GetName() or "") end)
        if is_table_rewards_path(cf) then return end
        if is_table_rewards_path(af) then return end
        if n ~= "" and n:lower():find("button", 1, true) then return end
        if active_table_need and not bypass_table_path_filter then
            local cfl = cf:lower()
            local afl = af:lower()
            if not cfl:find(active_table_need, 1, true) and not afl:find(active_table_need, 1, true) then
                return
            end
        else
            if is_likely_non_gameplay_level(af) then return end
        end
        if n == "" or seen[n] then return end
        seen[n] = true
        out[#out + 1] = a
    end

    -- Primary: base class for all table flipper collectible actors
    pcall(function()
        local all = FindAllOf("BP_Collectible_FlipperArm_Base_C")
        if all then
            for _, a in ipairs(all) do
                add_actor(a)
            end
        end
    end)

    -- Fallback: generic collectible actors whose name includes Flipper
    pcall(function()
        local all = FindAllOf("PFXCollectibleActor")
        if all then
            for _, a in ipairs(all) do
                if is_live(a) then
                    local n = ""
                    pcall(function() n = a:GetName() end)
                    if n ~= "" and n:lower():find("flipper", 1, true) then
                        add_actor(a)
                    end
                end
            end
        end
    end)

    -- Strict fallback: visible flipper meshes in active table only.
    -- Excludes buttons/rewards/hub via add_actor filters above.
    pcall(function()
        local smas = FindAllOf("StaticMeshActor")
        if not smas then return end
        for _, sma in ipairs(smas) do
            if is_live(sma) then
                local n = ""
                pcall(function() n = tostring(sma:GetName() or "") end)
                local nn = n:lower()
                if nn:find("flipper", 1, true) and not nn:find("button", 1, true) then
                    add_actor(sma)
                end
            end
        end
    end)

    -- Fast path: classes that had live actors recently (usually 3 per table)
    for _, class_name in ipairs(FLIPPER_CLASS_DISCOVERY.active_classes) do
        pcall(function()
            local all = FindAllOf(class_name)
            if all then
                for _, a in ipairs(all) do
                    add_actor(a)
                end
            end
        end)
    end

    -- If nothing found yet, do broad discovery once and repopulate active cache.
    if #out == 0 then
        local discovered = discover_all_flipper_classes() or {}
        local active = {}
        for _, class_name in ipairs(discovered) do
            pcall(function()
                local all = FindAllOf(class_name)
                if all and #all > 0 then
                    active[#active + 1] = class_name
                    for _, a in ipairs(all) do
                        add_actor(a)
                    end
                end
            end)
        end
        FLIPPER_CLASS_DISCOVERY.active_classes = active
    end

    return out
end

local function probe_scale_targets()
    local lines = {}

    local ball = find_active_ball()
    if ball and is_live(ball) then
        local bname = "?"
        local bclass = "?"
        pcall(function() bname = tostring(ball:GetName()) end)
        pcall(function() bclass = tostring(ball:GetClass():GetFullName()) end)
        lines[#lines + 1] = "ball=" .. bname
        lines[#lines + 1] = "ball_class=" .. bclass
    else
        lines[#lines + 1] = "ball=none"
    end

    local flippers = collect_flipper_actors()
    lines[#lines + 1] = "flippers_found=" .. tostring(flippers and #flippers or 0)

    if flippers then
        for i = 1, math.min(#flippers, 6) do
            local f = flippers[i]
            local n = "?"
            local c = "?"
            local s = "?"
            pcall(function() n = tostring(f:GetName()) end)
            pcall(function() c = tostring(f:GetClass():GetFullName()) end)
            pcall(function()
                local rc = f:Call("K2_GetRootComponent")
                local rs = rc and rc:Get("RelativeScale3D") or nil
                if rs then
                    s = string.format("(%.2f,%.2f,%.2f)", rs.X, rs.Y, rs.Z)
                end
            end)
            lines[#lines + 1] = string.format("f%d=%s|%s|scale=%s", i, n, c, s)
        end
    end

    return table.concat(lines, " || ")
end

local function apply_big_ball()
    local target_scale = cheats.big_ball and BIG_BALL_SCALE or 1.0
    local balls = collect_live_balls()
    if not balls or #balls == 0 then
        local one = find_active_ball()
        if one then balls = { one } else balls = {} end
    end

    if #balls == 0 then
        return false, "no active ball"
    end

    local ok_count = 0
    for _, ball in ipairs(balls) do
        if apply_actor_scale_with_collision(ball, target_scale) then
            refresh_collision_components(ball, "StaticMeshComponent")
            ok_count = ok_count + 1
        end
    end

    stats.big_ball_applies = stats.big_ball_applies + ok_count
    if ok_count > 0 then
        return true, "ball scale=" .. string.format("%.2f", target_scale) .. " (actors=" .. tostring(ok_count) .. ")"
    end
    return false, "failed to scale ball"
end

local function apply_large_flippers()
    local flippers = collect_flipper_actors()

    if not flippers or #flippers == 0 then
        return false, "no flipper actors"
    end

    local sx = cheats.large_flippers and FLIPPER_SCALE_X or 1.0

    local ok_count = 0
    for _, f in ipairs(flippers) do
        if is_live(f) then
            local actor_name = ""
            pcall(function() actor_name = tostring(f:GetName() or "") end)

            -- Keep actor transform neutral to avoid hierarchy-induced visual tilt.
            pcall(function() f:Call("SetActorScale3D", { X = 1.0, Y = 1.0, Z = 1.0 }) end)
            pcall(function() f:Call("SetActorEnableCollision", true) end)

            local length_axis = flipper_axis_cache[actor_name] or "X"

            -- 1) Update collision extents directly on BoxComponents (actual physics shape).
            pcall(function()
                local cls = FindClass("BoxComponent")
                if not cls then return end
                local comps = f:Call("K2_GetComponentsByClass", cls)
                if not comps then return end
                for i = 1, #comps do
                    local bc = comps[i]
                    if is_live(bc) then
                        local n = ""
                        pcall(function() n = tostring(bc:GetName() or "") end)
                        if n == "" then n = tostring(i) end
                        local key = actor_name .. "::" .. n
                        -- Snapshot original extent once (before first scale)
                        if not flipper_box_extents_cache[key] then
                            pcall(function()
                                local e = bc:Get("BoxExtent")
                                if e then
                                    flipper_box_extents_cache[key] = { X = e.X, Y = e.Y, Z = e.Z }
                                end
                            end)
                        end
                        local orig = flipper_box_extents_cache[key]
                        if orig then
                            -- Detect best length axis from original collision shape once.
                            if not flipper_axis_cache[actor_name] then
                                local ax = "X"
                                if orig.Y >= orig.X and orig.Y >= orig.Z then ax = "Y" end
                                if orig.Z >= orig.X and orig.Z >= orig.Y then ax = "Z" end
                                flipper_axis_cache[actor_name] = ax
                                length_axis = ax
                            end

                            local ex, ey, ez = orig.X, orig.Y, orig.Z
                            if length_axis == "X" then ex = orig.X * sx end
                            if length_axis == "Y" then ey = orig.Y * sx end
                            if length_axis == "Z" then ez = orig.Z * sx end

                            pcall(function()
                                bc:Call("SetBoxExtent", { X = ex, Y = ey, Z = ez }, true)
                            end)
                            pcall(function() bc:Call("SetCollisionEnabled", 3) end)
                            pcall(function() bc:Call("SetGenerateOverlapEvents", true) end)
                        end
                    end
                end
            end)

            -- 2) Stretch visible mesh on the same local axis as collision.
            pcall(function()
                local mcls = FindClass("StaticMeshComponent")
                if not mcls then return end
                local comps = f:Call("K2_GetComponentsByClass", mcls)
                if not comps then return end
                for i = 1, #comps do
                    local mc = comps[i]
                    if is_live(mc) then
                        local n = ""
                        pcall(function() n = tostring(mc:GetName() or "") end)
                        if n == "" then n = tostring(i) end
                        local key = actor_name .. "::" .. n
                        if not flipper_mesh_scales_cache[key] then
                            pcall(function()
                                local rs = mc:Get("RelativeScale3D")
                                if rs then
                                    flipper_mesh_scales_cache[key] = { X = rs.X, Y = rs.Y, Z = rs.Z }
                                end
                            end)
                        end
                        local base = flipper_mesh_scales_cache[key]
                        if base then
                            local mx, my, mz = base.X, base.Y, base.Z
                            if length_axis == "X" then mx = base.X * sx end
                            if length_axis == "Y" then my = base.Y * sx end
                            if length_axis == "Z" then mz = base.Z * sx end
                            pcall(function() mc:Call("SetRelativeScale3D", { X = mx, Y = my, Z = mz }) end)
                        end
                    end
                end
            end)

            ok_count = ok_count + 1
        end
    end

    stats.flipper_applies = stats.flipper_applies + ok_count
    return ok_count > 0, string.format("flippers scaled=%d @(%.2f,1.00,1.00)", ok_count, sx)
end

local function cheats_set_big_ball(enable)
    cheats.big_ball = not not enable
    local ok, msg = apply_big_ball()
    return cheats.big_ball, (ok and msg) or ("pending: " .. tostring(msg))
end

local function cheats_toggle_big_ball()
    return cheats_set_big_ball(not cheats.big_ball)
end

local function cheats_set_large_flippers(enable)
    cheats.large_flippers = not not enable
    local ok, msg = apply_large_flippers()
    return cheats.large_flippers, (ok and msg) or ("pending: " .. tostring(msg))
end

local function cheats_toggle_large_flippers()
    return cheats_set_large_flippers(not cheats.large_flippers)
end

local function get_flipper_scale_x()
    return FLIPPER_SCALE_X
end

-- Set flipper length (X scale, 1.0 = none/default, up to 5.0).
-- Immediately re-applies flipper scale if flippers are currently active.
local function set_flipper_length_x(val)
    val = math.max(1.0, math.min(5.0, tonumber(val) or 1.0))
    FLIPPER_SCALE_X = val
    if val > 1.0 then
        cheats.large_flippers = true
    else
        cheats.large_flippers = false
    end
    local ok, msg = apply_large_flippers()
    return val, (ok and msg) or ("pending: " .. tostring(msg))
end

local function cheats_reset_scale()
    cheats.big_ball = false
    cheats.large_flippers = false
    local bok, bmsg = apply_big_ball()
    local fok, fmsg = apply_large_flippers()
    return (bok and fok), "reset ball=" .. tostring(bmsg) .. " flippers=" .. tostring(fmsg)
end

local function cheats_status_line()
    local drain_list = {}
    for k in pairs(DRAIN_STATES) do drain_list[#drain_list+1] = k end
    table.sort(drain_list)
    return string.format(
        "%s v1: ballsave=%s logstates=%s big_ball=%s large_flippers=%s | saves=%d end_blocked=%d states_seen=%d | apply(ball=%d,flipper=%d) | drain_states={%s}",
        TAG,
        tostring(cheats.infinite_ball_save),
        tostring(cheats.log_game_states),
        tostring(cheats.big_ball),
        tostring(cheats.large_flippers),
        stats.ball_saves,
        stats.game_end_blocked,
        stats.state_changes,
        stats.big_ball_applies,
        stats.flipper_applies,
        table.concat(drain_list, ",")
    )
end

-- ============================================================================
-- INFINITE BALL SAVE
-- Hook: PFXGameflowObject_PlayTable:OnGameStateChanged (BlueprintEvent)
-- Pre-hook fires BEFORE the BP graph runs — we can inspect the new state
-- and call SkipPrepareGameEnd() to cancel drain sequences.
-- ============================================================================

-- Pre-hook: intercept state change BEFORE the BP graph handles it
pcall(function()
    RegisterHook("PFXGameflowObject_PlayTable:OnGameStateChanged", function(ctx, state_raw)
        V("OnGameStateChanged hook fired")
        local newState = 0
        pcall(function() newState = ReadU8(state_raw) end)
        stats.state_changes = stats.state_changes + 1

        if cheats.log_game_states then
            Log(TAG .. ": [GameState] -> " .. tostring(newState))
        end

        if cheats.infinite_ball_save and DRAIN_STATES[newState] then
            -- Try to skip prepare-game-end (cancels the drain sequence)
            local pt = nil
            pcall(function() pt = ctx end)
            if pt and is_live(pt) then
                local ok = pcall(function() pt:Call("SkipPrepareGameEnd") end)
                if ok then
                    stats.ball_saves = stats.ball_saves + 1
                    Log(TAG .. ": [BallSave] Blocked drain at state=" .. newState
                        .. " (saves=" .. stats.ball_saves .. ")")
                end
            end
        end

        -- Reapply scale cheats after state transitions / ball respawns
        if cheats.big_ball then pcall(apply_big_ball) end
        if cheats.large_flippers then pcall(apply_large_flippers) end
    end)
    Log(TAG .. ": OnGameStateChanged hook registered")
end)

pcall(function()
    RegisterHook("GFO_PlayTable_C:OnTableRestart__DelegateSignature", function()
        -- Table restart recreates actors; re-apply active scale cheats shortly after
        ExecuteWithDelay(1200, function()
            if cheats.big_ball then pcall(apply_big_ball) end
            if cheats.large_flippers then pcall(apply_large_flippers) end
        end)
    end)
    Log(TAG .. ": OnTableRestart hook registered (scale reapply)")
end)

pcall(function()
    RegisterHook("GFO_PlayTable_C:OnTableStartBegin", function()
        -- BP table-start path (decompiled) recreates runtime actors; reapply shortly after
        ExecuteWithDelay(900, function()
            if cheats.big_ball then pcall(apply_big_ball) end
            if cheats.large_flippers then pcall(apply_large_flippers) end
        end)
    end)
    Log(TAG .. ": OnTableStartBegin hook registered (scale reapply)")
end)

pcall(function()
    RegisterHook("GFO_PlayTable_C:OnGameResumed", function()
        -- Resume path can rebuild/correct actor state; reapply active scales
        ExecuteWithDelay(400, function()
            if cheats.big_ball then pcall(apply_big_ball) end
            if cheats.large_flippers then pcall(apply_large_flippers) end
        end)
    end)
    Log(TAG .. ": OnGameResumed hook registered (scale reapply)")
end)

pcall(function()
    RegisterHook("GFO_PlayTable_C:BP_OnActivated", function()
        -- PlayTable activation grabs handlers and initializes runtime actors
        ExecuteWithDelay(1600, function()
            if cheats.big_ball then pcall(apply_big_ball) end
            if cheats.large_flippers then pcall(apply_large_flippers) end
        end)
    end)
    Log(TAG .. ": BP_OnActivated hook registered (scale reapply)")
end)

-- Post-hook on OnGameEnd: if ball save enabled and game ends, restart
-- (catches cases where drain slipped through state hook)
pcall(function()
    RegisterHook("PFXGameflowObject_PlayTable:OnGameEnd", function(ctx)
        V("OnGameEnd hook fired, ballsave=%s", tostring(cheats.infinite_ball_save))
        if not cheats.infinite_ball_save then return end
        local pt = nil
        pcall(function() pt = ctx end)
        if pt and is_live(pt) then
            local ok = pcall(function() pt:Call("SkipPrepareGameEnd") end)
            if ok then
                stats.game_end_blocked = stats.game_end_blocked + 1
                Log(TAG .. ": [BallSave] OnGameEnd blocked (total=" .. stats.game_end_blocked .. ")")
            end
        end
    end)
    Log(TAG .. ": OnGameEnd hook registered")
end)

-- ============================================================================
-- WRIST MENU INJECTION (stub — hooks BP_WristMenu_C when it opens in-game)
-- This logs available entries and attempts to inject a Cheats tab.
-- ============================================================================
pcall(function()
    RegisterHook("BP_WristMenu_C:Open", function(ctx)
        V("BP_WristMenu_C:Open hook fired")
        Log(TAG .. ": [WristMenu] Opened — cheats available via bridge commands")
        -- Future: inject cheat entries into the wrist menu entries array
        -- For now just log so we can probe the menu structure
        pcall(function()
            local wm = ctx
            if not is_live(wm) then return end
            local wn = "?"
            pcall(function() wn = wm:GetName() end)
            Log(TAG .. ": [WristMenu] instance=" .. wn)
            force_wrist_menu_open_short(wm)
        end)
    end)
    Log(TAG .. ": WristMenu:Open hook registered")
end)

pcall(function()
    RegisterPreHook("BP_Pawn_Base_C:SetWristMenuEnabled", function(self, funcPtr, parms)
        if not wrist_guard.enabled then return end
        if not in_table_gameplay() then return end

        local now = os.clock()
        local dt = now - (wrist_guard.last_open_t or 0)
        if dt < 0 or dt > 2.2 then return end

        local bEnable = 1
        pcall(function() bEnable = ReadU8(parms) end)
        if bEnable == 0 then
            wrist_guard.blocked_close = wrist_guard.blocked_close + 1
            return "BLOCK"
        end
    end)
    Log(TAG .. ": Wrist menu close guard hook registered")
end)

-- ============================================================================
-- BRIDGE COMMANDS
-- ============================================================================

-- Toggle infinite ball save
pcall(function()
    RegisterCommand("cheats_ballsave", function(args)
        local enable = (args == nil or args == "" or args == "1" or args == "true")
        cheats_set_ballsave(enable)
        return TAG .. ": infinite_ball_save = " .. tostring(cheats.infinite_ball_save)
    end)
end)

-- Toggle state logging
pcall(function()
    RegisterCommand("cheats_logstates", function(args)
        local enable = (args == nil or args == "" or args == "1" or args == "true")
        cheats_set_logstates(enable)
        return TAG .. ": log_game_states = " .. tostring(cheats.log_game_states)
    end)
end)

-- Add/remove drain state numbers
pcall(function()
    RegisterCommand("cheats_drainstate", function(args)
        local n = tonumber(args)
        if not n then return "usage: cheats_drainstate <number>" end
        DRAIN_STATES[math.floor(n)] = true
        Log(TAG .. ": Added drain state " .. n)
        local states = {}
        for k in pairs(DRAIN_STATES) do states[#states+1] = k end
        table.sort(states)
        return TAG .. ": drain states = {" .. table.concat(states, ",") .. "}"
    end)
end)

-- Manual ball save trigger (call while in game if auto-hook misses)
pcall(function()
    RegisterCommand("cheats_saveball", function()
        local ok, msg = cheats_saveball()
        if ok then return TAG .. ": " .. msg end
        return TAG .. ": " .. msg
    end)
end)

-- Force restart current ball (if drain already happened)
pcall(function()
    RegisterCommand("cheats_restartball", function()
        local _, msg = cheats_restartball()
        return TAG .. ": " .. msg
    end)
end)

-- Pause/resume game
pcall(function()
    RegisterCommand("cheats_pause", function()
        local _, msg = cheats_pause_resume()
        return TAG .. ": " .. msg
    end)
end)

pcall(function()
    RegisterCommand("cheats_bigball", function(args)
        local enable = (args == nil or args == "" or args == "1" or args == "true")
        local on, msg = cheats_set_big_ball(enable)
        return TAG .. ": big_ball=" .. tostring(on) .. " | " .. tostring(msg)
    end)
end)

pcall(function()
    RegisterCommand("cheats_flippers", function(args)
        local enable = (args == nil or args == "" or args == "1" or args == "true")
        local on, msg = cheats_set_large_flippers(enable)
        return TAG .. ": large_flippers=" .. tostring(on) .. " | " .. tostring(msg)
    end)
end)

pcall(function()
    RegisterCommand("cheats_reapply_scale", function()
        local aok, amsg = apply_big_ball()
        local fok, fmsg = apply_large_flippers()
        return TAG .. ": reapply big_ball=" .. tostring(aok) .. "(" .. tostring(amsg) .. ")"
            .. " flippers=" .. tostring(fok) .. "(" .. tostring(fmsg) .. ")"
    end)
end)

pcall(function()
    RegisterCommand("cheats_reset_scale", function()
        local ok, msg = cheats_reset_scale()
        return TAG .. ": reset_scale ok=" .. tostring(ok) .. " | " .. tostring(msg)
    end)
end)

pcall(function()
    RegisterCommand("cheats_flipper_length", function(args)
        local val, msg = set_flipper_length_x(tonumber(args) or 1.0)
        return TAG .. ": flipper_length=" .. string.format("%.2f", val) .. " | " .. tostring(msg)
    end)
end)

pcall(function()
    RegisterCommand("cheats_probe_scale", function()
        return TAG .. ": " .. probe_scale_targets()
    end)
end)

-- Status dump
pcall(function()
    RegisterCommand("cheats_status", function()
        return cheats_status_line()
            .. string.format(" | wrist_guard=%s blocked=%d", tostring(wrist_guard.enabled), wrist_guard.blocked_close)
    end)
end)

pcall(function()
    RegisterCommand("cheats_wristguard", function(args)
        if args == "0" or args == "false" or args == "off" then
            wrist_guard.enabled = false
        elseif args == "1" or args == "true" or args == "on" or args == nil or args == "" then
            wrist_guard.enabled = true
        else
            wrist_guard.enabled = not wrist_guard.enabled
        end
        return TAG .. ": wrist_guard=" .. tostring(wrist_guard.enabled)
            .. " blocked=" .. tostring(wrist_guard.blocked_close)
    end)
end)

-- Global API for other mods (e.g. PFX_ModMenu)
PFX_Cheats = nil
_G.PFX_Cheats = {
    cheats = cheats,
    stats = stats,
    set_ball_save = cheats_set_ballsave,
    toggle_ball_save = cheats_toggle_ballsave,
    set_log_states = cheats_set_logstates,
    toggle_log_states = cheats_toggle_logstates,
    set_big_ball = cheats_set_big_ball,
    toggle_big_ball = cheats_toggle_big_ball,
    set_large_flippers = cheats_set_large_flippers,
    toggle_large_flippers = cheats_toggle_large_flippers,
    get_flipper_scale_x = get_flipper_scale_x,
    set_flipper_length_x = set_flipper_length_x,
    reset_scale = cheats_reset_scale,
    apply_big_ball = apply_big_ball,
    apply_large_flippers = apply_large_flippers,
    save_ball = cheats_saveball,
    restart_ball = cheats_restartball,
    pause_resume = cheats_pause_resume,
    status = cheats_status_line,
}
Log(TAG .. ": global API exported (_G.PFX_Cheats)")

-- Max all perks right now via CheatManager (works in-game)
pcall(function()
    RegisterCommand("cheats_maxperks", function()
        local cm = find_live("PFXCheatManager")
        if not cm then return "PFXCheatManager not found" end
        local ok = pcall(function() cm:Call("PFXDebug_TablePerkMaxAll") end)
        return TAG .. ": PFXDebug_TablePerkMaxAll ok=" .. tostring(ok)
    end)
end)

-- Max all masteries via CheatManager
pcall(function()
    RegisterCommand("cheats_maxmasteries", function()
        local cm = find_live("PFXCheatManager")
        if not cm then return "PFXCheatManager not found" end
        local ok = pcall(function() cm:Call("PFXDebug_TableMasteryMaxAll") end)
        return TAG .. ": PFXDebug_TableMasteryMaxAll ok=" .. tostring(ok)
    end)
end)

-- Unlock all collectibles via CheatManager
pcall(function()
    RegisterCommand("cheats_unlockall", function()
        local cm = find_live("PFXCheatManager")
        if not cm then return "PFXCheatManager not found" end
        local ok = pcall(function() cm:Call("PFXDebug_Collectibles_UnlockAll", false) end)
        return TAG .. ": PFXDebug_Collectibles_UnlockAll ok=" .. tostring(ok)
    end)
end)

-- Max all championship via CheatManager
pcall(function()
    RegisterCommand("cheats_maxchamp", function()
        local cm = find_live("PFXCheatManager")
        if not cm then return "PFXCheatManager not found" end
        local ok = pcall(function() cm:Call("PFXDebug_Championship_MaxAll") end)
        return TAG .. ": PFXDebug_Championship_MaxAll ok=" .. tostring(ok)
    end)
end)

Log(TAG .. ": v1 loaded — ball_save=" .. tostring(cheats.infinite_ball_save)
    .. " log_states=" .. tostring(cheats.log_game_states)
    .. " big_ball=" .. tostring(cheats.big_ball)
    .. " large_flippers=" .. tostring(cheats.large_flippers))
Log(TAG .. ": Bridge cmds: cheats_status, cheats_ballsave, cheats_saveball, cheats_restartball")
Log(TAG .. ": Bridge cmds: cheats_pause, cheats_logstates, cheats_drainstate <n>")
Log(TAG .. ": Bridge cmds: cheats_bigball 0|1, cheats_flippers 0|1, cheats_reapply_scale")
Log(TAG .. ": Bridge cmds: cheats_maxperks, cheats_maxmasteries, cheats_unlockall, cheats_maxchamp")
