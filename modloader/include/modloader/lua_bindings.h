#pragma once
// modloader/include/modloader/lua_bindings.h
// Registers all global Lua functions: FindClass, RegisterHook, CallNative, etc.

namespace sol { class state; }

namespace lua_bindings {

// Register all global functions and types into the Lua state
void register_all(sol::state& lua);

} // namespace lua_bindings
