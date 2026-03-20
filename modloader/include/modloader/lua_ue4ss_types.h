// modloader/include/modloader/lua_ue4ss_types.h
// UE4SS-compatible UClass, UStruct, UFunction, UEnum, AActor Lua helpers
// Extends the base UObject with additional methods and type tables

#pragma once

#include <sol/sol.hpp>

namespace lua_ue4ss_types {

// Register all extended types into the Lua state
void register_all(sol::state& lua);

// Individual registration functions
void extend_uobject(sol::state& lua);        // UObject additional methods
void register_uclass_type(sol::state& lua);   // UClassMethods table
void register_ufunction_type(sol::state& lua);// UFunctionMethods table
void register_uenum_type(sol::state& lua);    // UEnumMethods table

} // namespace lua_ue4ss_types
