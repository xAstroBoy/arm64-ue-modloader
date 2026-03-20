// modloader/include/modloader/lua_enums.h
// UE4SS-compatible enum tables

#pragma once

#include <sol/sol.hpp>

namespace lua_enums {

void register_all(sol::state& lua);

void register_efindname(sol::state& lua);
void register_property_types(sol::state& lua);
void register_eobject_flags(sol::state& lua);
void register_einternal_object_flags(sol::state& lua);
void register_key(sol::state& lua);
void register_modifier_key(sol::state& lua);
void register_mod_keys(sol::state& lua);

} // namespace lua_enums
