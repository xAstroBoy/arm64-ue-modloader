#pragma once
// modloader/include/modloader/lua_uobject.h
// UObject Lua userdata wrapper — __index resolves properties, __newindex writes
// :Get() :Set() :Call() :IsValid() :GetName() etc.

#include "modloader/types.h"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace lua_uobject {

// The userdata struct stored in Lua — wraps a raw UObject pointer
struct LuaUObject {
    ue::UObject* ptr = nullptr;
};

// Register the UObject usertype in the Lua state
void register_types(sol::state& lua);

// Wrap a UObject* into a LuaUObject
LuaUObject wrap(ue::UObject* obj);

// Unwrap a LuaUObject to a raw UObject*
ue::UObject* unwrap(const LuaUObject& wrapped);

// Wrap or return sol::nil if null/CDO
sol::object wrap_or_nil(sol::state_view lua, ue::UObject* obj);

} // namespace lua_uobject
