// modloader/src/lua/lua_enums.cpp
// UE4SS-compatible enum tables — 1:1 matching names and values
// Key, ModifierKey, EFindName, PropertyTypes, EObjectFlags, EInternalObjectFlags

#include "modloader/lua_enums.h"
#include "modloader/logger.h"

namespace lua_enums {

void register_all(sol::state& lua) {
    register_efindname(lua);
    register_property_types(lua);
    register_eobject_flags(lua);
    register_einternal_object_flags(lua);
    register_key(lua);
    register_modifier_key(lua);
    register_mod_keys(lua);

    logger::log_info("LUA", "UE4SS-compatible enum tables registered");
}

// ═══════════════════════════════════════════════════════════════════════
// EFindName — used by FName constructors
// ═══════════════════════════════════════════════════════════════════════

void register_efindname(sol::state& lua) {
    sol::table t = lua.create_named_table("EFindName");
    t["FNAME_Find"] = 0;
    t["FNAME_Add"]  = 1;
}

// ═══════════════════════════════════════════════════════════════════════
// PropertyTypes — used to identify property types in reflection
// ═══════════════════════════════════════════════════════════════════════

void register_property_types(sol::state& lua) {
    sol::table t = lua.create_named_table("PropertyTypes");

    t["ObjectProperty"]      = 0;
    t["ObjectPtrProperty"]   = 1;
    t["Int8Property"]        = 2;
    t["Int16Property"]       = 3;
    t["IntProperty"]         = 4;
    t["Int64Property"]       = 5;
    t["ByteProperty"]        = 6;
    t["UInt16Property"]      = 7;
    t["UInt32Property"]      = 8;
    t["UInt64Property"]      = 9;
    t["NameProperty"]        = 10;
    t["FloatProperty"]       = 11;
    t["DoubleProperty"]      = 12;
    t["BoolProperty"]        = 13;
    t["StrProperty"]         = 14;
    t["TextProperty"]        = 15;
    t["ClassProperty"]       = 16;
    t["SoftClassProperty"]   = 17;
    t["SoftObjectProperty"]  = 18;
    t["WeakObjectProperty"]  = 19;
    t["LazyObjectProperty"]  = 20;
    t["StructProperty"]      = 21;
    t["EnumProperty"]        = 22;
    t["ArrayProperty"]       = 23;
    t["MapProperty"]         = 24;
    t["SetProperty"]         = 25;
    t["InterfaceProperty"]   = 26;
    t["DelegateProperty"]    = 27;
    t["MulticastDelegateProperty"]         = 28;
    t["MulticastInlineDelegateProperty"]   = 29;
    t["MulticastSparseDelegateProperty"]   = 30;
    t["FieldPathProperty"]   = 31;
}

// ═══════════════════════════════════════════════════════════════════════
// EObjectFlags — EObjectFlags enum values
// ═══════════════════════════════════════════════════════════════════════

void register_eobject_flags(sol::state& lua) {
    sol::table t = lua.create_named_table("EObjectFlags");

    t["RF_NoFlags"]                  = 0x00000000;
    t["RF_Public"]                   = 0x00000001;
    t["RF_Standalone"]               = 0x00000002;
    t["RF_MarkAsNative"]             = 0x00000004;
    t["RF_Transactional"]            = 0x00000008;
    t["RF_ClassDefaultObject"]       = 0x00000010;
    t["RF_ArchetypeObject"]          = 0x00000020;
    t["RF_Transient"]                = 0x00000040;
    t["RF_MarkAsRootSet"]            = 0x00000080;
    t["RF_TagGarbageTemp"]           = 0x00000100;
    t["RF_NeedInitialization"]       = 0x00000200;
    t["RF_NeedLoad"]                 = 0x00000400;
    t["RF_KeepForCooker"]            = 0x00000800;
    t["RF_NeedPostLoad"]             = 0x00001000;
    t["RF_NeedPostLoadSubobjects"]   = 0x00002000;
    t["RF_NewerVersionExists"]       = 0x00004000;
    t["RF_BeginDestroyed"]           = 0x00008000;
    t["RF_FinishDestroyed"]          = 0x00010000;
    t["RF_BeingRegenerated"]         = 0x00020000;
    t["RF_DefaultSubObject"]         = 0x00040000;
    t["RF_WasLoaded"]                = 0x00080000;
    t["RF_TextExportTransient"]      = 0x00100000;
    t["RF_LoadCompleted"]            = 0x00200000;
    t["RF_InheritableComponentTemplate"] = 0x00400000;
    t["RF_DuplicateTransient"]       = 0x00800000;
    t["RF_StrongRefOnFrame"]         = 0x01000000;
    t["RF_NonPIEDuplicateTransient"] = 0x02000000;
    t["RF_Dynamic"]                  = 0x04000000;
    t["RF_WillBeLoaded"]             = 0x08000000;
}

// ═══════════════════════════════════════════════════════════════════════
// EInternalObjectFlags
// ═══════════════════════════════════════════════════════════════════════

void register_einternal_object_flags(sol::state& lua) {
    sol::table t = lua.create_named_table("EInternalObjectFlags");

    t["None"]                    = 0;
    t["ReachableInCluster"]      = (1 << 23);
    t["ClusterRoot"]             = (1 << 24);
    t["Native"]                  = (1 << 25);
    t["Async"]                   = (1 << 26);
    t["AsyncLoading"]            = (1 << 27);
    t["Unreachable"]             = (1 << 28);
    t["PendingKill"]             = (1 << 29);
    t["RootSet"]                 = (1 << 30);
    t["GarbageCollectionKeepFlags"] = static_cast<int>((1 << 25) | (1 << 26));
    t["AllFlags"]                = static_cast<int>(0x7F800000);
}

// ═══════════════════════════════════════════════════════════════════════
// Key — UE4SS keyboard key codes (Windows VK codes)
// ═══════════════════════════════════════════════════════════════════════

void register_key(sol::state& lua) {
    sol::table t = lua.create_named_table("Key");

    // NOTE: On Quest/Android these don't map to physical keys, but UE4SS mods
    // reference them so we must provide the table for compatibility.
    // Controller button mapping is done separately.

    // Letters
    t["A"] = 0x41; t["B"] = 0x42; t["C"] = 0x43; t["D"] = 0x44;
    t["E"] = 0x45; t["F"] = 0x46; t["G"] = 0x47; t["H"] = 0x48;
    t["I"] = 0x49; t["J"] = 0x4A; t["K"] = 0x4B; t["L"] = 0x4C;
    t["M"] = 0x4D; t["N"] = 0x4E; t["O"] = 0x4F; t["P"] = 0x50;
    t["Q"] = 0x51; t["R"] = 0x52; t["S"] = 0x53; t["T"] = 0x54;
    t["U"] = 0x55; t["V"] = 0x56; t["W"] = 0x57; t["X"] = 0x58;
    t["Y"] = 0x59; t["Z"] = 0x5A;

    // Numbers
    t["ZERO"] = 0x30; t["ONE"] = 0x31; t["TWO"] = 0x32; t["THREE"] = 0x33;
    t["FOUR"] = 0x34; t["FIVE"] = 0x35; t["SIX"] = 0x36; t["SEVEN"] = 0x37;
    t["EIGHT"] = 0x38; t["NINE"] = 0x39;

    // Numpad
    t["NUM_ZERO"] = 0x60; t["NUM_ONE"] = 0x61; t["NUM_TWO"] = 0x62;
    t["NUM_THREE"] = 0x63; t["NUM_FOUR"] = 0x64; t["NUM_FIVE"] = 0x65;
    t["NUM_SIX"] = 0x66; t["NUM_SEVEN"] = 0x67; t["NUM_EIGHT"] = 0x68;
    t["NUM_NINE"] = 0x69;

    // Function keys
    t["F1"] = 0x70; t["F2"] = 0x71; t["F3"] = 0x72; t["F4"] = 0x73;
    t["F5"] = 0x74; t["F6"] = 0x75; t["F7"] = 0x76; t["F8"] = 0x77;
    t["F9"] = 0x78; t["F10"] = 0x79; t["F11"] = 0x7A; t["F12"] = 0x7B;

    // Control keys
    t["ESCAPE"] = 0x1B;
    t["RETURN"] = 0x0D;
    t["ENTER"] = 0x0D;
    t["SPACE"] = 0x20;
    t["BACKSPACE"] = 0x08;
    t["TAB"] = 0x09;
    t["LEFT_SHIFT"] = 0xA0;
    t["RIGHT_SHIFT"] = 0xA1;
    t["LEFT_CONTROL"] = 0xA2;
    t["RIGHT_CONTROL"] = 0xA3;
    t["LEFT_ALT"] = 0xA4;
    t["RIGHT_ALT"] = 0xA5;
    t["CAPS_LOCK"] = 0x14;
    t["NUM_LOCK"] = 0x90;
    t["SCROLL_LOCK"] = 0x91;

    // Arrow keys
    t["LEFT"] = 0x25;
    t["UP"] = 0x26;
    t["RIGHT"] = 0x27;
    t["DOWN"] = 0x28;

    // Navigation
    t["INSERT"] = 0x2D;
    t["DELETE"] = 0x2E;
    t["HOME"] = 0x24;
    t["END"] = 0x23;
    t["PAGE_UP"] = 0x21;
    t["PAGE_DOWN"] = 0x22;

    // Symbols
    t["OEM_PLUS"] = 0xBB;
    t["OEM_MINUS"] = 0xBD;
    t["OEM_COMMA"] = 0xBC;
    t["OEM_PERIOD"] = 0xBE;
    t["OEM_1"] = 0xBA; // ;:
    t["OEM_2"] = 0xBF; // /?
    t["OEM_3"] = 0xC0; // `~
    t["OEM_4"] = 0xDB; // [{
    t["OEM_5"] = 0xDC; // \|
    t["OEM_6"] = 0xDD; // ]}
    t["OEM_7"] = 0xDE; // '"

    // Numpad operators
    t["MULTIPLY"] = 0x6A;
    t["ADD"] = 0x6B;
    t["SUBTRACT"] = 0x6D;
    t["DECIMAL"] = 0x6E;
    t["DIVIDE"] = 0x6F;

    // Misc
    t["PRINT_SCREEN"] = 0x2C;
    t["PAUSE"] = 0x13;
}

// ═══════════════════════════════════════════════════════════════════════
// ModifierKey — UE4SS modifier key flags
// ═══════════════════════════════════════════════════════════════════════

void register_modifier_key(sol::state& lua) {
    sol::table t = lua.create_named_table("ModifierKey");

    t["SHIFT"]   = 0x01;
    t["CONTROL"] = 0x02;
    t["ALT"]     = 0x04;
}

// ═══════════════════════════════════════════════════════════════════════
// ModKeys — alias table (UE4SS compat)
// ═══════════════════════════════════════════════════════════════════════

void register_mod_keys(sol::state& lua) {
    sol::table t = lua.create_named_table("ModKeys");

    t["SHIFT"]   = 0x01;
    t["CONTROL"] = 0x02;
    t["ALT"]     = 0x04;
}

} // namespace lua_enums
