#pragma once
// modloader/include/modloader/init.h
// Boot sequence orchestration — called from JNI_OnLoad on background thread

namespace init {

// Run the full boot sequence — returns true on success
bool boot();

// Check if boot is complete
bool is_initialized();

} // namespace init
