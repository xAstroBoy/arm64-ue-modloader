// modloader/include/modloader/lua_ustruct.h
// UE4 UStruct Lua userdata wrapper — typed field access via reflection
// Enables struct.X, struct.Y, struct.Z syntax for FVector, FRotator, IntPoint, etc.
// Fields are resolved via reflection::walk_properties on the inner UStruct.

#pragma once

#include <sol/sol.hpp>
#include <cstdint>
#include <string>

namespace ue {
    struct UStruct;
    struct FProperty;
}

namespace lua_ustruct {

// ═══════════════════════════════════════════════════════════════════════
// LuaUStruct — wraps a raw pointer to inline struct memory + its UStruct*
// for reflection-based field access.
//
// Two modes:
//   Non-owning (owns_data=false): data points into a live UObject's memory.
//     Writes go directly to the UObject. Valid as long as the UObject lives.
//   Owning (owns_data=true): data was allocated by us (e.g. for ProcessEvent
//     param buffers or table-constructed structs). Freed on destruction.
// ═══════════════════════════════════════════════════════════════════════

struct LuaUStruct {
    uint8_t*     data      = nullptr;   // Pointer to the struct's raw memory
    ue::UStruct* ustruct   = nullptr;   // The UStruct* for reflection
    int32_t      size      = 0;         // Total struct size in bytes
    bool         owns_data = false;     // If true, destructor frees data

    ~LuaUStruct();

    // No implicit copies — must be explicit
    LuaUStruct() = default;
    LuaUStruct(const LuaUStruct& other);
    LuaUStruct& operator=(const LuaUStruct& other);
    LuaUStruct(LuaUStruct&& other) noexcept;
    LuaUStruct& operator=(LuaUStruct&& other) noexcept;

    bool is_valid() const;
};

// ═══════════════════════════════════════════════════════════════════════
// Registration — call once during Lua state setup
// ═══════════════════════════════════════════════════════════════════════

void register_all(sol::state& lua);

// ═══════════════════════════════════════════════════════════════════════
// Factory helpers
// ═══════════════════════════════════════════════════════════════════════

// Create a non-owning LuaUStruct wrapping existing memory
LuaUStruct wrap(uint8_t* data, ue::UStruct* ustruct, int32_t size);

// Create an owning LuaUStruct with a copy of the data
LuaUStruct copy(const uint8_t* data, ue::UStruct* ustruct, int32_t size);

// Create an owning LuaUStruct from a Lua table, filling fields via reflection
LuaUStruct from_table(sol::state_view lua, const sol::table& tbl,
                       ue::UStruct* ustruct, int32_t size);

// Fill existing struct memory from a Lua table using reflection
void fill_from_table(uint8_t* data, ue::UStruct* ustruct,
                     const sol::table& tbl);

} // namespace lua_ustruct
