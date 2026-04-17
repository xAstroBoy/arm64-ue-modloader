#pragma once
// modloader/include/modloader/config.h
// Runtime configuration loaded from config.json in the app data directory.
// Controls auto-dump behavior, object monitor, and other runtime settings.

#include <string>

namespace config
{

    // Load modloader_config.json from the given data directory.
    // If the file does not exist, creates it with defaults and loads those.
    // Auto-migrates from old config.json if found.
    // Must be called after paths::init() and logger::init().
    void load(const std::string &data_dir);

    // Write current config values to modloader_config.json in the given data directory.
    void save(const std::string &data_dir);

    // ── Feature toggles ─────────────────────────────────────────────────────

    // Whether Lua mods are loaded at all (default: true)
    bool lua_mods_enabled();

    // Whether custom .pak files are mounted (default: true)
    bool pak_loading_enabled();

    // Whether the ADB command bridge starts (default: true)
    bool adb_bridge_enabled();

    // Whether to write UEModLoader.log to disk (default: true)
    bool log_to_file();

    // Log level: "debug", "info", "warn", "error" (default: "info")
    const std::string &log_level();

    // ── Auto-dump settings ──────────────────────────────────────────────────

    // Whether to generate the SDK on boot (default: false)
    bool auto_dump_on_boot();

    // Whether to auto-redump SDK when object count grows (level load) (default: false)
    bool auto_dump_on_level_change();

    // Whether the background object monitor thread runs at all (default: false)
    bool object_monitor_enabled();

    // ── Object monitor tuning ───────────────────────────────────────────────

    // How often the monitor polls GUObjectArray, in milliseconds (default: 5000)
    int monitor_poll_interval_ms();

    // Minimum object growth to trigger a re-dump (default: 500)
    int monitor_growth_threshold();

    // Minimum time between auto re-dumps, in milliseconds (default: 30000)
    int monitor_cooldown_ms();

} // namespace config
