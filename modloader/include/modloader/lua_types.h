// modloader/include/modloader/lua_types.h
// UE4SS-compatible Lua type wrappers
// FName, FText, FString, RemoteUnrealParam, LocalUnrealParam,
// FWeakObjectPtr, FOutputDevice, ThreadId

#pragma once

#include <sol/sol.hpp>
#include <string>
#include <cstdint>
#include <thread>

namespace ue { struct UObject; }

namespace lua_types {

// ═══════════════════════════════════════════════════════════════════════
// FName — immutable name in UE FName pool
// ═══════════════════════════════════════════════════════════════════════

struct LuaFName {
    int32_t comparison_index = 0;
    int32_t number = 0;
    mutable std::string cached_string;

    LuaFName();
    LuaFName(const std::string& name);
    LuaFName(const std::string& name, int find_type); // 0=FIND, 1=ADD
    LuaFName(int32_t index);
    LuaFName(int32_t index, int find_type);

    std::string to_string() const;
    int32_t get_comparison_index() const;
    bool is_valid() const;
};

// ═══════════════════════════════════════════════════════════════════════
// FText — localized text wrapper
// ═══════════════════════════════════════════════════════════════════════

struct LuaFText {
    std::string text;

    LuaFText();
    LuaFText(const std::string& str);
    std::string to_string() const;
};

// ═══════════════════════════════════════════════════════════════════════
// FString — mutable UE string wrapper
// ═══════════════════════════════════════════════════════════════════════

struct LuaFString {
    std::string data;

    LuaFString();
    LuaFString(const std::string& str);

    std::string to_string() const;
    void empty();
    int len() const;
    bool is_empty() const;
    void append(const std::string& str);
    int find(const std::string& search) const;
    bool starts_with(const std::string& prefix) const;
    bool ends_with(const std::string& suffix) const;
    LuaFString to_upper() const;
    LuaFString to_lower() const;
};

// ═══════════════════════════════════════════════════════════════════════
// RemoteUnrealParam — wraps hook callback parameter pointers
// ═══════════════════════════════════════════════════════════════════════

struct LuaRemoteUnrealParam {
    enum class ParamType {
        Unknown = 0,
        UObjectPtr,
        Bool,
        Int32,
        Int64,
        Float,
        Double,
        FNameVal,
        FStringVal,
        FTextVal,
        Byte,
        Enum8,
        Enum16,
        Enum32,
        StructPtr,
        RawPtr
    };

    void* ptr = nullptr;
    ParamType param_type = ParamType::Unknown;
    std::string property_type_name;

    sol::object get(sol::this_state ts) const;
    void set(sol::object value);
    std::string type_name() const;
};

// Alias
using LuaLocalUnrealParam = LuaRemoteUnrealParam;

// ═══════════════════════════════════════════════════════════════════════
// FWeakObjectPtr
// ═══════════════════════════════════════════════════════════════════════

struct LuaFWeakObjectPtr {
    ue::UObject* ptr = nullptr;

    sol::object get(sol::this_state ts) const;
    bool is_valid() const;
};

// ═══════════════════════════════════════════════════════════════════════
// FOutputDevice — for ProcessConsoleExec callbacks
// ═══════════════════════════════════════════════════════════════════════

struct LuaFOutputDevice {
    void* device = nullptr;

    void log(const std::string& msg);
};

// ═══════════════════════════════════════════════════════════════════════
// ThreadId
// ═══════════════════════════════════════════════════════════════════════

struct LuaThreadId {
    std::thread::id id;
};

// ═══════════════════════════════════════════════════════════════════════
// Registration
// ═══════════════════════════════════════════════════════════════════════

void register_all(sol::state& lua);

void register_fname(sol::state& lua);
void register_ftext(sol::state& lua);
void register_fstring(sol::state& lua);
void register_remote_unreal_param(sol::state& lua);
void register_fweakobjectptr(sol::state& lua);
void register_foutputdevice(sol::state& lua);
void register_thread_id(sol::state& lua);

} // namespace lua_types
