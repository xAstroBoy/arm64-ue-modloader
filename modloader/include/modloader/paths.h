#pragma once
// modloader/include/modloader/paths.h
// Runtime path resolution — all paths derived from app data directory

#include <string>

namespace paths
{

    // Discover the app data directory via JNI getExternalFilesDir() — no hardcoded paths
    // Uses JavaVM* stored from JNI_OnLoad to call ActivityThread.currentApplication()
    void init();

    // Base data directory
    const std::string &data_dir();

    // Subdirectories
    std::string mods_dir();  // data_dir + "/mods/"
    std::string paks_dir();  // data_dir + "/paks/"
    std::string sdk_dir();   // data_dir + "/SDK/"
    std::string log_path();  // data_dir + "/UEModLoader.log"
    std::string crash_log(); // data_dir + "/modloader_crash.log"

    // SDK subdirectories (old per-class format)
    std::string sdk_classes_dir();   // data_dir + "/SDK/Classes/"
    std::string sdk_structs_dir();   // data_dir + "/SDK/Structs/"
    std::string sdk_enums_dir();     // data_dir + "/SDK/Enums/"
    std::string sdk_index_path();    // data_dir + "/SDK/_index.lua"
    std::string sdk_manifest_path(); // data_dir + "/SDK/_sdk_manifest.json"

    // UE4SS-format dump directories
    std::string cxx_header_dir(); // data_dir + "/CXXHeaderDump/"
    std::string lua_types_dir();  // data_dir + "/Lua/"

    // Usmap mappings file
    std::string usmap_path(); // data_dir + "/Mappings.usmap"

} // namespace paths
