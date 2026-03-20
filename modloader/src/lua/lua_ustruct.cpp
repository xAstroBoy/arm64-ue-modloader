// modloader/src/lua/lua_ustruct.cpp
// UE4 UStruct Lua userdata wrapper — typed field access via reflection
// Enables struct.X, struct.Y, struct.Z syntax for FVector, FRotator, IntPoint, etc.

#include "modloader/lua_ustruct.h"
#include "modloader/reflection_walker.h"
#include "modloader/lua_uobject.h"
#include "modloader/lua_tarray.h"
#include "modloader/lua_types.h"
#include "modloader/symbols.h"
#include "modloader/logger.h"
#include "modloader/types.h"

#include <cstring>

namespace lua_ustruct {

// ═══════════════════════════════════════════════════════════════════════
// LuaUStruct lifecycle
// ═══════════════════════════════════════════════════════════════════════

LuaUStruct::~LuaUStruct() {
    if (owns_data && data) {
        delete[] data;
        data = nullptr;
    }
}

LuaUStruct::LuaUStruct(const LuaUStruct& other)
    : ustruct(other.ustruct), size(other.size), owns_data(other.owns_data) {
    if (other.owns_data && other.data && other.size > 0) {
        data = new uint8_t[other.size];
        std::memcpy(data, other.data, other.size);
    } else {
        data = other.data;
    }
}

LuaUStruct& LuaUStruct::operator=(const LuaUStruct& other) {
    if (this == &other) return *this;
    if (owns_data && data) delete[] data;
    ustruct = other.ustruct;
    size = other.size;
    owns_data = other.owns_data;
    if (other.owns_data && other.data && other.size > 0) {
        data = new uint8_t[other.size];
        std::memcpy(data, other.data, other.size);
    } else {
        data = other.data;
    }
    return *this;
}

LuaUStruct::LuaUStruct(LuaUStruct&& other) noexcept
    : data(other.data), ustruct(other.ustruct), size(other.size), owns_data(other.owns_data) {
    other.data = nullptr;
    other.owns_data = false;
}

LuaUStruct& LuaUStruct::operator=(LuaUStruct&& other) noexcept {
    if (this == &other) return *this;
    if (owns_data && data) delete[] data;
    data = other.data;
    ustruct = other.ustruct;
    size = other.size;
    owns_data = other.owns_data;
    other.data = nullptr;
    other.owns_data = false;
    return *this;
}

bool LuaUStruct::is_valid() const {
    return data != nullptr && ustruct != nullptr && size > 0;
}

// ═══════════════════════════════════════════════════════════════════════
// Field read helper — reads a single field from struct memory
// offset is relative to the struct base (data pointer)
// ═══════════════════════════════════════════════════════════════════════

static sol::object read_field_value(sol::state_view lua, const uint8_t* base,
                                     const reflection::PropertyInfo& fi) {
    const uint8_t* ptr = base + fi.offset;
    if (!ue::is_mapped_ptr(ptr)) return sol::nil;

    switch (fi.type) {
        case reflection::PropType::BoolProperty: {
            if (fi.bool_byte_mask) {
                return sol::make_object(lua, (ptr[fi.bool_byte_offset] & fi.bool_byte_mask) != 0);
            }
            return sol::make_object(lua, *reinterpret_cast<const bool*>(ptr));
        }

        case reflection::PropType::ByteProperty:
            return sol::make_object(lua, static_cast<int>(ptr[0]));

        case reflection::PropType::Int8Property:
            return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const int8_t*>(ptr)));

        case reflection::PropType::Int16Property:
            return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const int16_t*>(ptr)));

        case reflection::PropType::UInt16Property:
            return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const uint16_t*>(ptr)));

        case reflection::PropType::IntProperty:
            return sol::make_object(lua, *reinterpret_cast<const int32_t*>(ptr));

        case reflection::PropType::UInt32Property:
            return sol::make_object(lua, static_cast<double>(*reinterpret_cast<const uint32_t*>(ptr)));

        case reflection::PropType::Int64Property:
            return sol::make_object(lua, static_cast<double>(*reinterpret_cast<const int64_t*>(ptr)));

        case reflection::PropType::UInt64Property:
            return sol::make_object(lua, static_cast<double>(*reinterpret_cast<const uint64_t*>(ptr)));

        case reflection::PropType::FloatProperty:
            return sol::make_object(lua, *reinterpret_cast<const float*>(ptr));

        case reflection::PropType::DoubleProperty:
            return sol::make_object(lua, *reinterpret_cast<const double*>(ptr));

        case reflection::PropType::NameProperty: {
            int32_t fname_idx = *reinterpret_cast<const int32_t*>(ptr);
            return sol::make_object(lua, reflection::fname_to_string(fname_idx));
        }

        case reflection::PropType::StrProperty: {
            struct FStr { char16_t* data; int32_t num; int32_t max; };
            const FStr* fstr = reinterpret_cast<const FStr*>(ptr);
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

        case reflection::PropType::ObjectProperty:
        case reflection::PropType::WeakObjectProperty:
        case reflection::PropType::SoftObjectProperty:
        case reflection::PropType::LazyObjectProperty:
        case reflection::PropType::InterfaceProperty:
        case reflection::PropType::ClassProperty: {
            ue::UObject* obj = *reinterpret_cast<ue::UObject* const*>(ptr);
            if (!obj || !ue::is_valid_ptr(obj)) return sol::nil;
            lua_uobject::LuaUObject wrapped;
            wrapped.ptr = obj;
            return sol::make_object(lua, wrapped);
        }

        case reflection::PropType::StructProperty: {
            // Nested struct — recurse with LuaUStruct
            ue::UStruct* inner = ue::read_field<ue::UStruct*>(fi.raw, ue::fprop::STRUCT_INNER_STRUCT);
            if (inner) {
                LuaUStruct nested;
                nested.data = const_cast<uint8_t*>(ptr);
                nested.ustruct = inner;
                nested.size = fi.element_size;
                nested.owns_data = false;
                return sol::make_object(lua, nested);
            }
            return sol::make_object(lua, sol::lightuserdata_value(const_cast<uint8_t*>(ptr)));
        }

        case reflection::PropType::ArrayProperty: {
            ue::FProperty* inner_prop = ue::read_field<ue::FProperty*>(fi.raw, ue::fprop::ARRAY_INNER);
            lua_tarray::LuaTArray arr;
            arr.array_ptr = const_cast<uint8_t*>(ptr);
            arr.inner_prop = inner_prop;
            arr.element_size = inner_prop ? ue::fprop_get_element_size(inner_prop) : 0;
            return sol::make_object(lua, arr);
        }

        case reflection::PropType::EnumProperty: {
            if (fi.element_size == 1) return sol::make_object(lua, static_cast<int>(ptr[0]));
            if (fi.element_size == 2) return sol::make_object(lua, static_cast<int>(*reinterpret_cast<const int16_t*>(ptr)));
            if (fi.element_size == 4) return sol::make_object(lua, *reinterpret_cast<const int32_t*>(ptr));
            return sol::make_object(lua, static_cast<int>(ptr[0]));
        }

        default:
            return sol::make_object(lua, sol::lightuserdata_value(const_cast<uint8_t*>(ptr)));
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Field write helper — writes a single field in struct memory
// ═══════════════════════════════════════════════════════════════════════

static bool write_field_value(uint8_t* base, const reflection::PropertyInfo& fi,
                               const sol::object& value) {
    uint8_t* ptr = base + fi.offset;
    if (!ue::is_mapped_ptr(ptr)) return false;

    switch (fi.type) {
        case reflection::PropType::BoolProperty: {
            bool val = value.as<bool>();
            if (fi.bool_byte_mask) {
                if (val) ptr[fi.bool_byte_offset] |= fi.bool_byte_mask;
                else ptr[fi.bool_byte_offset] &= ~fi.bool_byte_mask;
            } else {
                *reinterpret_cast<bool*>(ptr) = val;
            }
            return true;
        }

        case reflection::PropType::ByteProperty:
        case reflection::PropType::Int8Property:
            ptr[0] = static_cast<uint8_t>(value.as<int>());
            return true;

        case reflection::PropType::Int16Property:
        case reflection::PropType::UInt16Property:
            *reinterpret_cast<int16_t*>(ptr) = static_cast<int16_t>(value.as<int>());
            return true;

        case reflection::PropType::IntProperty:
            *reinterpret_cast<int32_t*>(ptr) = value.as<int32_t>();
            return true;

        case reflection::PropType::UInt32Property:
            *reinterpret_cast<uint32_t*>(ptr) = static_cast<uint32_t>(value.as<double>());
            return true;

        case reflection::PropType::Int64Property:
            *reinterpret_cast<int64_t*>(ptr) = static_cast<int64_t>(value.as<double>());
            return true;

        case reflection::PropType::UInt64Property:
            *reinterpret_cast<uint64_t*>(ptr) = static_cast<uint64_t>(value.as<double>());
            return true;

        case reflection::PropType::FloatProperty:
            *reinterpret_cast<float*>(ptr) = value.as<float>();
            return true;

        case reflection::PropType::DoubleProperty:
            *reinterpret_cast<double*>(ptr) = value.as<double>();
            return true;

        case reflection::PropType::ObjectProperty:
        case reflection::PropType::WeakObjectProperty:
        case reflection::PropType::ClassProperty: {
            if (value.is<lua_uobject::LuaUObject>()) {
                *reinterpret_cast<ue::UObject**>(ptr) = value.as<lua_uobject::LuaUObject&>().ptr;
                return true;
            } else if (value == sol::nil) {
                *reinterpret_cast<ue::UObject**>(ptr) = nullptr;
                return true;
            }
            return false;
        }

        case reflection::PropType::StructProperty: {
            // Accept LuaUStruct (memcpy) or table (fill fields)
            if (value.is<LuaUStruct>()) {
                const LuaUStruct& src = value.as<const LuaUStruct&>();
                if (src.data && src.size > 0) {
                    int32_t copy_size = (src.size < fi.element_size) ? src.size : fi.element_size;
                    std::memcpy(ptr, src.data, copy_size);
                }
                return true;
            } else if (value.get_type() == sol::type::table) {
                ue::UStruct* inner = ue::read_field<ue::UStruct*>(fi.raw, ue::fprop::STRUCT_INNER_STRUCT);
                if (inner) {
                    fill_from_table(ptr, inner, value.as<sol::table>());
                    return true;
                }
            }
            return false;
        }

        case reflection::PropType::EnumProperty: {
            int val = value.as<int>();
            if (fi.element_size == 1) ptr[0] = static_cast<uint8_t>(val);
            else if (fi.element_size == 2) *reinterpret_cast<int16_t*>(ptr) = static_cast<int16_t>(val);
            else *reinterpret_cast<int32_t*>(ptr) = val;
            return true;
        }

        default:
            logger::log_warn("USTRUCT", "Cannot write field '%s' — unsupported type %d",
                           fi.name.c_str(), static_cast<int>(fi.type));
            return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Property lookup cache — walk_properties is expensive, cache results
// ═══════════════════════════════════════════════════════════════════════

static std::unordered_map<ue::UStruct*, std::vector<reflection::PropertyInfo>> s_prop_cache;

static const std::vector<reflection::PropertyInfo>& get_cached_props(ue::UStruct* ustruct) {
    auto it = s_prop_cache.find(ustruct);
    if (it != s_prop_cache.end()) return it->second;
    auto props = reflection::walk_properties(ustruct, true);
    auto [inserted, _] = s_prop_cache.emplace(ustruct, std::move(props));
    return inserted->second;
}

static const reflection::PropertyInfo* find_field(ue::UStruct* ustruct, const std::string& name) {
    const auto& props = get_cached_props(ustruct);
    for (const auto& p : props) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// Registration — UStruct usertype for sol2
// ═══════════════════════════════════════════════════════════════════════

void register_all(sol::state& lua) {
    lua.new_usertype<LuaUStruct>("UStruct",
        sol::no_constructor,

        "IsValid", [](const LuaUStruct& self) -> bool {
            return self.is_valid();
        },

        "GetTypeName", [](const LuaUStruct& self) -> std::string {
            if (!self.ustruct) return "UStruct(unknown)";
            return reflection::get_short_name(reinterpret_cast<const ue::UObject*>(self.ustruct));
        },

        "GetSize", [](const LuaUStruct& self) -> int32_t {
            return self.size;
        },

        "IsOwning", [](const LuaUStruct& self) -> bool {
            return self.owns_data;
        },

        // Clone — create an owning copy of this struct
        "Clone", [](const LuaUStruct& self) -> LuaUStruct {
            if (!self.data || self.size <= 0) return LuaUStruct{};
            return lua_ustruct::copy(self.data, self.ustruct, self.size);
        },

        // CopyFrom — copy data from another struct or table
        "CopyFrom", [](LuaUStruct& self, sol::object source) -> bool {
            if (!self.data || self.size <= 0) return false;
            if (source.is<LuaUStruct>()) {
                const LuaUStruct& src = source.as<const LuaUStruct&>();
                if (src.data && src.size > 0) {
                    int32_t copy_size = (src.size < self.size) ? src.size : self.size;
                    std::memcpy(self.data, src.data, copy_size);
                    return true;
                }
            } else if (source.get_type() == sol::type::table) {
                if (self.ustruct) {
                    fill_from_table(self.data, self.ustruct, source.as<sol::table>());
                    return true;
                }
            }
            return false;
        },

        // GetFields — return a table of {name=type_string} for all fields
        "GetFields", [](sol::this_state ts, const LuaUStruct& self) -> sol::object {
            sol::state_view lua(ts);
            if (!self.ustruct) return sol::nil;
            sol::table result = lua.create_table();
            const auto& props = get_cached_props(self.ustruct);
            for (const auto& p : props) {
                result[p.name] = p.inner_type_name.empty() ? "unknown" : p.inner_type_name;
            }
            return result;
        },

        // ── __index for dynamic field access: struct.X, struct.Y, etc. ──
        sol::meta_function::index, [](sol::this_state ts, LuaUStruct& self,
                                       const std::string& key) -> sol::object {
            sol::state_view lua(ts);
            if (!self.data || !self.ustruct) return sol::nil;

            const auto* fi = find_field(self.ustruct, key);
            if (!fi) {
                // Not a field — check if it's a known method
                // Sol2 checks usertype methods BEFORE __index, so we only get here
                // for keys that are NOT built-in methods (IsValid, GetTypeName, etc.)
                return sol::nil;
            }

            return read_field_value(lua, self.data, *fi);
        },

        // ── __newindex for dynamic field writing: struct.X = 100 ──
        sol::meta_function::new_index, [](LuaUStruct& self, const std::string& key,
                                           sol::object value) {
            if (!self.data || !self.ustruct) return;

            const auto* fi = find_field(self.ustruct, key);
            if (!fi) {
                logger::log_warn("USTRUCT", "Field '%s' not found on struct '%s'",
                               key.c_str(),
                               reflection::get_short_name(reinterpret_cast<const ue::UObject*>(self.ustruct)).c_str());
                return;
            }

            write_field_value(self.data, *fi, value);
        },

        // ── __tostring ──
        sol::meta_function::to_string, [](sol::this_state ts, const LuaUStruct& self) -> std::string {
            if (!self.ustruct || !self.data) return "UStruct(nil)";

            std::string type_name = reflection::get_short_name(
                reinterpret_cast<const ue::UObject*>(self.ustruct));

            // For common types, show field values inline
            const auto& props = get_cached_props(self.ustruct);
            if (props.empty()) {
                char buf[128];
                snprintf(buf, sizeof(buf), "UStruct(%s, %d bytes)", type_name.c_str(), self.size);
                return std::string(buf);
            }

            std::string fields;
            int count = 0;
            for (const auto& p : props) {
                if (count > 0) fields += ", ";
                if (count >= 6) { fields += "..."; break; }  // Limit output

                fields += p.name + "=";

                // Read the value and append a simple representation
                const uint8_t* ptr = self.data + p.offset;
                switch (p.type) {
                    case reflection::PropType::FloatProperty:
                        fields += std::to_string(*reinterpret_cast<const float*>(ptr));
                        break;
                    case reflection::PropType::DoubleProperty:
                        fields += std::to_string(*reinterpret_cast<const double*>(ptr));
                        break;
                    case reflection::PropType::IntProperty:
                        fields += std::to_string(*reinterpret_cast<const int32_t*>(ptr));
                        break;
                    case reflection::PropType::ByteProperty:
                        fields += std::to_string(static_cast<int>(ptr[0]));
                        break;
                    case reflection::PropType::BoolProperty:
                        if (p.bool_byte_mask)
                            fields += (ptr[p.bool_byte_offset] & p.bool_byte_mask) ? "true" : "false";
                        else
                            fields += *reinterpret_cast<const bool*>(ptr) ? "true" : "false";
                        break;
                    default:
                        fields += "?";
                        break;
                }
                count++;
            }

            return "UStruct(" + type_name + ": " + fields + ")";
        },

        // ── Equality — same data pointer AND same struct type ──
        sol::meta_function::equal_to, [](const LuaUStruct& a, const LuaUStruct& b) -> bool {
            return a.data == b.data && a.ustruct == b.ustruct;
        }
    );

    logger::log_info("LUA", "UStruct userdata type registered");
}

// ═══════════════════════════════════════════════════════════════════════
// Factory helpers
// ═══════════════════════════════════════════════════════════════════════

LuaUStruct wrap(uint8_t* data, ue::UStruct* ustruct, int32_t size) {
    LuaUStruct s;
    s.data = data;
    s.ustruct = ustruct;
    s.size = size;
    s.owns_data = false;
    return s;
}

LuaUStruct copy(const uint8_t* data, ue::UStruct* ustruct, int32_t size) {
    LuaUStruct s;
    if (data && size > 0) {
        s.data = new uint8_t[size];
        std::memcpy(s.data, data, size);
    }
    s.ustruct = ustruct;
    s.size = size;
    s.owns_data = true;
    return s;
}

LuaUStruct from_table(sol::state_view lua, const sol::table& tbl,
                       ue::UStruct* ustruct, int32_t size) {
    LuaUStruct s;
    if (size > 0) {
        s.data = new uint8_t[size];
        std::memset(s.data, 0, size);
    }
    s.ustruct = ustruct;
    s.size = size;
    s.owns_data = true;

    if (ustruct) {
        fill_from_table(s.data, ustruct, tbl);
    }

    return s;
}

void fill_from_table(uint8_t* data, ue::UStruct* ustruct, const sol::table& tbl) {
    if (!data || !ustruct) return;

    const auto& props = get_cached_props(ustruct);

    for (const auto& p : props) {
        sol::optional<sol::object> val = tbl[p.name];
        if (val && val->valid() && val->get_type() != sol::type::lua_nil) {
            write_field_value(data, p, *val);
        }
    }
}

} // namespace lua_ustruct
