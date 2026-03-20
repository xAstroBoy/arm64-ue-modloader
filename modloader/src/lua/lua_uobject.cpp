// modloader/src/lua/lua_uobject.cpp
// UObject Lua userdata wrapper — property access, function calls, validity checks
// __index resolves properties via Offset_Internal
// __newindex writes properties
// Methods: Get, Set, Call, IsValid, GetName, GetFullName, GetClass, GetClassName

#include "modloader/lua_uobject.h"
#include "modloader/class_rebuilder.h"
#include "modloader/reflection_walker.h"
#include "modloader/process_event_hook.h"
#include "modloader/lua_tarray.h"
#include "modloader/lua_ustruct.h"
#include "modloader/lua_types.h"
#include "modloader/symbols.h"
#include "modloader/logger.h"
#include "modloader/types.h"

#include <cstring>

namespace lua_uobject {

// ═══ ARM64 FText support ═════════════════════════════════════════════════
// FText layout (24 bytes):
//   [0x00] ITextData*       — pointer to the text data object
//   [0x08] FSharedRefCount* — reference controller
//   [0x10] uint32 Flags + padding
//
// FText::ToString() returns FString by value, which on ARM64 uses X8 sret
// (FString has non-trivial destructor → indirect return per Itanium ABI).
// This is problematic to call from inline asm.
//
// SOLUTION: Use UKismetTextLibrary::Conv_TextToString via ProcessEvent.
// This is exactly what UE4SS does — no calling convention issues since
// ProcessEvent handles all parameter marshaling through the reflection system.
//
// FText WRITE uses FText::FromString via inline asm (confirmed working).

struct FStrRaw { char16_t* data; int32_t num; int32_t max; };

// ═══ FText → string via UKismetTextLibrary::Conv_TextToString (ProcessEvent) ═══
// This avoids all ARM64 calling convention issues by going through UE4's
// reflection/ProcessEvent system — exactly what UE4SS does.
static std::string ftext_to_string_via_kismet(const void* ftext_ptr) {
    // Cache the CDO, function, and param info on first call
    static ue::UObject* s_cdo = nullptr;
    static ue::UFunction* s_func = nullptr;
    static int32_t s_text_param_offset = -1;
    static int32_t s_ret_offset = -1;
    static uint16_t s_parms_size = 0;
    static bool s_init_failed = false;

    if (s_init_failed) return "";

    if (!s_func) {
        if (!symbols::ProcessEvent) {
            s_init_failed = true;
            logger::log_warn("FTEXT", "Conv_TextToString: ProcessEvent not available");
            return "";
        }

        // Strategy: rebuild KismetTextLibrary class to get the function,
        // then find the CDO as "Default__KismetTextLibrary" in GUObjectArray.
        auto* rc = rebuilder::rebuild("KismetTextLibrary");
        if (!rc) {
            s_init_failed = true;
            logger::log_warn("FTEXT", "Conv_TextToString: failed to rebuild KismetTextLibrary");
            return "";
        }

        auto* rf = rc->find_function("Conv_TextToString");
        if (!rf || !rf->raw) {
            s_init_failed = true;
            logger::log_warn("FTEXT", "Conv_TextToString: function not found on KismetTextLibrary");
            return "";
        }

        // Find the CDO — search GUObjectArray for "Default__KismetTextLibrary"
        s_cdo = reflection::find_object_by_name("Default__KismetTextLibrary");
        if (!s_cdo) {
            // Fallback: try to find any instance of KismetTextLibrary
            s_cdo = reflection::find_first_instance("KismetTextLibrary");
        }
        if (!s_cdo) {
            // Last resort: try StaticFindObject with short name
            if (symbols::StaticFindObject) {
                s_cdo = symbols::StaticFindObject(nullptr, nullptr, L"Default__KismetTextLibrary", false);
            }
        }
        if (!s_cdo) {
            s_init_failed = true;
            logger::log_warn("FTEXT", "Conv_TextToString: KismetTextLibrary CDO not found");
            return "";
        }

        logger::log_info("FTEXT", "Conv_TextToString: CDO found @ %p (%s)",
            s_cdo, reflection::get_short_name(s_cdo).c_str());

        s_func = static_cast<ue::UFunction*>(rf->raw);
        s_parms_size = ue::ufunc_get_parms_size(s_func);

        // Find param offsets from reflection data
        for (const auto& pi : rf->params) {
            if (pi.type == reflection::PropType::TextProperty && !(pi.flags & ue::CPF_ReturnParm)) {
                s_text_param_offset = pi.offset;
            }
        }
        // Accept return value of type StrProperty OR TextProperty
        if (rf->return_prop) {
            s_ret_offset = rf->return_offset;
        }

        logger::log_info("FTEXT", "Conv_TextToString ready: parms=%d text@%d ret@%d",
            s_parms_size, s_text_param_offset, s_ret_offset);

        if (s_text_param_offset < 0 || s_ret_offset < 0 || s_parms_size == 0) {
            s_init_failed = true;
            logger::log_warn("FTEXT", "Conv_TextToString: bad param layout");
            return "";
        }
    }

    // Allocate and zero the params buffer
    std::vector<uint8_t> params_buf(s_parms_size, 0);

    // Copy the 24-byte FText into the input param position
    // We're just copying the raw bytes (pointers) — NOT incrementing refcount.
    // This is safe because ProcessEvent won't destroy the input (it's const FText&).
    std::memcpy(params_buf.data() + s_text_param_offset, ftext_ptr, 24);

    // Safety: check the FText has a valid ITextData pointer before calling
    void* text_data_check = *reinterpret_cast<void* const*>(
        params_buf.data() + s_text_param_offset);
    if (!text_data_check || !ue::is_mapped_ptr(text_data_check)) {
        return "";  // empty/null FText — return empty string
    }

    // Call ProcessEvent — Conv_TextToString returns FString in the params buffer
    symbols::ProcessEvent(s_cdo, s_func, params_buf.data());

    // Read the return FString from the params buffer at offset 24
    // param[1] is ReturnValue: StrProperty, offset=24, size=16
    const uint8_t* ret_ptr = params_buf.data() + s_ret_offset;
    const FStrRaw* fstr = reinterpret_cast<const FStrRaw*>(ret_ptr);

    std::string result;
    if (fstr->data && fstr->num > 0 && ue::is_mapped_ptr(fstr->data)) {
        // Convert UTF-16 to UTF-8
        int count = fstr->num - 1; // num includes null terminator
        for (int i = 0; i < count; i++) {
            char16_t c = fstr->data[i];
            if (c < 0x80) result += static_cast<char>(c);
            else if (c < 0x800) {
                result += static_cast<char>(0xC0 | (c >> 6));
                result += static_cast<char>(0x80 | (c & 0x3F));
            } else {
                result += static_cast<char>(0xE0 | (c >> 12));
                result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                result += static_cast<char>(0x80 | (c & 0x3F));
            }
        }
        // NOTE: The FString data was allocated by UE4's allocator (FMemory::Malloc).
        // Do NOT call free() on it — that would corrupt the heap.
        // Accept a small leak per FText read. The data is typically small.
        // TODO: Use FMemory::Free if/when we resolve that symbol.
    }

    return result;
}

// ═══ String → FText via UKismetTextLibrary::Conv_StringToText (ProcessEvent) ═══
// Creates an FText from a UTF-8 string using UE4's own allocator.
// This ensures the FText's internal data uses FMemory::Malloc, so
// when UE4 later destroys it (e.g., via SetText), it calls FMemory::Free
// on data allocated by FMemory::Malloc — no allocator mismatch.
// Writes the resulting 24-byte FText directly to out_ftext.
static bool ftext_from_string_via_kismet(void* out_ftext, const std::string& str) {
    static ue::UObject* s_cdo = nullptr;
    static ue::UFunction* s_func = nullptr;
    static int32_t s_str_param_offset = -1;
    static int32_t s_ret_offset = -1;
    static uint16_t s_parms_size = 0;
    static bool s_init_failed = false;

    if (s_init_failed) return false;

    if (!s_func) {
        if (!symbols::ProcessEvent) { s_init_failed = true; return false; }

        auto* rc = rebuilder::rebuild("KismetTextLibrary");
        if (!rc) { s_init_failed = true; return false; }

        auto* rf = rc->find_function("Conv_StringToText");
        if (!rf || !rf->raw) { s_init_failed = true; return false; }

        // Reuse the CDO we already cached for Conv_TextToString
        s_cdo = reflection::find_object_by_name("Default__KismetTextLibrary");
        if (!s_cdo) { s_init_failed = true; return false; }

        s_func = static_cast<ue::UFunction*>(rf->raw);
        s_parms_size = ue::ufunc_get_parms_size(s_func);

        for (const auto& pi : rf->params) {
            if (pi.type == reflection::PropType::StrProperty && !(pi.flags & ue::CPF_ReturnParm)) {
                s_str_param_offset = pi.offset;
            }
        }
        if (rf->return_prop) {
            s_ret_offset = rf->return_offset;
        }

        logger::log_info("FTEXT", "Conv_StringToText ready: parms=%d str@%d ret@%d",
            s_parms_size, s_str_param_offset, s_ret_offset);

        if (s_str_param_offset < 0 || s_ret_offset < 0 || s_parms_size == 0) {
            s_init_failed = true;
            return false;
        }
    }

    // Allocate and zero the params buffer
    std::vector<uint8_t> params_buf(s_parms_size, 0);
    logger::log_info("FTEXT", "DIAG: params_buf allocated, size=%d, ptr=%p",
        (int)s_parms_size, params_buf.data());

    // Fill the FString input param: { char16_t* Data, int32 Num, int32 Max }
    size_t wlen = str.size() + 1;
    char16_t* wbuf = new char16_t[wlen];
    for (size_t i = 0; i < str.size(); i++)
        wbuf[i] = static_cast<char16_t>(static_cast<unsigned char>(str[i]));
    wbuf[str.size()] = 0;

    logger::log_info("FTEXT", "DIAG: FString ready, wbuf=%p, wlen=%d, str='%s'",
        wbuf, (int)wlen, str.c_str());

    FStrRaw* fstr = reinterpret_cast<FStrRaw*>(params_buf.data() + s_str_param_offset);
    fstr->data = wbuf;
    fstr->num = static_cast<int32_t>(wlen);
    fstr->max = static_cast<int32_t>(wlen);

    logger::log_info("FTEXT", "DIAG: FStrRaw set at offset %d: data=%p num=%d max=%d",
        s_str_param_offset, fstr->data, fstr->num, fstr->max);
    logger::log_info("FTEXT", "DIAG: CDO=%p, func=%p, params=%p, about to call ProcessEvent",
        s_cdo, s_func, params_buf.data());

    // Call Conv_StringToText via ProcessEvent
    auto original_pe = pe_hook::get_original();
    if (original_pe) {
        logger::log_info("FTEXT", "DIAG: Using ORIGINAL ProcessEvent (bypass hook) at %p", (void*)original_pe);
        original_pe(s_cdo, s_func, params_buf.data());
    } else {
        logger::log_info("FTEXT", "DIAG: Using HOOKED ProcessEvent at %p", (void*)symbols::ProcessEvent);
        symbols::ProcessEvent(s_cdo, s_func, params_buf.data());
    }

    logger::log_info("FTEXT", "DIAG: ProcessEvent RETURNED (no crash), copying FText from offset %d", s_ret_offset);

    // Copy the 24-byte FText return value to the output buffer
    std::memcpy(out_ftext, params_buf.data() + s_ret_offset, 24);

    logger::log_info("FTEXT", "DIAG: FText copied to out_ftext=%p, done!", out_ftext);

    // DO NOT delete[] wbuf — ProcessEvent may have already freed the FString
    // data via FMemory::Free. Accept a small leak for safety.
    // delete[] wbuf;

    return true;
}

// Convert FStrRaw (UTF-16) to std::string (UTF-8)
// Used for FString property reading and call return values
static std::string fstr_to_utf8(const FStrRaw& fstr, bool owns_data = false) {
    if (!fstr.data || fstr.num <= 0) return "";
    std::string utf8;
    int count = fstr.num - 1; // num includes null terminator
    for (int i = 0; i < count; i++) {
        char16_t c = fstr.data[i];
        if (c < 0x80) utf8 += static_cast<char>(c);
        else if (c < 0x800) {
            utf8 += static_cast<char>(0xC0 | (c >> 6));
            utf8 += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            utf8 += static_cast<char>(0xE0 | (c >> 12));
            utf8 += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            utf8 += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    if (owns_data) free(fstr.data);
    return utf8;
}

// FText::FromString(FString&&) → FText  (static member function)
// FText is 24 bytes > 16 → always indirect return via X8 on ARM64
// X0 = FString* param, X8 = sret (FText buffer to write into)
static inline void arm64_call_ftext_fromstring(void* out_ftext, void* fstring_param) {
    if (!symbols::FText_FromString) return;
    __asm__ __volatile__(
        "mov x8, %0\n\t"   // sret buffer → X8
        "mov x0, %1\n\t"   // FString&& param → X0
        "blr %2\n\t"       // call FText::FromString
        :
        : "r"(out_ftext), "r"(fstring_param), "r"(symbols::FText_FromString)
        : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
          "x9", "x10", "x11", "x12", "x13", "x14", "x15",
          "x16", "x17", "x30", "memory"
    );
}

// ═══ Property read helper ═══════════════════════════════════════════════
static sol::object read_property_value(sol::state_view lua, ue::UObject* obj,
                                        const rebuilder::RebuiltProperty& prop) {
    if (!obj) return sol::nil;
    // msync page probe — catches freed/dangling UObject pointers before dereference
    if (!ue::is_mapped_ptr(obj)) return sol::nil;
    // Also probe the page at the target offset to catch cross-page overflows
    const uint8_t* target = reinterpret_cast<const uint8_t*>(obj) + prop.offset;
    if (!ue::is_mapped_ptr(target)) return sol::nil;

    const uint8_t* base = reinterpret_cast<const uint8_t*>(obj);

    switch (prop.type) {
        case reflection::PropType::BoolProperty:
            return sol::make_object(lua, prop.read_bool(obj));

        case reflection::PropType::ByteProperty:
            return sol::make_object(lua, static_cast<int>(base[prop.offset]));

        case reflection::PropType::Int8Property:
            return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const int8_t*>(base + prop.offset)));

        case reflection::PropType::Int16Property:
            return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const int16_t*>(base + prop.offset)));

        case reflection::PropType::UInt16Property:
            return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const uint16_t*>(base + prop.offset)));

        case reflection::PropType::IntProperty:
            return sol::make_object(lua, *reinterpret_cast<const int32_t*>(base + prop.offset));

        case reflection::PropType::UInt32Property:
            return sol::make_object(lua, static_cast<double>(*reinterpret_cast<const uint32_t*>(base + prop.offset)));

        case reflection::PropType::Int64Property:
            return sol::make_object(lua, static_cast<double>(*reinterpret_cast<const int64_t*>(base + prop.offset)));

        case reflection::PropType::UInt64Property:
            return sol::make_object(lua, static_cast<double>(*reinterpret_cast<const uint64_t*>(base + prop.offset)));

        case reflection::PropType::FloatProperty:
            return sol::make_object(lua, *reinterpret_cast<const float*>(base + prop.offset));

        case reflection::PropType::DoubleProperty:
            return sol::make_object(lua, *reinterpret_cast<const double*>(base + prop.offset));

        case reflection::PropType::NameProperty: {
            // FName is at offset — resolve it via the FNamePool
            int32_t fname_index = *reinterpret_cast<const int32_t*>(base + prop.offset);
            std::string name = reflection::fname_to_string(fname_index);
            return sol::make_object(lua, name);
        }

        case reflection::PropType::StrProperty: {
            // FString: { TCHAR* Data; int32 ArrayNum; int32 ArrayMax; }
            struct FString {
                char16_t* data;
                int32_t num;
                int32_t max;
            };
            const FString* fstr = reinterpret_cast<const FString*>(base + prop.offset);
            if (fstr->data && fstr->num > 0 && ue::is_mapped_ptr(fstr->data)) {
                // Convert UTF-16 to UTF-8
                std::string utf8;
                for (int i = 0; i < fstr->num - 1; i++) {
                    char16_t c = fstr->data[i];
                    if (c < 0x80) {
                        utf8 += static_cast<char>(c);
                    } else if (c < 0x800) {
                        utf8 += static_cast<char>(0xC0 | (c >> 6));
                        utf8 += static_cast<char>(0x80 | (c & 0x3F));
                    } else {
                        utf8 += static_cast<char>(0xE0 | (c >> 12));
                        utf8 += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                        utf8 += static_cast<char>(0x80 | (c & 0x3F));
                    }
                }
                return sol::make_object(lua, utf8);
            }
            return sol::make_object(lua, std::string(""));
        }

        case reflection::PropType::TextProperty: {
            // FText is 0x18 (24) bytes. Read via UKismetTextLibrary::Conv_TextToString.
            const void* ftext_ptr = base + prop.offset;
            std::string utf8 = ftext_to_string_via_kismet(ftext_ptr);
            return sol::make_object(lua, utf8);
        }

        case reflection::PropType::ObjectProperty:
        case reflection::PropType::WeakObjectProperty:
        case reflection::PropType::SoftObjectProperty:
        case reflection::PropType::LazyObjectProperty:
        case reflection::PropType::InterfaceProperty: {
            ue::UObject* ptr = *reinterpret_cast<ue::UObject* const*>(base + prop.offset);
            if (!ptr || !ue::is_valid_ptr(ptr)) return sol::nil;
            // Wrap as LuaUObject
            LuaUObject wrapped;
            wrapped.ptr = ptr;
            return sol::make_object(lua, wrapped);
        }

        case reflection::PropType::ClassProperty: {
            ue::UClass* cls = *reinterpret_cast<ue::UClass* const*>(base + prop.offset);
            if (!cls || !ue::is_valid_ptr(cls)) return sol::nil;
            LuaUObject wrapped;
            wrapped.ptr = reinterpret_cast<ue::UObject*>(cls);
            return sol::make_object(lua, wrapped);
        }

        case reflection::PropType::StructProperty: {
            // Return as LuaUStruct with typed field access via reflection
            ue::UStruct* inner_struct = ue::read_field<ue::UStruct*>(prop.raw, ue::fprop::STRUCT_INNER_STRUCT);
            if (inner_struct) {
                lua_ustruct::LuaUStruct s;
                s.data = const_cast<uint8_t*>(base + prop.offset);
                s.ustruct = inner_struct;
                s.size = prop.element_size;
                s.owns_data = false;  // Points into live UObject memory
                return sol::make_object(lua, s);
            }
            // Fallback if inner struct not resolved
            return sol::make_object(lua, sol::lightuserdata_value(const_cast<uint8_t*>(base + prop.offset)));
        }

        case reflection::PropType::ArrayProperty: {
            // Return as proper LuaTArray usertype with typed element access
            ue::FProperty* inner_prop = ue::read_field<ue::FProperty*>(prop.raw, ue::fprop::ARRAY_INNER);
            lua_tarray::LuaTArray arr;
            arr.array_ptr = const_cast<uint8_t*>(base + prop.offset);
            arr.inner_prop = inner_prop;
            arr.element_size = inner_prop ? ue::fprop_get_element_size(inner_prop) : 0;
            return sol::make_object(lua, arr);
        }

        case reflection::PropType::EnumProperty: {
            // Read the underlying integer value
            if (prop.element_size == 1) return sol::make_object(lua, static_cast<int>(base[prop.offset]));
            if (prop.element_size == 2) return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const int16_t*>(base + prop.offset)));
            if (prop.element_size == 4) return sol::make_object(lua, *reinterpret_cast<const int32_t*>(base + prop.offset));
            return sol::make_object(lua, static_cast<int>(base[prop.offset]));
        }

        case reflection::PropType::MapProperty: {
            // Return as LuaTMap with key/value FProperty dispatch
            ue::FProperty* key_prop = ue::read_field<ue::FProperty*>(prop.raw, ue::fprop::MAP_KEY_PROP);
            ue::FProperty* val_prop = ue::read_field<ue::FProperty*>(prop.raw, ue::fprop::MAP_VALUE_PROP);
            lua_tarray::LuaTMap map;
            map.map_ptr = const_cast<uint8_t*>(base + prop.offset);
            map.key_prop = key_prop;
            map.value_prop = val_prop;
            map.key_size = key_prop ? ue::fprop_get_element_size(key_prop) : 0;
            map.value_size = val_prop ? ue::fprop_get_element_size(val_prop) : 0;
            return sol::make_object(lua, map);
        }

        default:
            // Raw memory access for unknown types — return as lightuserdata
            return sol::make_object(lua, sol::lightuserdata_value(const_cast<uint8_t*>(base + prop.offset)));
    }
}

// ═══ Property write helper ══════════════════════════════════════════════
static bool write_property_value(ue::UObject* obj, const rebuilder::RebuiltProperty& prop,
                                  const sol::object& value) {
    if (!obj) return false;
    // msync page probe — catches freed/dangling UObject pointers before write
    if (!ue::is_mapped_ptr(obj)) return false;
    // Probe target offset page too
    const uint8_t* target = reinterpret_cast<const uint8_t*>(obj) + prop.offset;
    if (!ue::is_mapped_ptr(target)) return false;

    uint8_t* base = reinterpret_cast<uint8_t*>(obj);

    switch (prop.type) {
        case reflection::PropType::BoolProperty: {
            bool val = value.as<bool>();
            prop.write_bool(obj, val);
            return true;
        }

        case reflection::PropType::ByteProperty:
        case reflection::PropType::Int8Property: {
            int val = value.as<int>();
            base[prop.offset] = static_cast<uint8_t>(val);
            return true;
        }

        case reflection::PropType::Int16Property:
        case reflection::PropType::UInt16Property: {
            int val = value.as<int>();
            *reinterpret_cast<int16_t*>(base + prop.offset) = static_cast<int16_t>(val);
            return true;
        }

        case reflection::PropType::IntProperty: {
            int32_t val = value.as<int32_t>();
            *reinterpret_cast<int32_t*>(base + prop.offset) = val;
            return true;
        }

        case reflection::PropType::UInt32Property: {
            uint32_t val = static_cast<uint32_t>(value.as<double>());
            *reinterpret_cast<uint32_t*>(base + prop.offset) = val;
            return true;
        }

        case reflection::PropType::Int64Property: {
            int64_t val = static_cast<int64_t>(value.as<double>());
            *reinterpret_cast<int64_t*>(base + prop.offset) = val;
            return true;
        }

        case reflection::PropType::UInt64Property: {
            uint64_t val = static_cast<uint64_t>(value.as<double>());
            *reinterpret_cast<uint64_t*>(base + prop.offset) = val;
            return true;
        }

        case reflection::PropType::FloatProperty: {
            float val = value.as<float>();
            *reinterpret_cast<float*>(base + prop.offset) = val;
            return true;
        }

        case reflection::PropType::DoubleProperty: {
            double val = value.as<double>();
            *reinterpret_cast<double*>(base + prop.offset) = val;
            return true;
        }

        case reflection::PropType::ObjectProperty:
        case reflection::PropType::WeakObjectProperty:
        case reflection::PropType::ClassProperty: {
            if (value.is<LuaUObject>()) {
                LuaUObject& wrapped = value.as<LuaUObject&>();
                *reinterpret_cast<ue::UObject**>(base + prop.offset) = wrapped.ptr;
                return true;
            } else if (value.get_type() == sol::type::lightuserdata) {
                void* ptr = value.as<void*>();
                *reinterpret_cast<void**>(base + prop.offset) = ptr;
                return true;
            } else if (value == sol::nil) {
                *reinterpret_cast<void**>(base + prop.offset) = nullptr;
                return true;
            }
            return false;
        }

        case reflection::PropType::StructProperty: {
            // Accept LuaUStruct — memcpy into the property's memory
            if (value.is<lua_ustruct::LuaUStruct>()) {
                const lua_ustruct::LuaUStruct& src = value.as<const lua_ustruct::LuaUStruct&>();
                if (src.data && src.size > 0) {
                    int32_t copy_size = (src.size < prop.element_size) ? src.size : prop.element_size;
                    std::memcpy(base + prop.offset, src.data, copy_size);
                }
                return true;
            }
            // Accept Lua table — fill fields via reflection
            if (value.get_type() == sol::type::table) {
                ue::UStruct* inner_struct = ue::read_field<ue::UStruct*>(prop.raw, ue::fprop::STRUCT_INNER_STRUCT);
                if (inner_struct) {
                    lua_ustruct::fill_from_table(base + prop.offset, inner_struct, value.as<sol::table>());
                    return true;
                }
            }
            logger::log_warn("UOBJ", "Cannot write struct property '%s' — pass a UStruct or table", prop.name.c_str());
            return false;
        }

        case reflection::PropType::EnumProperty: {
            int val = value.as<int>();
            if (prop.element_size == 1) base[prop.offset] = static_cast<uint8_t>(val);
            else if (prop.element_size == 2) *reinterpret_cast<int16_t*>(base + prop.offset) = static_cast<int16_t>(val);
            else *reinterpret_cast<int32_t*>(base + prop.offset) = val;
            return true;
        }

        case reflection::PropType::TextProperty: {
            // FText is 0x18 (24) bytes. Use Conv_StringToText via ProcessEvent
            // to ensure data is allocated by UE4's allocator (FMemory::Malloc),
            // avoiding heap corruption when UE4 later destroys the FText.
            std::string str;
            if (value.is<std::string>()) {
                str = value.as<std::string>();
            } else if (value.is<lua_types::LuaFText>()) {
                str = value.as<lua_types::LuaFText&>().to_string();
            } else if (value.is<lua_types::LuaFString>()) {
                str = value.as<lua_types::LuaFString&>().to_string();
            } else {
                logger::log_warn("UOBJ", "Cannot write FText '%s' — expected string", prop.name.c_str());
                return false;
            }
            // Destroy old FText (decrement refcount)
            if (symbols::FText_Dtor) {
                symbols::FText_Dtor(base + prop.offset);
            }
            // Create new FText via ProcessEvent (UE4 allocator)
            if (!ftext_from_string_via_kismet(base + prop.offset, str)) {
                // Fallback to arm64 asm if ProcessEvent approach fails
                if (symbols::FText_FromString) {
                    size_t wlen = str.size() + 1;
                    char16_t* wbuf = static_cast<char16_t*>(malloc(wlen * sizeof(char16_t)));
                    for (size_t i = 0; i < str.size(); i++)
                        wbuf[i] = static_cast<char16_t>(static_cast<unsigned char>(str[i]));
                    wbuf[str.size()] = 0;
                    struct { char16_t* data; int32_t num; int32_t max; } tmp_fstr = {
                        wbuf, static_cast<int32_t>(wlen), static_cast<int32_t>(wlen)
                    };
                    arm64_call_ftext_fromstring(base + prop.offset, &tmp_fstr);
                    // Don't free wbuf — ownership transferred to FText
                } else {
                    logger::log_warn("UOBJ", "Cannot write FText '%s' — no write method available", prop.name.c_str());
                    return false;
                }
            }
            logger::log_info("UOBJ", "Wrote FText '%s' = \"%s\"", prop.name.c_str(), str.c_str());
            return true;
        }

        default:
            logger::log_warn("UOBJ", "Cannot write property '%s' of unsupported type", prop.name.c_str());
            return false;
    }
}

// ═══ ProcessEvent call helper ═══════════════════════════════════════════
static sol::object call_ufunction(sol::state_view lua, ue::UObject* obj,
                                   const std::string& func_name, sol::variadic_args va) {
    if (!obj || !symbols::ProcessEvent) return sol::nil;

    ue::UClass* cls = ue::uobj_get_class(obj);
    if (!cls) return sol::nil;

    std::string class_name = reflection::get_short_name(reinterpret_cast<const ue::UObject*>(cls));
    auto* rc = rebuilder::rebuild(class_name);
    if (!rc) {
        logger::log_warn("UOBJ", "Cannot call %s — class '%s' not rebuilt", func_name.c_str(), class_name.c_str());
        return sol::nil;
    }

    auto* rf = rc->find_function(func_name);
    if (!rf || !rf->raw) {
        logger::log_warn("UOBJ", "Function '%s' not found on class '%s'", func_name.c_str(), class_name.c_str());
        return sol::nil;
    }

    ue::UFunction* func = static_cast<ue::UFunction*>(rf->raw);

    // Allocate params buffer
    uint16_t parms_size = ue::ufunc_get_parms_size(func);
    if (parms_size == 0) {
        // No params — just call
        symbols::ProcessEvent(obj, func, nullptr);
        return sol::nil;
    }

    // Allocate and zero the params buffer
    std::vector<uint8_t> params_buf(parms_size, 0);
    void* params = params_buf.data();

    // Track FString allocations to clean up after ProcessEvent
    std::vector<char16_t*> fstring_allocs;
    // Track FText param offsets to destroy after ProcessEvent
    std::vector<uint8_t*> ftext_params;

    // Fill in parameters from variadic args — ONLY properties with CPF_Parm
    // (skip Blueprint local variables which are also stored as FProperties)
    int arg_idx = 0;
    for (const auto& pi : rf->params) {
        if (arg_idx >= static_cast<int>(va.size())) break;

        // Skip non-parameter properties (Blueprint locals, return values)
        if (!(pi.flags & ue::CPF_Parm) || (pi.flags & ue::CPF_ReturnParm)) continue;

        sol::object arg = va[arg_idx];
        uint8_t* param_ptr = params_buf.data() + pi.offset;

        switch (pi.type) {
            case reflection::PropType::BoolProperty: {
                bool val = arg.as<bool>();
                if (pi.bool_byte_mask) {
                    if (val) param_ptr[pi.bool_byte_offset] |= pi.bool_byte_mask;
                    else param_ptr[pi.bool_byte_offset] &= ~pi.bool_byte_mask;
                } else {
                    *reinterpret_cast<bool*>(param_ptr) = val;
                }
                break;
            }
            case reflection::PropType::FloatProperty:
                *reinterpret_cast<float*>(param_ptr) = arg.as<float>();
                break;
            case reflection::PropType::DoubleProperty:
                *reinterpret_cast<double*>(param_ptr) = arg.as<double>();
                break;
            case reflection::PropType::IntProperty:
                *reinterpret_cast<int32_t*>(param_ptr) = arg.as<int32_t>();
                break;
            case reflection::PropType::ByteProperty:
                *param_ptr = static_cast<uint8_t>(arg.as<int>());
                break;
            case reflection::PropType::NameProperty: {
                // FName = { int32 ComparisonIndex, int32 Number }
                if (arg.is<lua_types::LuaFName>()) {
                    auto& fn = arg.as<lua_types::LuaFName&>();
                    *reinterpret_cast<int32_t*>(param_ptr) = fn.comparison_index;
                    *reinterpret_cast<int32_t*>(param_ptr + 4) = fn.number;
                } else if (arg.is<std::string>()) {
                    std::string s = arg.as<std::string>();
                    int32_t idx = reflection::fname_string_to_index(s);
                    if (idx == 0 && symbols::FName_Init) {
                        ue::FName fname;
                        std::wstring wname(s.begin(), s.end());
                        symbols::FName_Init(&fname, wname.c_str(), 0);
                        idx = fname.ComparisonIndex;
                    }
                    *reinterpret_cast<int32_t*>(param_ptr) = idx;
                    *reinterpret_cast<int32_t*>(param_ptr + 4) = 0;
                } else if (arg.is<int32_t>()) {
                    *reinterpret_cast<int32_t*>(param_ptr) = arg.as<int32_t>();
                    *reinterpret_cast<int32_t*>(param_ptr + 4) = 0;
                }
                break;
            }
            case reflection::PropType::StrProperty: {
                // FString = { char16_t* Data; int32 Num; int32 Max; }
                struct FStr { char16_t* data; int32_t num; int32_t max; };
                FStr* fstr = reinterpret_cast<FStr*>(param_ptr);
                if (arg.is<std::string>()) {
                    std::string s = arg.as<std::string>();
                    // Convert UTF-8 to UTF-16 (ASCII subset)
                    size_t wlen = s.size() + 1; // +1 for null terminator
                    char16_t* wbuf = new char16_t[wlen];
                    fstring_allocs.push_back(wbuf);
                    for (size_t i = 0; i < s.size(); i++) {
                        wbuf[i] = static_cast<char16_t>(static_cast<unsigned char>(s[i]));
                    }
                    wbuf[s.size()] = 0;
                    fstr->data = wbuf;
                    fstr->num = static_cast<int32_t>(wlen);
                    fstr->max = static_cast<int32_t>(wlen);
                } else if (arg.is<lua_types::LuaFString>()) {
                    auto& fs = arg.as<lua_types::LuaFString&>();
                    std::string s = fs.to_string();
                    size_t wlen = s.size() + 1;
                    char16_t* wbuf = new char16_t[wlen];
                    fstring_allocs.push_back(wbuf);
                    for (size_t i = 0; i < s.size(); i++) {
                        wbuf[i] = static_cast<char16_t>(static_cast<unsigned char>(s[i]));
                    }
                    wbuf[s.size()] = 0;
                    fstr->data = wbuf;
                    fstr->num = static_cast<int32_t>(wlen);
                    fstr->max = static_cast<int32_t>(wlen);
                }
                break;
            }
            case reflection::PropType::EnumProperty: {
                int val = arg.as<int>();
                if (pi.element_size == 1) *param_ptr = static_cast<uint8_t>(val);
                else if (pi.element_size == 2) *reinterpret_cast<int16_t*>(param_ptr) = static_cast<int16_t>(val);
                else *reinterpret_cast<int32_t*>(param_ptr) = val;
                break;
            }
            case reflection::PropType::ObjectProperty:
            case reflection::PropType::ClassProperty: {
                if (arg.is<LuaUObject>()) {
                    *reinterpret_cast<ue::UObject**>(param_ptr) = arg.as<LuaUObject&>().ptr;
                } else if (arg.get_type() == sol::type::lightuserdata) {
                    *reinterpret_cast<void**>(param_ptr) = arg.as<void*>();
                }
                break;
            }
            case reflection::PropType::StructProperty: {
                // Accept LuaUStruct — memcpy into params buffer
                if (arg.is<lua_ustruct::LuaUStruct>()) {
                    const lua_ustruct::LuaUStruct& src = arg.as<const lua_ustruct::LuaUStruct&>();
                    if (src.data && src.size > 0) {
                        int32_t copy_size = (src.size < pi.element_size) ? src.size : pi.element_size;
                        std::memcpy(param_ptr, src.data, copy_size);
                    }
                }
                // Accept Lua table — fill fields via reflection
                else if (arg.get_type() == sol::type::table) {
                    ue::UStruct* inner_struct = ue::read_field<ue::UStruct*>(pi.raw, ue::fprop::STRUCT_INNER_STRUCT);
                    if (inner_struct) {
                        lua_ustruct::fill_from_table(param_ptr, inner_struct, arg.as<sol::table>());
                    }
                }
                break;
            }
            case reflection::PropType::TextProperty: {
                // FText = 0x18 (24) bytes in the params buffer
                // Use Conv_StringToText via ProcessEvent to ensure UE4 allocator
                std::string str;
                if (arg.is<std::string>()) {
                    str = arg.as<std::string>();
                } else if (arg.is<lua_types::LuaFText>()) {
                    str = arg.as<lua_types::LuaFText&>().to_string();
                } else if (arg.is<lua_types::LuaFString>()) {
                    str = arg.as<lua_types::LuaFString&>().to_string();
                }
                if (!str.empty()) {
                    logger::log_info("CALL", "DIAG: TextProperty arg='%s', param_ptr=%p, offset in params=%d",
                        str.c_str(), param_ptr, (int)(param_ptr - params_buf.data()));
                    if (!ftext_from_string_via_kismet(param_ptr, str)) {
                        logger::log_info("CALL", "DIAG: kismet failed, trying arm64 fallback");
                        // Fallback to arm64 asm
                        if (symbols::FText_FromString && symbols::FText_Ctor) {
                            symbols::FText_Ctor(param_ptr);
                            size_t wlen = str.size() + 1;
                            char16_t* wbuf = static_cast<char16_t*>(malloc(wlen * sizeof(char16_t)));
                            for (size_t i = 0; i < str.size(); i++)
                                wbuf[i] = static_cast<char16_t>(static_cast<unsigned char>(str[i]));
                            wbuf[str.size()] = 0;
                            struct { char16_t* data; int32_t num; int32_t max; } tmp_fstr = {
                                wbuf, static_cast<int32_t>(wlen), static_cast<int32_t>(wlen)
                            };
                            // NOTE: Do NOT call FText_Dtor before FromString —
                            // Ctor just initialized, Dtor would destroy it.
                            // FromString writes via X8 sret (overwrites completely).
                            arm64_call_ftext_fromstring(param_ptr, &tmp_fstr);
                        }
                    }
                    ftext_params.push_back(param_ptr);
                }
                break;
            }
            default:
                // For unknown types, try raw pointer
                if (arg.get_type() == sol::type::lightuserdata) {
                    *reinterpret_cast<void**>(param_ptr) = arg.as<void*>();
                }
                break;
        }

        arg_idx++;
    }

    // Call ProcessEvent
    logger::log_info("CALL", "DIAG: About to call outer ProcessEvent: obj=%p func=%p(%s) params=%p parms_size=%d",
        obj, func, rf->name.c_str(), params, (int)parms_size);
    symbols::ProcessEvent(obj, func, params);
    logger::log_info("CALL", "DIAG: Outer ProcessEvent RETURNED (no crash)");

    // Clean up FString allocations
    for (auto* p : fstring_allocs) {
        delete[] p;
    }

    // NOTE: Do NOT call FText_Dtor on FText params after ProcessEvent.
    // ProcessEvent may share/copy the ITextData pointer into the target object.
    // If we destroy our temporary FText, the shared ITextData refcount drops,
    // potentially freeing data the target object still references — causing a
    // use-after-free crash on the next render frame.
    // Accept a small leak of one ITextData per Call() with FText params.
    // The params_buf raw bytes will be freed when the vector goes out of scope,
    // but the ITextData itself persists (shared with the target object).

    // Extract return value
    if (rf->return_prop && rf->return_prop->raw && rf->return_offset < parms_size) {
        const uint8_t* ret_ptr = params_buf.data() + rf->return_offset;

        switch (rf->return_prop->type) {
            case reflection::PropType::BoolProperty: {
                if (rf->return_prop->bool_byte_mask) {
                    return sol::make_object(lua, (ret_ptr[rf->return_prop->bool_byte_offset] & rf->return_prop->bool_byte_mask) != 0);
                }
                return sol::make_object(lua, *reinterpret_cast<const bool*>(ret_ptr));
            }
            case reflection::PropType::FloatProperty:
                return sol::make_object(lua, *reinterpret_cast<const float*>(ret_ptr));
            case reflection::PropType::DoubleProperty:
                return sol::make_object(lua, *reinterpret_cast<const double*>(ret_ptr));
            case reflection::PropType::IntProperty:
                return sol::make_object(lua, *reinterpret_cast<const int32_t*>(ret_ptr));
            case reflection::PropType::ObjectProperty: {
                ue::UObject* ret_obj = *reinterpret_cast<ue::UObject* const*>(ret_ptr);
                if (ret_obj && ue::is_valid_ptr(ret_obj)) {
                    LuaUObject wrapped;
                    wrapped.ptr = ret_obj;
                    return sol::make_object(lua, wrapped);
                }
                return sol::nil;
            }
            case reflection::PropType::StrProperty: {
                struct FStr { char16_t* data; int32_t num; int32_t max; };
                const FStr* fstr = reinterpret_cast<const FStr*>(ret_ptr);
                if (fstr->data && fstr->num > 0 && ue::is_mapped_ptr(fstr->data)) {
                    std::string utf8;
                    for (int i = 0; i < fstr->num - 1; i++) {
                        char16_t c = fstr->data[i];
                        if (c < 0x80) utf8 += static_cast<char>(c);
                        else if (c < 0x800) {
                            utf8 += static_cast<char>(0xC0 | (c >> 6));
                            utf8 += static_cast<char>(0x80 | (c & 0x3F));
                        } else {
                            utf8 += static_cast<char>(0xE0 | (c >> 12));
                            utf8 += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                            utf8 += static_cast<char>(0x80 | (c & 0x3F));
                        }
                    }
                    return sol::make_object(lua, utf8);
                }
                return sol::make_object(lua, std::string(""));
            }
            case reflection::PropType::TextProperty: {
                // Read FText return value via UKismetTextLibrary::Conv_TextToString
                std::string utf8 = ftext_to_string_via_kismet(ret_ptr);
                return sol::make_object(lua, utf8);
            }
            case reflection::PropType::StructProperty: {
                // Return struct as owning LuaUStruct copy (params buffer is about to be freed)
                ue::UStruct* inner_struct = ue::read_field<ue::UStruct*>(rf->return_prop->raw, ue::fprop::STRUCT_INNER_STRUCT);
                if (inner_struct) {
                    return sol::make_object(lua, lua_ustruct::copy(ret_ptr, inner_struct, rf->return_prop->element_size));
                }
                return sol::make_object(lua, sol::lightuserdata_value(const_cast<uint8_t*>(ret_ptr)));
            }
            default:
                return sol::make_object(lua, sol::lightuserdata_value(const_cast<uint8_t*>(ret_ptr)));
        }
    }

    return sol::nil;
}

// ═══ Register LuaUObject userdata type ══════════════════════════════════
void register_types(sol::state& lua) {
    lua.new_usertype<LuaUObject>("UObject",
        sol::no_constructor,

        // ── Core methods ──
        "IsValid", [](const LuaUObject& self) -> bool {
            return self.ptr && ue::is_valid_ptr(self.ptr) && ue::is_valid_uobject(self.ptr);
        },

        "GetName", [](const LuaUObject& self) -> std::string {
            if (!self.ptr) return "";
            return reflection::get_short_name(self.ptr);
        },

        "GetFullName", [](const LuaUObject& self) -> std::string {
            if (!self.ptr) return "";
            return reflection::get_full_name(self.ptr);
        },

        "GetClass", [](sol::this_state ts, const LuaUObject& self) -> sol::object {
            if (!self.ptr) return sol::nil;
            ue::UClass* cls = ue::uobj_get_class(self.ptr);
            if (!cls) return sol::nil;
            LuaUObject wrapped;
            wrapped.ptr = reinterpret_cast<ue::UObject*>(cls);
            return sol::make_object(ts, wrapped);
        },

        "GetClassName", [](const LuaUObject& self) -> std::string {
            if (!self.ptr) return "";
            ue::UClass* cls = ue::uobj_get_class(self.ptr);
            if (!cls) return "";
            return reflection::get_short_name(reinterpret_cast<const ue::UObject*>(cls));
        },

        "GetAddress", [](sol::this_state ts, const LuaUObject& self) -> sol::object {
            sol::state_view lua(ts);
            return sol::make_object(lua, sol::lightuserdata_value(self.ptr));
        },

        "ToHex", [](const LuaUObject& self) -> std::string {
            char buf[32];
            snprintf(buf, sizeof(buf), "0x%016lX", (unsigned long)reinterpret_cast<uintptr_t>(self.ptr));
            return std::string(buf);
        },

        // ── Property access ──
        "Get", [](sol::this_state ts, LuaUObject& self, const std::string& prop_name) -> sol::object {
            sol::state_view lua(ts);
            if (!self.ptr) return sol::nil;

            ue::UClass* cls = ue::uobj_get_class(self.ptr);
            if (!cls) return sol::nil;

            std::string class_name = reflection::get_short_name(reinterpret_cast<const ue::UObject*>(cls));
            auto* rc = rebuilder::rebuild(class_name);
            if (!rc) return sol::nil;

            auto* rp = rc->find_property(prop_name);
            if (!rp) return sol::nil;

            return read_property_value(lua, self.ptr, *rp);
        },

        "Set", [](LuaUObject& self, const std::string& prop_name, sol::object value) -> bool {
            if (!self.ptr) return false;

            ue::UClass* cls = ue::uobj_get_class(self.ptr);
            if (!cls) return false;

            std::string class_name = reflection::get_short_name(reinterpret_cast<const ue::UObject*>(cls));
            auto* rc = rebuilder::rebuild(class_name);
            if (!rc) return false;

            auto* rp = rc->find_property(prop_name);
            if (!rp) {
                logger::log_warn("UOBJ", "Property '%s' not found on '%s'", prop_name.c_str(), class_name.c_str());
                return false;
            }

            return write_property_value(self.ptr, *rp, value);
        },

        // GetProp / SetProp aliases
        "GetProp", [](sol::this_state ts, LuaUObject& self, const std::string& prop_name) -> sol::object {
            sol::state_view lua(ts);
            if (!self.ptr) return sol::nil;

            ue::UClass* cls = ue::uobj_get_class(self.ptr);
            if (!cls) return sol::nil;

            std::string class_name = reflection::get_short_name(reinterpret_cast<const ue::UObject*>(cls));
            auto* rc = rebuilder::rebuild(class_name);
            if (!rc) return sol::nil;

            auto* rp = rc->find_property(prop_name);
            if (!rp) return sol::nil;

            return read_property_value(lua, self.ptr, *rp);
        },

        "SetProp", [](LuaUObject& self, const std::string& prop_name, sol::object value) -> bool {
            if (!self.ptr) return false;

            ue::UClass* cls = ue::uobj_get_class(self.ptr);
            if (!cls) return false;

            std::string class_name = reflection::get_short_name(reinterpret_cast<const ue::UObject*>(cls));
            auto* rc = rebuilder::rebuild(class_name);
            if (!rc) return false;

            auto* rp = rc->find_property(prop_name);
            if (!rp) return false;

            return write_property_value(self.ptr, *rp, value);
        },

        // ── Function calls via ProcessEvent ──
        "Call", [](sol::this_state ts, LuaUObject& self, const std::string& func_name,
                   sol::variadic_args va) -> sol::object {
            sol::state_view lua(ts);
            return call_ufunction(lua, self.ptr, func_name, va);
        },

        "CallFunc", [](sol::this_state ts, LuaUObject& self, const std::string& func_name,
                       sol::variadic_args va) -> sol::object {
            sol::state_view lua(ts);
            return call_ufunction(lua, self.ptr, func_name, va);
        },

        // ── Per-instance hooks ──
        "HookProp", [](LuaUObject& self, const std::string& prop_name,
                       sol::function callback) -> uint64_t {
            return rebuilder::hook_property_instance(self.ptr, prop_name,
                [callback](ue::UObject* obj, const std::string& name, void* old_val, void* new_val) -> bool {
                    auto result = callback(LuaUObject{obj}, name);
                    if (result.valid() && result.get_type() == sol::type::boolean) {
                        return result.get<bool>();
                    }
                    return false;
                });
        },

        "HookFunc", [](LuaUObject& self, const std::string& func_name,
                       sol::optional<sol::function> pre, sol::optional<sol::function> post) -> uint64_t {
            rebuilder::FuncPreCallback pre_cb = nullptr;
            rebuilder::FuncPostCallback post_cb = nullptr;

            if (pre && pre->valid()) {
                sol::function pre_fn = *pre;
                pre_cb = [pre_fn](ue::UObject* self_obj, ue::UFunction* func, void* parms) -> bool {
                    auto result = pre_fn(LuaUObject{self_obj});
                    if (result.valid() && result.get_type() == sol::type::string) {
                        std::string s = result;
                        if (s == "BLOCK") return true;
                    }
                    if (result.valid() && result.get_type() == sol::type::boolean) {
                        return result.get<bool>();
                    }
                    return false;
                };
            }

            if (post && post->valid()) {
                sol::function post_fn = *post;
                post_cb = [post_fn](ue::UObject* self_obj, ue::UFunction* func, void* parms) {
                    post_fn(LuaUObject{self_obj});
                };
            }

            return rebuilder::hook_function_instance(self.ptr, func_name, pre_cb, post_cb);
        },

        // ── __index for dynamic property AND function resolution ──
        // This enables BOTH obj.PropertyName AND obj:FunctionName(args) syntax.
        // Property lookup first, then UFunction lookup returning a bound callable.
        // This is the key to UE4SS-style: obj:K2_DestroyActor(), obj:OnUnlocked(true), etc.
        sol::meta_function::index, [](sol::this_state ts, LuaUObject& self,
                                       const std::string& key) -> sol::object {
            sol::state_view lua(ts);
            if (!self.ptr) return sol::nil;

            // First check: is this a built-in method name?
            // (IsValid, GetName, GetFullName, GetClass, GetClassName, GetAddress,
            //  ToHex, Get, Set, GetProp, SetProp, Call, CallFunc, HookProp, HookFunc)
            // Sol2 handles these via the usertype definition BEFORE __index,
            // so we only get here for keys that are NOT built-in methods.

            ue::UClass* cls = ue::uobj_get_class(self.ptr);
            if (!cls) return sol::nil;

            std::string class_name = reflection::get_short_name(reinterpret_cast<const ue::UObject*>(cls));
            auto* rc = rebuilder::rebuild(class_name);
            if (!rc) return sol::nil;

            // 1) Try property lookup first
            auto* rp = rc->find_property(key);
            if (rp) return read_property_value(lua, self.ptr, *rp);

            // 2) Try UFunction lookup — return a bound callable lambda
            //    This enables obj:FuncName(args) exactly like UE4SS
            auto* rf = rc->find_function(key);
            if (rf && rf->raw) {
                // Capture self.ptr and func_name to create a callable
                ue::UObject* obj_ptr = self.ptr;
                std::string func_name = key;
                return sol::make_object(lua, [obj_ptr, func_name](sol::this_state inner_ts,
                                                                    sol::variadic_args va) -> sol::object {
                    sol::state_view inner_lua(inner_ts);
                    return call_ufunction(inner_lua, obj_ptr, func_name, va);
                });
            }

            return sol::nil;
        },

        // ── __newindex for dynamic property writing ──
        sol::meta_function::new_index, [](LuaUObject& self, const std::string& key,
                                           sol::object value) {
            if (!self.ptr) return;

            ue::UClass* cls = ue::uobj_get_class(self.ptr);
            if (!cls) return;

            std::string class_name = reflection::get_short_name(reinterpret_cast<const ue::UObject*>(cls));
            auto* rc = rebuilder::rebuild(class_name);
            if (!rc) return;

            auto* rp = rc->find_property(key);
            if (rp) {
                write_property_value(self.ptr, *rp, value);
            }
        },

        // ── Equality ──
        sol::meta_function::equal_to, [](const LuaUObject& a, const LuaUObject& b) -> bool {
            return a.ptr == b.ptr;
        },

        // ── String representation ──
        sol::meta_function::to_string, [](const LuaUObject& self) -> std::string {
            if (!self.ptr) return "UObject(nil)";
            std::string name = reflection::get_short_name(self.ptr);
            char buf[64];
            snprintf(buf, sizeof(buf), "0x%lX", (unsigned long)reinterpret_cast<uintptr_t>(self.ptr));
            return "UObject(" + name + " @ " + buf + ")";
        }
    );

    logger::log_info("LUA", "UObject userdata type registered");
}

LuaUObject wrap(ue::UObject* obj) {
    LuaUObject wrapped;
    wrapped.ptr = obj;
    return wrapped;
}

ue::UObject* unwrap(const LuaUObject& wrapped) {
    return wrapped.ptr;
}

sol::object wrap_or_nil(sol::state_view lua, ue::UObject* obj) {
    if (!obj || !ue::is_valid_ptr(obj)) return sol::nil;

    std::string name = reflection::get_short_name(obj);
    if (ue::is_default_object(name.c_str())) {
        logger::log_warn("FILTER", "Discarded %s — CDO cannot be used as instance", name.c_str());
        return sol::nil;
    }

    LuaUObject wrapped;
    wrapped.ptr = obj;
    return sol::make_object(lua, wrapped);
}

} // namespace lua_uobject
