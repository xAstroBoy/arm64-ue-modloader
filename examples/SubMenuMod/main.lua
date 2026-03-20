-- examples/SubMenuMod/main.lua
-- ═══════════════════════════════════════════════════════════════════════
-- Sub-Menu Mod — Dynamic debug menu pages
--
-- This demonstrates:
--   - RegisterSubMenu for creating sub-pages
--   - NavigateTo with dynamic populate callbacks
--   - AddItem for building page content dynamically
--   - RegisterSelector for cycling options
--   - Nested sub-page navigation
--
-- Requires: DebugMenuAPI mod
-- ═══════════════════════════════════════════════════════════════════════

local TAG = "SubMenuMod"

-- Some state that the sub-menu will display/modify
local settings = {
    difficulty = "Normal",
    speed      = 1.0,
    options    = { "Easy", "Normal", "Hard", "Nightmare" },
    speeds     = { 0.5, 1.0, 1.5, 2.0, 3.0 },
    speed_idx  = 2,
    log_count  = 0,
}

if not (SharedAPI and SharedAPI.DebugMenu) then
    Log(TAG .. ": DebugMenuAPI not available, skipping")
    return
end

local api = SharedAPI.DebugMenu

-- ═══════════════════════════════════════════════════════════════════════
-- Simple API: Items on the root Mods page
-- ═══════════════════════════════════════════════════════════════════════

-- Selector: cycles through options on each confirm
api.RegisterSelector(TAG, "Difficulty", settings.options, function(value, index)
    settings.difficulty = value
    Log(TAG .. ": Difficulty set to " .. value)
end)

-- Action: one-shot button
api.RegisterAction(TAG, "Log Status", function()
    settings.log_count = settings.log_count + 1
    Log(TAG .. ": Status logged " .. settings.log_count .. " times")
    Log(TAG .. ":   Difficulty = " .. settings.difficulty)
    Log(TAG .. ":   Speed = " .. tostring(settings.speed))
end)

-- ═══════════════════════════════════════════════════════════════════════
-- Advanced API: Sub-menu with dynamic content
-- ═══════════════════════════════════════════════════════════════════════

api.RegisterSubMenu(TAG, "Advanced Settings", function()
    -- NavigateTo creates a new page and calls populate() to fill it.
    -- populate() is re-called on every Refresh() — content is dynamic.
    api.NavigateTo({
        name = "Advanced Settings",
        populate = function()
            -- Add items to the current page
            api.AddItem("Speed: " .. tostring(settings.speed), function()
                -- Cycle through speed options
                settings.speed_idx = (settings.speed_idx % #settings.speeds) + 1
                settings.speed = settings.speeds[settings.speed_idx]
                Log(TAG .. ": Speed = " .. tostring(settings.speed))
                -- Refresh to show updated value
                api.Refresh()
            end)

            api.AddItem("Reset All to Default", function()
                settings.difficulty = "Normal"
                settings.speed = 1.0
                settings.speed_idx = 2
                settings.log_count = 0
                Log(TAG .. ": All settings reset!")
                api.Refresh()
            end)

            -- Separator / label (no callback = non-interactive)
            api.AddItem("--- Info ---", nil)

            api.AddItem("Current: " .. settings.difficulty .. " @ " .. settings.speed .. "x", nil)
        end
    })
end)

-- ═══════════════════════════════════════════════════════════════════════
-- Static Sub-Page (alternative to dynamic)
-- ═══════════════════════════════════════════════════════════════════════
-- AddPage creates a page linked from root. Items are added once (static).

local info_page = api.AddPage("info", "Mod Info")

api.AddItemToPage(info_page, TAG, "Version: 1.0", "action", {
    callback = function() Log(TAG .. ": v1.0") end
})

api.AddItemToPage(info_page, TAG, "Author: YourName", "action", {
    callback = function() Log(TAG .. ": by YourName") end
})

api.AddItemToPage(info_page, TAG, "Verbose Logging", "toggle", {
    default = false,
    callback = function(on)
        Log(TAG .. ": Verbose = " .. tostring(on))
    end
})

Log(TAG .. ": Loaded — check Debug Menu → Mods")
