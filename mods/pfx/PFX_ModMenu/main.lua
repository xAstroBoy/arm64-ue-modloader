-- ============================================================================
-- PFX_ModMenu v8 — In-Game Mod Menu via Legal Section Takeover
-- ============================================================================
-- v8 BEAUTIFUL LAYOUT — Native game widget types:
--   Section headers  → disabled WBP_Button_Default_C
--   Toggles          → WBP_OptionsEntry_C Type=1 (native OFF/ON carousel switcher)
--   Sliders          → WBP_OptionsEntry_C Type=2 (numeric slider)
--   Action buttons   → WBP_Button_Default_C
--
-- Hooks (v8):
--   + WBP_OptionsEntry_C:HandleSwitcherValueChanged   (toggle dispatch)
--   + WBP_OptionsEntry_C:BndEvt..._Slider_..._OnValueChanged  (slider)
--   + WBP_Button_Default_C:BndEvt..._ComponentBoundEvent_0_...  (action buttons)
--   - WBP_CheckboxFilterButton_C:IsCheckedChanged     (REMOVED — no longer used)
--
-- v7 ROOT CAUSE (preserved): BndEvt hook name had "K2Node_" prefix that
--   does NOT exist in the decompiled Blueprint. Fixed: "ComponentBoundEvent_0".
-- ============================================================================
local TAG = "PFX_ModMenu"
local VERBOSE = true
local function V(...) if VERBOSE then Log(TAG .. " [V] " .. string.format(...)) end end
Log(TAG .. ": Loading v8...")

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

local function set_button_text(btn, text)
    if not is_live(btn) then return false end
    pcall(function() btn:Call("SetTitleText", text) end)
    pcall(function() btn:Set("Button Title Text", text) end)
    return true
end

local function clear_button_icon(btn)
    if not is_live(btn) then return end
    pcall(function() btn:Set("Icon Texture", nil) end)
    pcall(function()
        local nm = btn:Get("NotificationMark")
        if nm then nm:Set("Visibility", 1) end  -- 1 = Collapsed
    end)
end

-- ============================================================================
-- STATE
-- ============================================================================
local ball_save_on    = true
local log_states_on   = true
local panel_built     = false
local built_instances = {}          -- [widget_obj] = true once panel is built
local action_buttons  = {}          -- [idx] = widget (sparse — headers have no entry)
local btn_to_idx      = {}          -- [widget_obj] = idx  (O(1) click dispatch)
local syncing_toggle_widgets = {}   -- [widget_obj] = true while programmatically syncing

local click_stats = {
    hits    = 0,
    actions = 0,
    misses  = 0,
    last_id = "",
    last_src= "",
}

-- ============================================================================
-- ACTIONS
-- ============================================================================
-- Fields:
--   header = true    → section divider (non-clickable, no btn_to_idx entry)
--   toggle = true    → WBP_OptionsEntry_C Type=1 (OFF/ON carousel)
--   slider = true    → WBP_OptionsEntry_C Type=2 (numeric slider)
--   (plain)          → WBP_Button_Default_C (fires once, flashes result)
--   label            → string or function()->string
-- ============================================================================
local ACTIONS = {

    -- ── CHEATS ───────────────────────────────────────────────────────────────
    { id = "hdr_cheats", header = true, label = "── CHEATS ──" },

    {
        id     = "ball_save",
        toggle = true,
        label  = "Ball Save (Infinite)",
        fn = function()
            local api = rawget(_G, "PFX_Cheats")
            if api and api.toggle_ball_save then
                ball_save_on = not not api.toggle_ball_save()
                return "Ball Save: " .. (ball_save_on and "ON" or "OFF")
            end
            ball_save_on = not ball_save_on
            return "Ball Save: " .. (ball_save_on and "ON" or "OFF") .. " (local)"
        end,
    },
    {
        id     = "big_ball",
        toggle = true,
        label  = "Big Ball",
        fn = function()
            local api = rawget(_G, "PFX_Cheats")
            if api and api.toggle_big_ball then
                local on, msg = api.toggle_big_ball()
                return "BigBall: " .. tostring(msg or on)
            end
            return "BigBall: PFX_Cheats not loaded"
        end,
    },
    {
        id     = "large_flippers",
        toggle = true,
        label  = "Large Flippers",
        fn = function()
            local api = rawget(_G, "PFX_Cheats")
            if api and api.toggle_large_flippers then
                local on, msg = api.toggle_large_flippers()
                return "Flippers: " .. tostring(msg or on)
            end
            return "Flippers: PFX_Cheats not loaded"
        end,
    },
    {
        id        = "flipper_length",
        slider    = true,
        label     = "Flipper Length",
        min       = 1.0,
        max       = 5.0,
        step      = 0.5,
        get_value = function()
            local api = rawget(_G, "PFX_Cheats")
            return (api and api.get_flipper_scale_x and api.get_flipper_scale_x()) or 1.0
        end,
        fn = function(val)
            local api = rawget(_G, "PFX_Cheats")
            if api and api.set_flipper_length_x then
                api.set_flipper_length_x(val)
                Log(TAG .. ": [slider] flipper_length -> " .. string.format("%.2f", val))
            end
        end,
    },
    {
        id     = "finger_mode",
        toggle = true,
        label  = "Finger Mode",
        fn = function()
            local api = rawget(_G, "PFX_FingerMode")
            if api and api.toggle then
                local on = api.toggle()
                return "Finger: " .. (on and "ON" or "OFF")
            end
            return "Finger: PFX_FingerMode not loaded"
        end,
    },
    {
        id     = "cheat_logstates",
        toggle = true,
        label  = "Log Game States",
        fn = function()
            local api = rawget(_G, "PFX_Cheats")
            if api and api.toggle_log_states then
                log_states_on = not not api.toggle_log_states()
                return "LogStates: " .. (log_states_on and "ON" or "OFF")
            end
            log_states_on = not log_states_on
            return "LogStates: " .. (log_states_on and "ON" or "OFF") .. " (local)"
        end,
    },

    -- ── ACTIONS ──────────────────────────────────────────────────────────────
    { id = "hdr_actions", header = true, label = "── ACTIONS ──" },

    {
        id    = "cheat_saveball",
        label = "Save Ball NOW",
        fn = function()
            local api = rawget(_G, "PFX_Cheats")
            if api and api.save_ball then
                local ok, msg = api.save_ball()
                return "SaveBall: " .. tostring(msg)
            end
            return "SaveBall: PFX_Cheats not loaded"
        end,
    },
    {
        id    = "cheat_pause",
        label = "Pause / Resume",
        fn = function()
            local api = rawget(_G, "PFX_Cheats")
            if api and api.pause_resume then
                local _, msg = api.pause_resume()
                return "Pause: " .. tostring(msg)
            end
            return "Pause: PFX_Cheats not loaded"
        end,
    },
    {
        id    = "cheat_restartball",
        label = "Restart Ball",
        fn = function()
            local api = rawget(_G, "PFX_Cheats")
            if api and api.restart_ball then
                local _, msg = api.restart_ball()
                return "Restart: " .. tostring(msg)
            end
            return "Restart: PFX_Cheats not loaded"
        end,
    },

    -- ── MAX & UNLOCK ──────────────────────────────────────────────────────────
    { id = "hdr_max", header = true, label = "── MAX & UNLOCK ──" },

    {
        id    = "max_all",
        label = "Max All + Unlock All",
        fn = function()
            local api = rawget(_G, "PFX_Max")
            if api and api.run then
                pcall(api.run)
                return "MaxAll: done"
            end
            pcall(function()
                local cm = FindFirstOf("PFXCheatManager")
                if is_live(cm) then
                    cm:Call("PFXDebug_TablePerkMaxAll")
                    cm:Call("PFXDebug_TableMasteryMaxAll")
                    cm:Call("PFXDebug_Collectibles_UnlockAll", false)
                end
            end)
            return "MaxAll: done (fallback)"
        end,
    },
    {
        id    = "fix_trophies",
        label = "Fix Trophies -> Physical",
        fn = function()
            local api = rawget(_G, "PFX_Max")
            if api and api.fix_trophies then
                local n = 0
                pcall(function() n = api.fix_trophies() or 0 end)
                return "Trophies: " .. n .. " swapped"
            end
            return "Trophies: PFX_Max not loaded"
        end,
    },

    -- ── RANDOMIZE ─────────────────────────────────────────────────────────────
    { id = "hdr_rand", header = true, label = "── RANDOMIZE ──" },

    {
        id    = "rand_all",
        label = "Randomize All Hub Slots",
        fn = function()
            local api = rawget(_G, "PFX_Rand")
            if api then
                local n = 0
                pcall(function() n = api.scramble_all() or 0 end)
                return "Rand all: " .. n .. " slots"
            end
            return "Rand all: PFX_Rand not loaded"
        end,
    },
    {
        id    = "rand_tables",
        label = "Randomize Table Cosmetics",
        fn = function()
            local api = rawget(_G, "PFX_Rand")
            if api then pcall(api.scramble_tables); return "Rand tables: done" end
            return "Rand tables: PFX_Rand not loaded"
        end,
    },
    {
        id    = "rand_wall",
        label = "Randomize Walls",
        fn = function()
            local api = rawget(_G, "PFX_Rand")
            if api then
                local n = 0
                pcall(function() n = api.scramble_cat(api.CAT_WALL) or 0 end)
                return "Rand Wall: " .. n
            end
            return "Rand Wall: PFX_Rand not loaded"
        end,
    },
    {
        id    = "rand_floor",
        label = "Randomize Floors",
        fn = function()
            local api = rawget(_G, "PFX_Rand")
            if api then
                local n = 0
                pcall(function() n = api.scramble_cat(api.CAT_FLOOR) or 0 end)
                return "Rand Floor: " .. n
            end
            return "Rand Floor: PFX_Rand not loaded"
        end,
    },
    {
        id    = "rand_poster",
        label = "Randomize Posters",
        fn = function()
            local api = rawget(_G, "PFX_Rand")
            if api then
                local n = 0
                pcall(function() n = api.scramble_cat(api.CAT_POSTER) or 0 end)
                return "Rand Poster: " .. n
            end
            return "Rand Poster: PFX_Rand not loaded"
        end,
    },
    {
        id    = "rand_statue",
        label = "Randomize Statues",
        fn = function()
            local api = rawget(_G, "PFX_Rand")
            if api then
                local n = 0
                pcall(function() n = api.scramble_cat(api.CAT_STATUE) or 0 end)
                return "Rand Statue: " .. n
            end
            return "Rand Statue: PFX_Rand not loaded"
        end,
    },
    {
        id    = "rand_gadget",
        label = "Randomize Gadgets",
        fn = function()
            local api = rawget(_G, "PFX_Rand")
            if api then
                local n = 0
                pcall(function() n = api.scramble_cat(api.CAT_GADGET) or 0 end)
                return "Rand Gadget: " .. n
            end
            return "Rand Gadget: PFX_Rand not loaded"
        end,
    },
    {
        id    = "rand_hub",
        label = "Randomize Hub Interior",
        fn = function()
            local api = rawget(_G, "PFX_Rand")
            if api then
                local n = 0
                pcall(function() n = api.scramble_cat(api.CAT_HUB) or 0 end)
                return "Rand Hub: " .. n
            end
            return "Rand Hub: PFX_Rand not loaded"
        end,
    },
}

-- ============================================================================
-- ACTION HELPERS
-- ============================================================================
local function get_action_label(idx)
    local a = ACTIONS[idx]
    if not a then return "?" end
    if type(a.label) == "function" then return a.label() end
    return tostring(a.label)
end

-- Returns the actual current toggle state by querying the relevant API.
local function action_toggle_state(idx)
    local a = ACTIONS[idx]
    if not a or not a.toggle then return false end
    local id  = a.id
    local api = rawget(_G, "PFX_Cheats")
    if id == "ball_save" then
        if api and api.cheats then return not not api.cheats.infinite_ball_save end
        return not not ball_save_on
    elseif id == "big_ball" then
        return (api and api.cheats and not not api.cheats.big_ball) or false
    elseif id == "large_flippers" then
        return (api and api.cheats and not not api.cheats.large_flippers) or false
    elseif id == "finger_mode" then
        local fapi = rawget(_G, "PFX_FingerMode")
        if fapi then
            if fapi.is_enabled then
                local ok, v = pcall(function() return fapi.is_enabled() end)
                if ok then return not not v end
            end
            if fapi.enabled ~= nil then return not not fapi.enabled end
        end
        return false
    elseif id == "cheat_logstates" then
        if api and api.cheats then return not not api.cheats.log_game_states end
        return not not log_states_on
    end
    return false
end

-- Programmatically sync the OptionsEntry Type=1 switcher widget to actual toggle state.
-- Uses syncing guard so HandleSwitcherValueChanged ignores our own updates.
local function sync_switcher_widget(idx)
    local btn = action_buttons[idx]
    if not is_live(btn) then return end
    local on = action_toggle_state(idx)
    local target_idx = on and 1 or 0
    syncing_toggle_widgets[btn] = true
    pcall(function()
        local os_widget = btn:Get("OptionSwitcher")
        if is_live(os_widget) then
            pcall(function() os_widget:Call("SetSelectedIndex", target_idx) end)
            pcall(function() os_widget:Set("SelectedIndex", target_idx) end)
        end
        pcall(function() btn:Set("SelectedOptionIndex", target_idx) end)
        pcall(function() btn:Call("UpdateSelection") end)
    end)
    syncing_toggle_widgets[btn] = nil
end

-- Core dispatcher — called for plain action buttons and bridge commands.
-- For toggles, called only from bridge_exec (HandleSwitcherValueChanged handles UI).
local function dispatch_action(idx, source)
    local a = ACTIONS[idx]
    if not a or a.header or a.slider then return end

    click_stats.actions = click_stats.actions + 1
    click_stats.last_id  = a.id
    click_stats.last_src = source

    local result = "?"
    local ok_fn  = pcall(function() result = tostring(a.fn() or get_action_label(idx)) end)
    if not ok_fn then result = "ERROR" end

    Log(TAG .. ": [" .. source .. "] [" .. a.id .. "] -> " .. result)

    local btn = action_buttons[idx]
    if a.toggle then
        -- Sync the OptionsEntry carousel to reflect the new state
        if is_live(btn) then sync_switcher_widget(idx) end
    else
        -- Flash result text on the button, restore label after 2 s
        if is_live(btn) then set_button_text(btn, result) end
        pcall(function()
            ExecuteWithDelay(2000, function()
                local b = action_buttons[idx]
                if is_live(b) then set_button_text(b, get_action_label(idx)) end
            end)
        end)
    end
end

-- ============================================================================
-- SETTINGS WIDGET PROPERTY ACCESS
-- ============================================================================
local function get_legal_button(w)
    local lb = nil
    pcall(function() lb = w:Get("WBP_Button_Legal") end)
    return is_live(lb) and lb or nil
end

local function get_llc(w)
    local llc = nil
    pcall(function() llc = w:Get("LegalLineContainer") end)
    return is_live(llc) and llc or nil
end

local function rename_legal_button_all()
    local all = nil
    pcall(function() all = FindAllOf("WBP_WristMenuSettings_C") end)
    if not all then return end
    for _, w in ipairs(all) do
        if is_live(w) then
            pcall(function()
                local lb = get_legal_button(w)
                if not lb then return end
                set_button_text(lb, "MODS")
                clear_button_icon(lb)
                pcall(function() lb:Call("SetHasNotificationMark", true) end)
            end)
        end
    end
end

-- ============================================================================
-- BUILD MOD PANEL (v8)
-- Populates LegalLineContainer using native game widget types.
-- ============================================================================
local function build_mod_panel_on(w)
    if not is_live(w) then return false end
    if built_instances[w] then return true end

    -- Rename nav button
    local lb = get_legal_button(w)
    if is_live(lb) then set_button_text(lb, "MODS") end

    -- Get LegalLineContainer — may need switcher nudge to activate it
    local llc = get_llc(w)
    if not is_live(llc) then
        pcall(function()
            local ss = w:Get("SubmenuSwitcher")
            if is_live(ss) then
                local nw = ss:Call("GetNumWidgets") or 0
                if nw > 0 then ss:Call("SetActiveWidgetIndex", nw - 1) end
            end
        end)
        llc = get_llc(w)
    end
    if not is_live(llc) then return false end

    pcall(function() llc:Call("ClearChildren") end)

    local pc = nil
    pcall(function() pc = FindFirstOf("BP_PlayerController_C") end)
    if not pc then return false end

    -- ── Title banner ─────────────────────────────────────────────────────────
    local title_btn = nil
    pcall(function() title_btn = CreateWidget("WBP_Button_Default_C", pc) end)
    if is_live(title_btn) then
        pcall(function() llc:Call("AddChild", title_btn) end)
        set_button_text(title_btn, "PFX MOD MENU  v8")
        clear_button_icon(title_btn)
        pcall(function() title_btn:Set("IsEnabled", false) end)
    end

    -- ── Build all action rows ─────────────────────────────────────────────────
    for idx = 1, #ACTIONS do
        local a = ACTIONS[idx]
        if not a then goto continue end

        -- ── Section header ────────────────────────────────────────────────────
        if a.header then
            local sep = nil
            pcall(function() sep = CreateWidget("WBP_Button_Default_C", pc) end)
            if is_live(sep) then
                pcall(function() llc:Call("AddChild", sep) end)
                set_button_text(sep, a.label)
                clear_button_icon(sep)
                pcall(function() sep:Set("IsEnabled", false) end)
            end

        -- ── Toggle → WBP_OptionsEntry_C Type=1 (native OFF/ON carousel) ──────
        elseif a.toggle then
            local entry = nil
            pcall(function() entry = CreateWidget("WBP_OptionsEntry_C", pc) end)
            if is_live(entry) then
                pcall(function() llc:Call("AddChild", entry) end)
                -- Configure as Type=1 (carousel/switcher)
                pcall(function() entry:Set("Type", 1) end)
                pcall(function() entry:Set("Title", get_action_label(idx)) end)
                -- Set up the OptionSwitcher child with OFF/ON options
                local init_idx = action_toggle_state(idx) and 1 or 0
                pcall(function()
                    local os_widget = entry:Get("OptionSwitcher")
                    if is_live(os_widget) then
                        -- SetOptions with OFF/ON labels (UE4SS marshals Lua table -> TArray<FText>)
                        pcall(function() os_widget:Call("SetOptions", {"OFF", "ON"}) end)
                        pcall(function() os_widget:Call("SetSelectedIndex", init_idx) end)
                        pcall(function() os_widget:Set("SelectedIndex", init_idx) end)
                    end
                end)
                pcall(function() entry:Set("SelectedOptionIndex", init_idx) end)
                pcall(function() entry:Call("SetupEntry") end)
                pcall(function() entry:Call("PreInitButtons") end)
                pcall(function() entry:Call("UpdateSelection") end)
                action_buttons[idx] = entry
                btn_to_idx[entry]   = idx
            end

        -- ── Slider → WBP_OptionsEntry_C Type=2 (numeric slider) ──────────────
        elseif a.slider then
            local entry = nil
            pcall(function() entry = CreateWidget("WBP_OptionsEntry_C", pc) end)
            if is_live(entry) then
                pcall(function() llc:Call("AddChild", entry) end)
                pcall(function() entry:Set("Type", 2) end)
                pcall(function() entry:Set("Title", a.label) end)
                pcall(function() entry:Set("SliderMinValue", a.min) end)
                pcall(function() entry:Set("SliderMaxValue", a.max) end)
                pcall(function() entry:Set("SliderStepSize", a.step) end)
                local cv = 1.0
                pcall(function() cv = a.get_value() end)
                pcall(function() entry:Set("SelectedSliderValue", cv) end)
                pcall(function() entry:Call("SetupEntry") end)
                pcall(function() entry:Call("RefreshValue") end)
                action_buttons[idx] = entry
                btn_to_idx[entry]   = idx
            end

        -- ── Plain action → WBP_Button_Default_C ──────────────────────────────
        else
            local btn = nil
            pcall(function() btn = CreateWidget("WBP_Button_Default_C", pc) end)
            if is_live(btn) then
                pcall(function() llc:Call("AddChild", btn) end)
                set_button_text(btn, get_action_label(idx))
                clear_button_icon(btn)
                action_buttons[idx] = btn
                btn_to_idx[btn]     = idx
            end
        end

        ::continue::
    end

    built_instances[w] = true
    panel_built = true

    local final = 0
    pcall(function() final = llc:Call("GetChildrenCount") end)
    local nm = ""
    pcall(function() nm = w:GetName() end)
    Log(TAG .. ": Panel built on " .. nm .. " — " .. final .. " items")

    -- Post-build: scroll to top, re-confirm MODS label + notification mark
    pcall(function() w:Call("ResetScroll") end)
    pcall(function()
        local lb2 = get_legal_button(w)
        if is_live(lb2) then
            set_button_text(lb2, "MODS")
            clear_button_icon(lb2)
            lb2:Call("SetHasNotificationMark", true)
        end
    end)
    return true
end

local function build_mod_panel()
    V("build_mod_panel")
    local all = nil
    pcall(function() all = FindAllOf("WBP_WristMenuSettings_C") end)
    if not all then all = {} end

    local built_count = 0
    for _, w in ipairs(all) do
        if not is_live(w) then goto cont end
        local llc = get_llc(w)
        if is_live(llc) then
            if build_mod_panel_on(w) then built_count = built_count + 1 end
        end
        ::cont::
    end
    return built_count
end

-- ============================================================================
-- HOOKS
-- ============================================================================

-- Construct: reset per-instance build flag, rename Legal→MODS
pcall(function()
    RegisterHook("WBP_WristMenuSettings_C:Construct", function(ctx)
        V("Construct fired")
        local self_obj = nil
        pcall(function() if is_live(ctx) then self_obj = ctx end end)
        if self_obj then built_instances[self_obj] = nil end
        pcall(rename_legal_button_all)
    end)
    Log(TAG .. ": WBP_WristMenuSettings_C:Construct hook registered")
end)

-- OnSubmenuButtonClicked: fires when any nav tab is tapped.
-- Build our panel here so content is ready before the switcher reveals it.
pcall(function()
    RegisterHook("WBP_WristMenuSettings_C:OnSubmenuButtonClicked", function(ctx)
        V("OnSubmenuButtonClicked fired")
        local self_obj = nil
        pcall(function() if is_live(ctx) then self_obj = ctx end end)
        if not is_live(self_obj) then return end
        pcall(function() build_mod_panel_on(self_obj) end)
        pcall(function() self_obj:Call("ResetScroll") end)
    end)
    Log(TAG .. ": WBP_WristMenuSettings_C:OnSubmenuButtonClicked hook registered")
end)

-- SelectStartingCategory: fires when settings open with a pre-selected tab.
pcall(function()
    RegisterHook("WBP_WristMenuSettings_C:SelectStartingCategory", function(ctx)
        V("SelectStartingCategory fired")
        local self_obj = nil
        pcall(function() if is_live(ctx) then self_obj = ctx end end)
        if is_live(self_obj) then pcall(function() build_mod_panel_on(self_obj) end) end
    end)
    Log(TAG .. ": WBP_WristMenuSettings_C:SelectStartingCategory hook registered")
end)

-- ── WBP_Button_Default_C CLICK ──────────────────────────────────────────────
-- ROOT CAUSE FIX: previous hook used "K2Node_ComponentBoundEvent_0" (wrong).
-- Correct name verified against WBP_Button_Default.cpp: "ComponentBoundEvent_0".
pcall(function()
    RegisterHook(
        "WBP_Button_Default_C:BndEvt__WBP_Button_Default_WBP_Button_Base_ComponentBoundEvent_0_OnButtonClicked__DelegateSignature",
        function(ctx)
            click_stats.hits = click_stats.hits + 1

            local btn = nil
            pcall(function() if is_live(ctx) then btn = ctx end end)
            if not is_live(btn) then
                pcall(function()
                    local c = ctx:get()
                    if is_live(c) then btn = c end
                end)
            end
            if not is_live(btn) then
                click_stats.misses = click_stats.misses + 1
                return
            end

            -- Skip nav buttons — OnSubmenuButtonClicked handles those
            local nm = ""
            pcall(function() nm = btn:GetName() end)
            if nm == "WBP_Button_Legal" or nm == "WBP_Button_VR"
            or nm == "WBP_Button_MR"   or nm == "WBP_Button_Tables"
            or nm == "WBP_Button_PP"   then
                return
            end

            local idx = btn_to_idx[btn]
            if not idx then
                click_stats.misses = click_stats.misses + 1
                V("click miss: no action for btn=%s", nm)
                return
            end

            dispatch_action(idx, "btn_default")
            return "BLOCK"
        end
    )
    Log(TAG .. ": WBP_Button_Default_C click hook registered")
end)

-- ── WBP_OptionsEntry_C TOGGLE (carousel switcher, Type=1) ───────────────────
-- Fires when the user clicks left/right arrow on a Type=1 OptionsEntry.
-- ctx = self (the WBP_OptionsEntry_C instance).
-- We read SelectedIndex from the OptionSwitcher child widget to determine new state.
pcall(function()
    RegisterHook("WBP_OptionsEntry_C:HandleSwitcherValueChanged", function(ctx)
        local entry = nil
        pcall(function() if is_live(ctx) then entry = ctx end end)
        if not is_live(entry) then return end

        -- Guard: ignore updates we trigger ourselves via sync_switcher_widget
        if syncing_toggle_widgets[entry] then return end

        local idx = btn_to_idx[entry]
        if not idx then return end
        local a = ACTIONS[idx]
        if not a or not a.toggle then return end

        -- Read the new selected index from OptionSwitcher child (or entry property)
        local opt_idx = 0
        pcall(function()
            local os_widget = entry:Get("OptionSwitcher")
            if is_live(os_widget) then
                opt_idx = tonumber(os_widget:Get("SelectedIndex")) or 0
            else
                opt_idx = tonumber(entry:Get("SelectedOptionIndex")) or 0
            end
        end)

        local new_state = (opt_idx == 1)  -- 0=OFF, 1=ON
        local cur_state = action_toggle_state(idx)

        if new_state ~= cur_state then
            click_stats.actions = click_stats.actions + 1
            click_stats.last_id  = a.id
            click_stats.last_src = "switcher"
            local result = "?"
            local ok_fn = pcall(function() result = tostring(a.fn() or a.id) end)
            if not ok_fn then result = "ERROR" end
            Log(TAG .. ": [switcher] [" .. a.id .. "] " .. (new_state and "ON" or "OFF") .. " -> " .. result)
        end

        -- Always sync widget back to actual state (handles failed toggles too)
        sync_switcher_widget(idx)
    end)
    Log(TAG .. ": WBP_OptionsEntry_C:HandleSwitcherValueChanged hook registered")
end)

-- ── WBP_OptionsEntry_C SLIDER (Type=2) ──────────────────────────────────────
pcall(function()
    RegisterHook(
        "WBP_OptionsEntry_C:BndEvt__WBP_OptionsEntry_Slider_ComponentBoundEvent_2_OnValueChanged__DelegateSignature",
        function(ctx)
            local self_obj = nil
            pcall(function() if is_live(ctx) then self_obj = ctx end end)
            if not is_live(self_obj) then return end

            local idx = btn_to_idx[self_obj]
            if not idx then return end
            local a = ACTIONS[idx]
            if not a or not a.slider then return end

            local val = 1.0
            pcall(function() val = tonumber(self_obj:Get("SelectedSliderValue")) or 1.0 end)
            pcall(a.fn, val)
        end
    )
    Log(TAG .. ": WBP_OptionsEntry_C slider hook registered")
end)

-- ============================================================================
-- BRIDGE COMMANDS
-- ============================================================================
pcall(function()
    RegisterCommand("modmenu_status", function()
        local lines = {
            string.format("%s v8 | actions=%d panel=%s", TAG, #ACTIONS, tostring(panel_built)),
            string.format("clicks: hits=%d actions=%d misses=%d last=[%s]@%s",
                click_stats.hits, click_stats.actions, click_stats.misses,
                click_stats.last_id, click_stats.last_src),
        }
        for i = 1, #ACTIONS do
            local a = ACTIONS[i]
            if a.header then
                lines[#lines + 1] = "  " .. a.label
            elseif not a.slider then
                local state = a.toggle and ("[" .. (action_toggle_state(i) and "ON" or "OF") .. "] ") or "[btn] "
                lines[#lines + 1] = "  " .. i .. ". " .. state .. get_action_label(i)
            else
                local cv = 0.0
                pcall(function() cv = a.get_value() end)
                lines[#lines + 1] = "  " .. i .. ". [sld=" .. string.format("%.1f", cv) .. "] " .. a.label
            end
        end
        return table.concat(lines, "\n")
    end)
end)

pcall(function()
    RegisterCommand("modmenu_exec", function(args)
        local idx = tonumber(args)
        if idx and ACTIONS[idx] and not ACTIONS[idx].header then
            dispatch_action(idx, "bridge_exec")
            return TAG .. ": exec done — " .. click_stats.last_id
        end
        local lines = { "Usage: modmenu_exec <1-" .. #ACTIONS .. ">  (skip headers)" }
        for i = 1, #ACTIONS do
            local a = ACTIONS[i]
            if not a.header then
                lines[#lines + 1] = "  " .. i .. ". " .. get_action_label(i)
            end
        end
        return table.concat(lines, "\n")
    end)
end)

pcall(function()
    RegisterCommand("modmenu_rebuild", function()
        panel_built     = false
        built_instances = {}
        action_buttons  = {}
        btn_to_idx      = {}
        local n = build_mod_panel()
        return n > 0 and (TAG .. ": rebuilt on " .. n .. " instance(s)")
            or TAG .. ": open Settings -> MODS tab first"
    end)
end)

pcall(function()
    RegisterCommand("modmenu_rename", function()
        pcall(rename_legal_button_all)
        return TAG .. ": rename attempted on all instances"
    end)
end)

-- Hot-reload: rename any already-open Legal buttons immediately
pcall(function()
    rename_legal_button_all()
    Log(TAG .. ": initial rename_legal_button_all() called")
end)

-- ============================================================================
Log(TAG .. ": v8 ready — " .. #ACTIONS .. " items (" ..
    (function()
        local actions, headers, toggles, sliders = 0, 0, 0, 0
        for _, x in ipairs(ACTIONS) do
            if x.header then headers = headers + 1
            elseif x.toggle then toggles = toggles + 1
            elseif x.slider then sliders = sliders + 1
            else actions = actions + 1 end
        end
        return actions .. " actions, " .. toggles .. " toggles, " .. sliders .. " sliders, " .. headers .. " headers"
    end)() .. ")")
Log(TAG .. ": Open wrist menu -> Settings (gear) -> tap MODS tab")
Log(TAG .. ": Bridge: modmenu_status | modmenu_exec <N> | modmenu_rebuild | modmenu_rename")
Log(TAG .. ": Toggles: native WBP_OptionsEntry_C Type=1 (OFF/ON carousel)")
