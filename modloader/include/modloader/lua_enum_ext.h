// modloader/include/modloader/lua_enum_ext.h
// Enum extension API for Lua:
//   - FindEnum(name)           → UEnum lightuserdata
//   - GetEnumTable(name)       → {ValueName = intValue, ...}
//   - GetEnumNames()           → {"DebugMenuType", "ECollisionChannel", ...}
//   - AppendEnumValue(e, n, v) → memory-edits UEnum TArray to add a value
//   - Enums.X                  → lazy-loaded enum constant tables

#pragma once

#include <sol/sol.hpp>

namespace lua_enum_ext {

// Register all enum extension functions into the Lua state.
// Call AFTER lua_ue4ss_types (which registers UEnumMethods).
void register_all(sol::state& lua);

} // namespace lua_enum_ext
