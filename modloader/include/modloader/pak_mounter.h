#pragma once
// modloader/include/modloader/pak_mounter.h
// Mount external .pak files via FPakPlatformFile::Mount
// Uses Dobby hooks on the engine's own Mount/MountAllPakFiles to capture
// the FPakPlatformFile instance at the exact moment the engine uses it —
// mirroring the proven Frida modloader approach.

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

namespace pak_mounter {

struct PakInfo {
    std::string name;
    std::string path;
    bool        mounted = false;
    int64_t     file_size = 0;
    std::string error;
};

// Install Dobby hooks on FPakPlatformFile::Mount and MountAllPakFiles.
// When the engine calls these functions, we capture the FPakPlatformFile
// instance from the this pointer and mount custom PAKs synchronously.
// Must be called after symbols::resolve_core_symbols().
void install_hooks();

// Install PAK hooks as EARLY as possible — before symbol resolution.
// Called from the early PAK hook thread in main.cpp, immediately after
// libUE4.so is detected. Uses raw base address and dlsym only.
// The 5-second boot delay means normal install_hooks() is too late —
// the engine already mounted its PAKs and loaded the startup level.
// This function installs hooks BEFORE the engine's init runs.
void install_early_hooks(uintptr_t base, void* lib_handle);

// Mount a .pak file by name (from paks/ directory) or full path.
// Uses captured FPakPlatformFile instance (from hook) or FPlatformFileManager fallback.
// Mounts at priority 1000 to override game PAKs.
bool mount(const std::string& pak_name);

// Mount all .pak files in paks/ directory — returns count mounted
int mount_all();

// Get all PAK info
const std::vector<PakInfo>& get_all();

// Check if a specific PAK is mounted
bool is_mounted(const std::string& pak_name);

// Reset cached FPakPlatformFile pointer so next mount() re-resolves it.
void reset_cache();

} // namespace pak_mounter
