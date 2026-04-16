#pragma once
// modloader/include/modloader/lua_dump_generator.h
// SDK Generator — produces UE4SS-identical output:
//   CXXHeaderDump/  — per-package .hpp + _enums.hpp  (C++ headers with offsets)
//   Lua/            — per-package .lua + _enums.lua  (LuaLS annotations)
//   SDK/            — per-class/struct/enum .lua files (legacy format)

#include <string>

namespace sdk_gen
{

    // ═══ Full generation (all 3 output types) ═══════════════════════════════

    // Generate everything from current reflection cache
    int generate();

    // Re-walk GUObjectArray then regenerate
    int regenerate();

    // ═══ Individual generators ══════════════════════════════════════════════

    // CXX header dump — produces CXXHeaderDump/{Package}.hpp + {Package}_enums.hpp
    int generate_cxx_headers();

    // Lua type annotations — produces Lua/{Package}.lua + {Package}_enums.lua
    int generate_lua_types();

    // Legacy SDK — produces SDK/Classes/*.lua, SDK/Structs/*.lua, SDK/Enums/*.lua, etc.
    int generate_legacy_sdk();

    // Usmap mappings — produces Mappings.usmap (FModel/UE4SS compatible binary format)
    int generate_usmap();

    // ═══ Counts from last generation ════════════════════════════════════════
    int class_count();
    int struct_count();
    int enum_count();

} // namespace sdk_gen
