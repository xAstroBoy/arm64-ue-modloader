// modloader/src/lua/lua_tarray.cpp
// UE4SS-compatible TArray, TMap, TSet Lua usertypes
// Element type dispatch via FProperty reflection — reads/writes individual
// elements from raw UE4 memory using the same type classification system
// as the rest of the modloader.

#include "modloader/lua_tarray.h"
#include "modloader/lua_uobject.h"
#include "modloader/lua_ustruct.h"
#include "modloader/lua_types.h"
#include "modloader/reflection_walker.h"
#include "modloader/symbols.h"
#include "modloader/logger.h"
#include "modloader/types.h"

#include <cstring>

namespace lua_tarray {

// ═══════════════════════════════════════════════════════════════════════
// UE4 TArray memory layout: { void* Data; int32 ArrayNum; int32 ArrayMax; }
// Total: 16 bytes on ARM64
// ═══════════════════════════════════════════════════════════════════════

struct RawTArray {
    void*   data;
    int32_t num;
    int32_t max;
};

// ═══════════════════════════════════════════════════════════════════════
// UE4 TMap/TSet memory layout (FScriptMap/FScriptSet):
// TSet: { FScriptSparseArray Elements; FHashAllocator Hash; }
// FScriptSparseArray: { void* Data; int32 MaxElements; int32 NumElements;
//                       FSparseArrayAllocationInfo FirstFreeIndex; }
// Simplified — we iterate valid elements via sparse array metadata
// ═══════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════
// Element type dispatch — read from raw memory via FProperty
// ═══════════════════════════════════════════════════════════════════════

sol::object read_element(sol::state_view lua, const void* element_ptr, ue::FProperty* prop) {
    if (!element_ptr || !prop) return sol::nil;

    const uint8_t* base = reinterpret_cast<const uint8_t*>(element_ptr);

    // Classify the property
    reflection::PropType pt = reflection::classify_property(reinterpret_cast<const ue::FField*>(prop));
    int32_t elem_size = ue::fprop_get_element_size(prop);

    switch (pt) {
        case reflection::PropType::BoolProperty: {
            uint8_t byte_mask = ue::read_field<uint8_t>(prop, ue::fprop::BOOL_BYTE_MASK);
            uint8_t field_mask = ue::read_field<uint8_t>(prop, ue::fprop::BOOL_FIELD_MASK);
            if (field_mask != 0xFF) {
                return sol::make_object(lua, (base[0] & field_mask) != 0);
            }
            return sol::make_object(lua, *reinterpret_cast<const bool*>(base));
        }

        case reflection::PropType::ByteProperty:
            return sol::make_object(lua, static_cast<int>(base[0]));

        case reflection::PropType::Int8Property:
            return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const int8_t*>(base)));

        case reflection::PropType::Int16Property:
            return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const int16_t*>(base)));

        case reflection::PropType::UInt16Property:
            return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const uint16_t*>(base)));

        case reflection::PropType::IntProperty:
            return sol::make_object(lua, *reinterpret_cast<const int32_t*>(base));

        case reflection::PropType::UInt32Property:
            return sol::make_object(lua, static_cast<double>(*reinterpret_cast<const uint32_t*>(base)));

        case reflection::PropType::Int64Property:
            return sol::make_object(lua, static_cast<double>(*reinterpret_cast<const int64_t*>(base)));

        case reflection::PropType::UInt64Property:
            return sol::make_object(lua, static_cast<double>(*reinterpret_cast<const uint64_t*>(base)));

        case reflection::PropType::FloatProperty:
            return sol::make_object(lua, *reinterpret_cast<const float*>(base));

        case reflection::PropType::DoubleProperty:
            return sol::make_object(lua, *reinterpret_cast<const double*>(base));

        case reflection::PropType::NameProperty: {
            int32_t fname_idx = *reinterpret_cast<const int32_t*>(base);
            return sol::make_object(lua, lua_types::LuaFName(fname_idx));
        }

        case reflection::PropType::StrProperty: {
            struct FStr { char16_t* data; int32_t num; int32_t max; };
            const FStr* fstr = reinterpret_cast<const FStr*>(base);
            if (fstr->data && fstr->num > 0) {
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
                return sol::make_object(lua, lua_types::LuaFString(utf8));
            }
            return sol::make_object(lua, lua_types::LuaFString(""));
        }

        case reflection::PropType::ObjectProperty:
        case reflection::PropType::WeakObjectProperty:
        case reflection::PropType::SoftObjectProperty:
        case reflection::PropType::LazyObjectProperty:
        case reflection::PropType::InterfaceProperty:
        case reflection::PropType::ClassProperty: {
            ue::UObject* obj = *reinterpret_cast<ue::UObject* const*>(base);
            if (!obj || !ue::is_valid_ptr(obj)) return sol::nil;
            lua_uobject::LuaUObject wrapped;
            wrapped.ptr = obj;
            return sol::make_object(lua, wrapped);
        }

        case reflection::PropType::StructProperty: {
            // Return as LuaUStruct with typed field access via reflection
            ue::UStruct* inner_struct = ue::read_field<ue::UStruct*>(prop, ue::fprop::STRUCT_INNER_STRUCT);
            if (inner_struct) {
                lua_ustruct::LuaUStruct s;
                s.data = const_cast<uint8_t*>(base);
                s.ustruct = inner_struct;
                s.size = elem_size;
                s.owns_data = false;
                return sol::make_object(lua, s);
            }
            return sol::make_object(lua, sol::lightuserdata_value(const_cast<uint8_t*>(base)));
        }

        case reflection::PropType::EnumProperty: {
            if (elem_size == 1) return sol::make_object(lua, static_cast<int>(base[0]));
            if (elem_size == 2) return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const int16_t*>(base)));
            if (elem_size == 4) return sol::make_object(lua, *reinterpret_cast<const int32_t*>(base));
            return sol::make_object(lua, static_cast<int>(base[0]));
        }

        case reflection::PropType::ArrayProperty: {
            // Nested TArray — return as LuaTArray
            ue::FProperty* nested_inner = ue::read_field<ue::FProperty*>(prop, ue::fprop::ARRAY_INNER);
            LuaTArray nested;
            nested.array_ptr = const_cast<uint8_t*>(base);
            nested.inner_prop = nested_inner;
            nested.element_size = nested_inner ? ue::fprop_get_element_size(nested_inner) : 0;
            return sol::make_object(lua, nested);
        }

        default:
            return sol::make_object(lua, sol::lightuserdata_value(const_cast<uint8_t*>(base)));
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Element type dispatch — write to raw memory via FProperty
// ═══════════════════════════════════════════════════════════════════════

void write_element(void* element_ptr, ue::FProperty* prop, const sol::object& value) {
    if (!element_ptr || !prop) return;

    uint8_t* base = reinterpret_cast<uint8_t*>(element_ptr);
    reflection::PropType pt = reflection::classify_property(reinterpret_cast<const ue::FField*>(prop));
    int32_t elem_size = ue::fprop_get_element_size(prop);

    switch (pt) {
        case reflection::PropType::BoolProperty: {
            uint8_t field_mask = ue::read_field<uint8_t>(prop, ue::fprop::BOOL_FIELD_MASK);
            bool val = value.as<bool>();
            if (field_mask != 0xFF) {
                if (val) base[0] |= field_mask;
                else base[0] &= ~field_mask;
            } else {
                *reinterpret_cast<bool*>(base) = val;
            }
            break;
        }
        case reflection::PropType::ByteProperty:
        case reflection::PropType::Int8Property:
            base[0] = static_cast<uint8_t>(value.as<int>());
            break;
        case reflection::PropType::Int16Property:
        case reflection::PropType::UInt16Property:
            *reinterpret_cast<int16_t*>(base) = static_cast<int16_t>(value.as<int>());
            break;
        case reflection::PropType::IntProperty:
            *reinterpret_cast<int32_t*>(base) = value.as<int32_t>();
            break;
        case reflection::PropType::UInt32Property:
            *reinterpret_cast<uint32_t*>(base) = static_cast<uint32_t>(value.as<double>());
            break;
        case reflection::PropType::Int64Property:
            *reinterpret_cast<int64_t*>(base) = static_cast<int64_t>(value.as<double>());
            break;
        case reflection::PropType::UInt64Property:
            *reinterpret_cast<uint64_t*>(base) = static_cast<uint64_t>(value.as<double>());
            break;
        case reflection::PropType::FloatProperty:
            *reinterpret_cast<float*>(base) = value.as<float>();
            break;
        case reflection::PropType::DoubleProperty:
            *reinterpret_cast<double*>(base) = value.as<double>();
            break;
        case reflection::PropType::ObjectProperty:
        case reflection::PropType::WeakObjectProperty:
        case reflection::PropType::ClassProperty: {
            if (value.is<lua_uobject::LuaUObject>()) {
                *reinterpret_cast<ue::UObject**>(base) = value.as<lua_uobject::LuaUObject&>().ptr;
            } else if (value == sol::nil) {
                *reinterpret_cast<ue::UObject**>(base) = nullptr;
            }
            break;
        }
        case reflection::PropType::EnumProperty: {
            if (elem_size == 1) base[0] = static_cast<uint8_t>(value.as<int>());
            else if (elem_size == 2) *reinterpret_cast<int16_t*>(base) = static_cast<int16_t>(value.as<int>());
            else *reinterpret_cast<int32_t*>(base) = value.as<int32_t>();
            break;
        }
        default:
            logger::log_warn("TARRAY", "Cannot write to element of type %d", static_cast<int>(pt));
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// LuaTArray methods
// ═══════════════════════════════════════════════════════════════════════

int32_t LuaTArray::num() const {
    if (!array_ptr) return 0;
    return reinterpret_cast<const RawTArray*>(array_ptr)->num;
}

int32_t LuaTArray::max() const {
    if (!array_ptr) return 0;
    return reinterpret_cast<const RawTArray*>(array_ptr)->max;
}

void* LuaTArray::data() const {
    if (!array_ptr) return nullptr;
    return reinterpret_cast<const RawTArray*>(array_ptr)->data;
}

bool LuaTArray::is_valid() const {
    return array_ptr != nullptr;
}

bool LuaTMap::is_valid() const {
    return map_ptr != nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// Register TArray usertype
// ═══════════════════════════════════════════════════════════════════════

static void register_tarray(sol::state& lua) {
    lua.new_usertype<LuaTArray>("TArray",
        sol::no_constructor,

        // ── __index (1-indexed, like UE4SS)
        sol::meta_function::index, [](sol::this_state ts, const LuaTArray& self, int index) -> sol::object {
            sol::state_view lua(ts);
            if (!self.array_ptr || !self.inner_prop) return sol::nil;
            int32_t arr_num = self.num();
            // 1-indexed → 0-indexed
            int idx = index - 1;
            if (idx < 0 || idx >= arr_num) return sol::nil;
            if (self.element_size <= 0) return sol::nil;
            const uint8_t* elem = reinterpret_cast<const uint8_t*>(self.data()) + idx * self.element_size;
            return read_element(lua, elem, self.inner_prop);
        },

        // ── __newindex (1-indexed)
        sol::meta_function::new_index, [](LuaTArray& self, int index, sol::object value) {
            if (!self.array_ptr || !self.inner_prop) return;
            int32_t arr_num = self.num();
            int idx = index - 1;
            if (idx < 0 || idx >= arr_num) {
                logger::log_warn("TARRAY", "Write out of bounds: index %d, num %d", index, arr_num);
                return;
            }
            if (self.element_size <= 0) return;
            uint8_t* elem = reinterpret_cast<uint8_t*>(self.data()) + idx * self.element_size;
            write_element(elem, self.inner_prop, value);
        },

        // ── __len
        sol::meta_function::length, [](const LuaTArray& self) -> int {
            return self.num();
        },

        // ── __tostring
        sol::meta_function::to_string, [](const LuaTArray& self) -> std::string {
            char buf[64];
            snprintf(buf, sizeof(buf), "TArray(Num=%d, Max=%d)", self.num(), self.max());
            return std::string(buf);
        },

        // ── Methods
        "GetArrayNum", &LuaTArray::num,
        "GetArrayMax", &LuaTArray::max,

        "GetArrayAddress", [](const LuaTArray& self) -> uintptr_t {
            return reinterpret_cast<uintptr_t>(self.array_ptr);
        },

        "GetArrayDataAddress", [](const LuaTArray& self) -> uintptr_t {
            return reinterpret_cast<uintptr_t>(self.data());
        },

        "IsEmpty", [](const LuaTArray& self) -> bool {
            return self.num() == 0;
        },

        "IsValid", &LuaTArray::is_valid,

        // ForEach — iterate all elements, callback(index, element)
        // 1-indexed. Return true from callback to break early.
        "ForEach", [](sol::this_state ts, const LuaTArray& self, sol::function callback) {
            sol::state_view lua(ts);
            if (!self.array_ptr || !self.inner_prop) return;
            int32_t n = self.num();
            if (n <= 0 || self.element_size <= 0) return;
            const uint8_t* base = reinterpret_cast<const uint8_t*>(self.data());
            if (!base) return;

            for (int32_t i = 0; i < n; i++) {
                sol::object elem = read_element(lua, base + i * self.element_size, self.inner_prop);
                auto result = callback(i + 1, elem); // 1-indexed
                if (result.valid() && result.get_type() == sol::type::boolean && result.get<bool>()) {
                    break;
                }
            }
        },

        // Empty / Clear — zero out the array count (does NOT free memory)
        "Empty", [](LuaTArray& self) {
            if (!self.array_ptr) return;
            RawTArray* raw = reinterpret_cast<RawTArray*>(self.array_ptr);
            raw->num = 0;
        },

        "Clear", [](LuaTArray& self) {
            if (!self.array_ptr) return;
            RawTArray* raw = reinterpret_cast<RawTArray*>(self.array_ptr);
            raw->num = 0;
        },

        // Add — appends an element to the end of the array.
        // REQUIRES that Num < Max (there must be pre-allocated capacity).
        // For safety, does NOT realloc — UE4 FMemory is not available from Lua.
        // Returns the new 1-based index, or 0 on failure.
        "Add", [](LuaTArray& self, sol::object value) -> int {
            if (!self.array_ptr || !self.inner_prop) return 0;
            RawTArray* raw = reinterpret_cast<RawTArray*>(self.array_ptr);
            if (raw->num >= raw->max) {
                logger::log_warn("TARRAY", "Add: array full (Num=%d, Max=%d) — cannot append",
                                 raw->num, raw->max);
                return 0;
            }
            if (self.element_size <= 0) return 0;
            uint8_t* elem = reinterpret_cast<uint8_t*>(raw->data) + raw->num * self.element_size;
            // Zero the element slot first
            std::memset(elem, 0, self.element_size);
            write_element(elem, self.inner_prop, value);
            raw->num++;
            return raw->num; // 1-based index of the new element
        },

        // AddFName — specialized: appends an FName to an FName[] array.
        // Takes a string or LuaFName. Returns new 1-based index or 0 on failure.
        "AddFName", [](LuaTArray& self, sol::object value) -> int {
            if (!self.array_ptr) return 0;
            RawTArray* raw = reinterpret_cast<RawTArray*>(self.array_ptr);
            if (raw->num >= raw->max) {
                logger::log_warn("TARRAY", "AddFName: array full (Num=%d, Max=%d)", raw->num, raw->max);
                return 0;
            }
            // FName is 8 bytes: { int32 ComparisonIndex, int32 Number }
            if (self.element_size < 8 && self.element_size != 4) {
                logger::log_warn("TARRAY", "AddFName: element_size=%d (expected 4 or 8)", self.element_size);
                return 0;
            }
            uint8_t* elem = reinterpret_cast<uint8_t*>(raw->data) + raw->num * self.element_size;
            int32_t comp_idx = 0;
            int32_t number = 0;
            if (value.is<lua_types::LuaFName>()) {
                auto& fn = value.as<lua_types::LuaFName&>();
                comp_idx = fn.comparison_index;
                number = fn.number;
            } else if (value.is<std::string>()) {
                std::string s = value.as<std::string>();
                comp_idx = reflection::fname_string_to_index(s);
                if (comp_idx == 0 && symbols::FName_Init) {
                    ue::FName fname;
                    std::wstring wname(s.begin(), s.end());
                    symbols::FName_Init(&fname, wname.c_str(), 0);
                    comp_idx = fname.ComparisonIndex;
                }
            } else if (value.is<int32_t>()) {
                comp_idx = value.as<int32_t>();
            }
            *reinterpret_cast<int32_t*>(elem) = comp_idx;
            if (self.element_size >= 8) {
                *reinterpret_cast<int32_t*>(elem + 4) = number;
            }
            raw->num++;
            return raw->num;
        }
    );
}

// ═══════════════════════════════════════════════════════════════════════
// Register TMap usertype (basic read-only iteration)
// ═══════════════════════════════════════════════════════════════════════

static void register_tmap(sol::state& lua) {
    // TMap in UE4 uses FScriptMap internally:
    // struct FScriptMap {
    //   FScriptSet Pairs; // Actually stores TPair<Key,Value> as elements
    // };
    // FScriptSet uses a sparse array + hash:
    // struct FScriptSet {
    //   FScriptSparseArray Elements; // { void* Data; int32 MaxElements;
    //                                //   int32 NumElements; int32 FirstFreeIndex; }
    //   FHashAllocator Hash;         // hash bucket metadata
    // };
    //
    // Each element in the sparse array is: { int32 NextFreeIndex; TPair<Key,Value> }
    // The first 4 bytes of each slot are the free-list link (-1 if valid/occupied).
    // Element stride = 4 (free-list int) + key_size + padding + value_size + padding

    lua.new_usertype<LuaTMap>("TMap",
        sol::no_constructor,

        "IsValid", &LuaTMap::is_valid,

        // ── __len returns count of valid entries
        sol::meta_function::length, [](const LuaTMap& self) -> int {
            if (!self.map_ptr) return 0;
            // NumElements is at offset 8 in the sparse array (Data=0, Max=4, Num=8)
            // But FScriptMap wraps FScriptSet which wraps FScriptSparseArray
            // FScriptMap → FScriptSet.Elements → FScriptSparseArray
            // Layout: { void* Data; int32 MaxElements; int32 NumElements; int32 FirstFreeIndex; }
            const int32_t* num_ptr = reinterpret_cast<const int32_t*>(
                reinterpret_cast<const uint8_t*>(self.map_ptr) + 8);
            return *num_ptr;
        },

        sol::meta_function::to_string, [](const LuaTMap& self) -> std::string {
            if (!self.map_ptr) return "TMap(invalid)";
            const int32_t* num_ptr = reinterpret_cast<const int32_t*>(
                reinterpret_cast<const uint8_t*>(self.map_ptr) + 8);
            char buf[64];
            snprintf(buf, sizeof(buf), "TMap(Num=%d)", *num_ptr);
            return std::string(buf);
        },

        // ForEach(callback) — iterates valid entries, callback(key, value)
        // Return true from callback to break.
        "ForEach", [](sol::this_state ts, const LuaTMap& self, sol::function callback) {
            sol::state_view lua(ts);
            if (!self.map_ptr || !self.key_prop || !self.value_prop) return;
            if (self.key_size <= 0 || self.value_size <= 0) return;

            // Read sparse array fields
            void* data = *reinterpret_cast<void**>(self.map_ptr);
            int32_t max_elements = *reinterpret_cast<const int32_t*>(
                reinterpret_cast<const uint8_t*>(self.map_ptr) + 4);
            if (!data || max_elements <= 0) return;

            // Each element slot: { int32 FreeListLink; TPair<K,V> }
            // Alignment needs careful handling — we compute entry stride from
            // the FProperty element sizes with 4-byte free-list prefix
            int32_t pair_size = self.key_size + self.value_size;
            // Entry: 4 bytes link + key + value, rounded up to 8-byte alignment
            int32_t entry_size = 4 + pair_size;
            entry_size = (entry_size + 7) & ~7; // Align to 8

            for (int32_t i = 0; i < max_elements; i++) {
                const uint8_t* entry = reinterpret_cast<const uint8_t*>(data) + i * entry_size;
                int32_t free_link = *reinterpret_cast<const int32_t*>(entry);

                // Valid if free_link == -1 (i.e., this slot is occupied)
                if (free_link != -1) continue;

                // Key starts at entry + 4
                const uint8_t* key_ptr = entry + 4;
                // Value starts after key (with potential alignment)
                const uint8_t* value_ptr = key_ptr + self.key_size;

                sol::object key_obj = read_element(lua, key_ptr, self.key_prop);
                sol::object val_obj = read_element(lua, value_ptr, self.value_prop);

                auto result = callback(key_obj, val_obj);
                if (result.valid() && result.get_type() == sol::type::boolean && result.get<bool>()) {
                    break;
                }
            }
        }
    );
}

// ═══════════════════════════════════════════════════════════════════════
// Register all
// ═══════════════════════════════════════════════════════════════════════

void register_all(sol::state& lua) {
    register_tarray(lua);
    register_tmap(lua);

    logger::log_info("LUA", "TArray + TMap usertypes registered (UE4SS-compatible)");
}

} // namespace lua_tarray

