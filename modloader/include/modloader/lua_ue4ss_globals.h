// modloader/include/modloader/lua_ue4ss_globals.h
// UE4SS-compatible global functions — print, FindObjects, LoadAsset,
// Register*PreHook/PostHook family, thread ID, shared variables, version info

#pragma once

#include <sol/sol.hpp>

namespace lua_ue4ss_globals {

// Register all UE4SS-compatible globals into the Lua state
void register_all(sol::state& lua);

// Set the game thread ID (called from ProcessEvent hook on first call)
void set_game_thread_id();

// Check if current thread is the game thread (safe for Lua VM access)
bool is_game_thread();

} // namespace lua_ue4ss_globals
