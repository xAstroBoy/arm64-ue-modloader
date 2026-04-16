#pragma once
// modloader/include/modloader/mod_loader.h
// Scans mods/ directory, loads each mod's main.lua, tracks status

#include <string>
#include <vector>

namespace mod_loader
{

    enum class ModStatus
    {
        LOADED,
        ERRORED,
        FAILED
    };

    struct ModInfo
    {
        std::string name;
        std::string path;
        ModStatus status = ModStatus::FAILED;
        std::string error;
        int error_count = 0;
    };

    // Scan mods/ directory and load all mods — returns number loaded
    int load_all();

    // Load a single mod by name
    bool load_mod(const std::string &name);

    // Hot-reload a mod (re-execute main.lua)
    bool reload_mod(const std::string &name);

    // Get all mod info
    const std::vector<ModInfo> &get_all_mods();

    // Find a specific mod by name
    const ModInfo *find_mod(const std::string &name);

    // Counts
    int loaded_count();
    int failed_count();
    int total_count();

    // True while the loader is in a mod-loading phase (initial batch and/or
    // an individual mod's main.lua is actively executing on any thread).
    bool is_any_mod_loading();

} // namespace mod_loader
