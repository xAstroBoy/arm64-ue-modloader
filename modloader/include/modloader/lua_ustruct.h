// modloader/include/modloader/lua_ustruct.h
// UE4 UStruct Lua userdata wrapper — typed field access via reflection
// Enables struct.X, struct.Y, struct.Z syntax for FVector, FRotator, IntPoint, etc.
// Fields are resolved via reflection::walk_properties on the inner UStruct.

#pragma once

#include <sol/sol.hpp>
#include <cstdint>
#include <string>

namespace ue
{
    struct UStruct;
    struct FProperty;
}

namespace lua_ustruct
{

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

    struct LuaUStruct
    {
        uint8_t *data = nullptr;        // Pointer to the struct's raw memory
        ue::UStruct *ustruct = nullptr; // The UStruct* for reflection
        int32_t size = 0;               // Total struct size in bytes
        bool owns_data = false;         // If true, destructor frees data

        ~LuaUStruct();

        // No implicit copies — must be explicit
        LuaUStruct() = default;
        LuaUStruct(const LuaUStruct &other);
        LuaUStruct &operator=(const LuaUStruct &other);
        LuaUStruct(LuaUStruct &&other) noexcept;
        LuaUStruct &operator=(LuaUStruct &&other) noexcept;

        bool is_valid() const;
    };

    // ═══════════════════════════════════════════════════════════════════════
    // Registration — call once during Lua state setup
    // ═══════════════════════════════════════════════════════════════════════

    void register_all(sol::state &lua);

    // ═══════════════════════════════════════════════════════════════════════
    // Factory helpers
    // ═══════════════════════════════════════════════════════════════════════

    // Create a non-owning LuaUStruct wrapping existing memory
    LuaUStruct wrap(uint8_t *data, ue::UStruct *ustruct, int32_t size);

    // Create an owning LuaUStruct with a copy of the data
    LuaUStruct copy(const uint8_t *data, ue::UStruct *ustruct, int32_t size);

    // Create an owning LuaUStruct from a Lua table, filling fields via reflection
    LuaUStruct from_table(sol::state_view lua, const sol::table &tbl,
                          ue::UStruct *ustruct, int32_t size);

    // Fill existing struct memory from a Lua table using reflection
    void fill_from_table(uint8_t *data, ue::UStruct *ustruct,
                         const sol::table &tbl);

    // ═══════════════════════════════════════════════════════════════════════
    // TArray conversion buffer management (nesting-safe)
    // ═══════════════════════════════════════════════════════════════════════
    // When filling ProcessEvent params, TArray element type mismatches require
    // allocating conversion buffers (e.g., UObject* → SoftObj). These MUST be
    // cleaned up after ProcessEvent to prevent:
    //   - Memory leaks (calloc'd buffers never freed)
    //   - Engine corruption (UE tries to FMemory::Realloc/Free our buffer)
    //   - Use-after-free (stale pointers in object fields)
    //
    // NESTING: call_ufunction can recurse (PE → hook → tick → Lua → Call).
    // Each level saves/restores the thread-local buffer list via ConvBufScope.
    //
    // Usage:
    //   ConvBufScope scope;                   // saves & clears buffer list
    //   ... write_property_value() calls ...  // may register conversion buffers
    //   pe_fn(obj, func, params);             // ProcessEvent executes
    //   scope.cleanup();                      // free/zero this level's buffers
    //   // ~ConvBufScope restores outer level's list

    // Opaque forward — implementation in lua_ustruct.cpp
    struct ConvBufEntry;

    // RAII scope for conversion buffer tracking. Nesting-safe.
    struct ConvBufScope
    {
        ConvBufScope();  // saves current entries, clears thread-local
        ~ConvBufScope(); // restores saved entries (cleanup should be called first)
        void cleanup();  // free/zero buffers tracked in THIS scope

    private:
        void *saved_; // opaque: holds saved std::vector<ConvBufEntry>
    };

} // namespace lua_ustruct
