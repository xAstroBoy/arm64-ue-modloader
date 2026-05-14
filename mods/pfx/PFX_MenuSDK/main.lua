-- ============================================================================
-- PFX_MenuSDK v1 — WristMenu / Settings control API
-- Exposes _G.PFX_MenuSDK for other mods to use.
--
-- SDK-verified properties/methods from CXXHeaderDump:
--   WBP_WristMenuSettings_C, WBP_Button_Default_C, WBP_Button_Base_C,
--   WBP_OptionsEntry_C, WBP_WristMenu_C, WBP_WristMenuButton_C
-- ============================================================================

local TAG = "PFX_MenuSDK"

-- ============================================================================
-- HELPERS
-- ============================================================================
local function is_live(obj)
    if not obj then return false end
    local ok, v = pcall(function() return obj:IsValid() end)
    return ok and v == true
end

-- ============================================================================
-- SDK CONSTANTS
-- Direct property names verified via CXXHeaderDump offsets
-- ============================================================================

-- WBP_WristMenuSettings_C nav button properties (WBP_Button_Default_C*)
local SETTINGS_BUTTONS = {
    vr      = "WBP_Button_VR",
    tables  = "WBP_Button_Tables",
    mr      = "WBP_Button_MR",
    pp      = "WBP_Button_PP",
    legal   = "WBP_Button_Legal",
}

-- WBP_WristMenuSettings_C content panel properties (VerticalBox*)
local SETTINGS_PANELS = {
    ingame      = "InGameOptionEntries",
    movement    = "MovementOptionEntries",
    audio       = "AudioOptionEntries",
    misc        = "MiscOptionEntries",
    access      = "AcsessibilityOptionEntries",  -- note: game typo
    legal       = "LegalLineContainer",
    credits_hdr = "CreditsStudiosHeader",
    buttons     = "ButtonContainer",
}

-- WBP_WristMenuSettings_C state properties
local SETTINGS_PROPS = {
    switcher        = "SubmenuSwitcher",          -- WidgetSwitcher*
    sub_buttons     = "SubmenuButtons",           -- TArray<WBP_Button_Default_C>
    option_entries  = "OptionEntries",            -- TArray<WBP_OptionsEntry_C>
    selected_btn    = "SelectedMenuButton",       -- WBP_Button_Base_C*
    scrollbox       = "SubmenuScrollBoxExtended", -- ScrollBoxExtended*
    arrow_up        = "ArrowUp",                  -- Button*
    arrow_down      = "ArrowDown",                -- Button*
    close_btn       = "WB_CloseButton",           -- WB_CloseButton_C*
    pin_btn         = "WB_PinButton",             -- WB_PinButton_C*
    refresh_needed  = "IsSettingsRefreshNeeded",  -- bool
    active_table_id = "ActiveTableId",            -- int32
}

-- WBP_WristMenuSettings_C UFunctions (SDK-verified callable via :Call())
local SETTINGS_FUNCS = {
    reset_scroll        = "ResetScroll",
    save_settings       = "SaveSettings",
    refresh_all         = "RefreshAllSettings",
    apply_constraints   = "ApplyOptionConstraints",
    select_start_cat    = "SelectStartingCategory",   -- bool ShowCredits
    on_submenu_clicked  = "OnSubmenuButtonClicked",   -- Widget* Widget, bool HideDescription
    subscribe_change    = "SubscribeToSettingsChange",
    unsubscribe_change  = "UnSubscribeFromSettingsChange",
    refresh_on_activate = "RefreshSettingsOnActivation",
}

-- WBP_Button_Base_C / WBP_Button_Default_C UFunctions
local BUTTON_FUNCS = {
    set_title       = "SetTitleText",               -- FText
    set_enabled     = "Set Button Enabled",         -- bool
    set_selected    = "SetSelected",                -- bool
    set_icon        = "SetIconImage",               -- Texture2D*
    set_notify_mark = "SetHasNotificationMark",     -- bool
    set_h_align     = "SetHorizontalAllignment",    -- EHorizontalAlignment
    update_state    = "UpdateButtonState",
}

-- WBP_OptionsEntry_C UFunctions
local OPTIONS_FUNCS = {
    set_enabled     = "SetWidgetEnabled",           -- bool
    update_sel      = "UpdateSelection",
    refresh_val     = "RefreshValue",
    load_defaults   = "LoadDefaultValues",
    setup_entry     = "SetupEntry",
    pre_init_btns   = "PreInitButtons",
}

-- ============================================================================
-- PUBLIC API TABLE
-- ============================================================================
local M = {}
_G.PFX_MenuSDK = M

-- Expose constants for external use
M.BUTTONS   = SETTINGS_BUTTONS
M.PANELS    = SETTINGS_PANELS
M.PROPS     = SETTINGS_PROPS
M.FUNCS     = SETTINGS_FUNCS
M.BTN_FUNCS = BUTTON_FUNCS
M.OPT_FUNCS = OPTIONS_FUNCS

-- ============================================================================
-- CORE UTILITIES
-- ============================================================================

M.is_live = is_live

-- Safe :Get() on a UObject
function M.get(obj, prop)
    if not is_live(obj) then return nil end
    local v = nil
    pcall(function() v = obj:Get(prop) end)
    return is_live(v) and v or nil
end

-- Safe :Call() on a UObject, returns result
function M.call(obj, func, ...)
    if not is_live(obj) then return nil end
    local args = {...}
    local ok, r = pcall(function() return obj:Call(func, table.unpack(args)) end)
    return ok and r or nil
end

-- ============================================================================
-- SETTINGS INSTANCE ACCESS
-- ============================================================================

-- Returns all live WBP_WristMenuSettings_C instances
function M.get_settings_instances()
    local all = nil
    pcall(function() all = FindAllOf("WBP_WristMenuSettings_C") end)
    return all or {}
end

-- Returns the WBP_WristMenu_C wrist menu actor
function M.get_wrist_menu()
    local w = nil
    pcall(function() w = FindFirstOf("WBP_WristMenu_C") end)
    return is_live(w) and w or nil
end

-- Returns a live player controller
function M.get_player_controller()
    local pc = nil
    pcall(function() pc = FindFirstOf("BP_PlayerController_C") end)
    if not is_live(pc) then
        pcall(function() pc = FindFirstOf("PlayerController") end)
    end
    return is_live(pc) and pc or nil
end

-- ============================================================================
-- SETTINGS NAVIGATION
-- ============================================================================

-- Get a named nav button from a settings widget
-- name: one of SETTINGS_BUTTONS keys ("legal", "vr", "tables", "mr", "pp")
function M.get_nav_button(settings, name)
    local prop = SETTINGS_BUTTONS[name] or name
    return M.get(settings, prop)
end

-- Get the Legal/MODS nav button
function M.get_legal_button(settings)
    return M.get(settings, SETTINGS_BUTTONS.legal)
end

-- Get the currently selected nav button
function M.get_selected_button(settings)
    return M.get(settings, SETTINGS_PROPS.selected_btn)
end

-- Check if the Legal/MODS page is currently active
function M.is_legal_page_active(settings)
    local lb = M.get_legal_button(settings)
    local sel = M.get_selected_button(settings)
    if not is_live(lb) or not is_live(sel) then return false end
    local ln, sn = "", ""
    pcall(function() ln = lb:GetName() end)
    pcall(function() sn = sel:GetName() end)
    return ln ~= "" and ln == sn
end

-- ============================================================================
-- SETTINGS CONTENT PANELS
-- ============================================================================

-- Get a content panel VerticalBox by name key ("legal", "audio", etc.)
function M.get_panel(settings, name)
    local prop = SETTINGS_PANELS[name] or name
    return M.get(settings, prop)
end

-- Get the LegalLineContainer (mod menu injection point)
function M.get_legal_panel(settings)
    return M.get(settings, SETTINGS_PANELS.legal)
end

-- Clear all children from a panel container
function M.clear_panel(container)
    if not is_live(container) then return false end
    pcall(function() container:Call("ClearChildren") end)
    return true
end

-- Add a widget to a container
function M.add_to_panel(container, widget)
    if not is_live(container) or not is_live(widget) then return false end
    pcall(function() container:Call("AddChild", widget) end)
    return true
end

-- ============================================================================
-- SETTINGS SWITCHER
-- ============================================================================

-- Get the SubmenuSwitcher widget
function M.get_switcher(settings)
    return M.get(settings, SETTINGS_PROPS.switcher)
end

-- Get all SubmenuButtons TArray
function M.get_submenu_buttons(settings)
    local arr = nil
    pcall(function() arr = settings:Get(SETTINGS_PROPS.sub_buttons) end)
    return arr
end

-- Force-switch the SubmenuSwitcher to show the Legal/MODS panel
-- Finds WBP_Button_Legal index in SubmenuButtons array, then calls SetActiveWidgetIndex
function M.switch_to_legal_panel(settings)
    if not is_live(settings) then return false end
    local lb = M.get_legal_button(settings)
    if not is_live(lb) then return false end
    local arr = M.get_submenu_buttons(settings)
    if not arr then return false end

    local idx = nil
    pcall(function()
        for i = 1, #arr do
            local b = arr[i]
            if is_live(b) then
                local bn, lbn = "", ""
                pcall(function() bn = b:GetName() end)
                pcall(function() lbn = lb:GetName() end)
                if bn == lbn then idx = i - 1; break end  -- 0-based for switcher
            end
        end
    end)

    if idx == nil then
        -- Fallback: Legal is always the last button
        local n = 0
        pcall(function() n = #arr end)
        if n > 0 then idx = n - 1 end
    end

    if idx == nil then return false end
    local ss = M.get_switcher(settings)
    if not is_live(ss) then return false end
    pcall(function() ss:Call("SetActiveWidgetIndex", idx) end)
    return true
end

-- ============================================================================
-- SETTINGS ACTIONS
-- ============================================================================

-- Scroll back to top
function M.reset_scroll(settings)
    M.call(settings, SETTINGS_FUNCS.reset_scroll)
end

-- Save current settings to persistent storage
function M.save_settings(settings)
    M.call(settings, SETTINGS_FUNCS.save_settings)
end

-- Refresh all settings values from storage
function M.refresh_all(settings)
    M.call(settings, SETTINGS_FUNCS.refresh_all)
end

-- Apply visibility/constraint rules to option entries
function M.apply_constraints(settings)
    M.call(settings, SETTINGS_FUNCS.apply_constraints)
end

-- ============================================================================
-- BUTTON HELPERS
-- ============================================================================

-- Set the text label on a WBP_Button_Default_C / WBP_Button_Base_C
function M.set_button_text(btn, text)
    if not is_live(btn) then return false end
    pcall(function() btn:Call(BUTTON_FUNCS.set_title, text) end)
    pcall(function() btn:Set("Button Title Text", text) end)
    pcall(function() btn:Set("ButtonTitleText", text) end)
    return true
end

-- Show or hide the red notification dot on a nav button
function M.set_notification_mark(btn, visible)
    if not is_live(btn) then return false end
    pcall(function() btn:Call(BUTTON_FUNCS.set_notify_mark, visible) end)
    return true
end

-- Enable/disable a button
function M.set_button_enabled(btn, enabled)
    if not is_live(btn) then return false end
    pcall(function() btn:Call(BUTTON_FUNCS.set_enabled, enabled) end)
    pcall(function() btn:Set("Is Enabled", enabled) end)
    return true
end

-- Clear the icon texture from a button
function M.clear_button_icon(btn)
    if not is_live(btn) then return false end
    pcall(function() btn:Set("Icon Texture", nil) end)
    pcall(function() btn:Set("IconTexture", nil) end)
    return true
end

-- ============================================================================
-- WIDGET CREATION
-- ============================================================================

-- Create a WBP_Button_Default_C with a text label
-- Returns the button widget or nil on failure
function M.create_button(label, pc)
    if not pc then pc = M.get_player_controller() end
    if not is_live(pc) then return nil end
    local btn = nil
    pcall(function() btn = CreateWidget("WBP_Button_Default_C", pc) end)
    if not is_live(btn) then return nil end
    M.set_button_text(btn, label)
    M.clear_button_icon(btn)
    return btn
end

-- Create a WBP_OptionsEntry_C with title and option labels
-- type_id: 0=buttons, 1=slider, 2=switcher
-- Returns the entry widget or nil on failure
-- NOTE: Experimental — requires SetupEntry() and may need AllOptionEntries registration
function M.create_options_entry(title, options, type_id, pc)
    if not pc then pc = M.get_player_controller() end
    if not is_live(pc) then return nil end
    local e = nil
    pcall(function() e = CreateWidget("WBP_OptionsEntry_C", pc) end)
    if not is_live(e) then return nil end
    pcall(function() e:Set("Title", title) end)
    if type_id then
        pcall(function() e:Set("Type", type_id) end)
    else
        pcall(function() e:Set("Type", 0) end)  -- default: button type
    end
    if options and #options > 0 then
        pcall(function() e:Set("Options", options) end)
    end
    pcall(function() e:Call(OPTIONS_FUNCS.setup_entry) end)
    return e
end

-- ============================================================================
-- COMPLETE SETTINGS PANEL BUILD HELPER
-- Clears LegalLineContainer and populates it with a list of widgets
-- widgets: array of {widget=..., tooltip=...}
-- ============================================================================
function M.build_custom_panel(settings, widgets, title)
    if not is_live(settings) then return false end
    local llc = M.get_legal_panel(settings)
    if not is_live(llc) then return false end

    M.clear_panel(llc)

    local pc = M.get_player_controller()
    if not is_live(pc) then return false end

    -- Header button (if title given)
    if title then
        local hdr = M.create_button(title, pc)
        if is_live(hdr) then
            pcall(function() hdr:Set("ToolTipText", "[M:header]") end)
            M.add_to_panel(llc, hdr)
        end
    end

    -- Add user-provided widgets
    for _, entry in ipairs(widgets or {}) do
        if is_live(entry.widget) then
            if entry.tooltip then
                pcall(function() entry.widget:Set("ToolTipText", entry.tooltip) end)
            end
            M.add_to_panel(llc, entry.widget)
        end
    end

    -- Scroll back to top after building
    pcall(function() M.reset_scroll(settings) end)

    local count = 0
    pcall(function() count = llc:Call("GetChildrenCount") end)
    return count > 0
end

-- ============================================================================
-- READY
-- ============================================================================
Log(TAG .. ": v1 loaded — _G.PFX_MenuSDK exposed")
Log(TAG .. ": Keys: get_settings_instances, get_legal_button, get_legal_panel,")
Log(TAG .. ":       create_button, create_options_entry, build_custom_panel,")
Log(TAG .. ":       set_button_text, set_notification_mark, reset_scroll, etc.")
