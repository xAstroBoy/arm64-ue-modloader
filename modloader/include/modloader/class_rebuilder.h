#pragma once
// modloader/include/modloader/class_rebuilder.h
// Live class rebuild system — navigate Class → Instance → Property/Function
// Supports property hooks, function hooks, instance tracking, introspection
// All offsets sourced from live UProperty->Offset_Internal — never hardcoded

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>
#include <cstdint>
#include "modloader/types.h"
#include "modloader/reflection_walker.h"

namespace rebuilder {

// Forward declarations
struct RebuiltClass;
struct RebuiltInstance;

// Property hook callback: (UObject* obj, property_name, old_value_ptr, new_value_ptr) -> bool
// Return true to allow the write, false to block it
using PropHookCallback = std::function<bool(ue::UObject*, const std::string&, void*, void*)>;

// Function hook callbacks
using FuncPreCallback  = std::function<bool(ue::UObject*, ue::UFunction*, void*)>;  // return true = BLOCK
using FuncPostCallback = std::function<void(ue::UObject*, ue::UFunction*, void*)>;

using HookId = uint64_t;

// ═══ RebuiltProperty ════════════════════════════════════════════════════
struct RebuiltProperty {
    std::string name;
    reflection::PropType type;
    int32_t     offset;
    int32_t     element_size;
    uint64_t    flags;
    std::string type_name;  // human readable type string

    // Bool specifics
    uint8_t     bool_byte_mask;
    uint8_t     bool_field_mask;
    uint8_t     bool_byte_offset_extra;

    ue::FProperty* raw;

    // Read this property's value from a live UObject instance
    // Buffer must be at least element_size bytes
    void read(const ue::UObject* obj, void* out_buf) const;

    // Write this property's value to a live UObject instance
    void write(ue::UObject* obj, const void* in_buf) const;

    // Read a bool property value
    bool read_bool(const ue::UObject* obj) const;

    // Write a bool property value
    void write_bool(ue::UObject* obj, bool value) const;
};

// ═══ RebuiltFunction ════════════════════════════════════════════════════
struct RebuiltFunction {
    std::string name;
    uint32_t    flags;       // EFunctionFlags
    uint16_t    parms_size;
    uint16_t    return_offset;
    uint8_t     num_parms;
    ue::UFunction* raw;

    std::vector<reflection::PropertyInfo> params;
    reflection::PropertyInfo* return_prop; // nullable
};

// ═══ RebuiltClass ═══════════════════════════════════════════════════════
struct RebuiltClass {
    std::string name;
    std::string parent_name;
    int32_t     properties_size;
    ue::UClass* raw;

    // Own properties and functions (NOT inherited)
    std::vector<RebuiltProperty> properties;
    std::vector<RebuiltFunction> functions;

    // Inherited properties and functions (from SuperStruct chain)
    std::vector<RebuiltProperty> all_properties;  // own + inherited
    std::vector<RebuiltFunction> all_functions;    // own + inherited

    // Fast name→entry lookup maps
    std::unordered_map<std::string, RebuiltProperty*> prop_map;
    std::unordered_map<std::string, RebuiltFunction*> func_map;

    // Live instance tracking
    std::unordered_set<ue::UObject*> live_instances;
    std::mutex instances_mutex;

    // Property hooks (per-class — fire for ALL instances)
    struct PropHookEntry {
        HookId id;
        std::string prop_name;
        PropHookCallback callback;
    };
    std::vector<PropHookEntry> prop_hooks;

    // Function hooks (per-class)
    struct FuncHookEntry {
        HookId id;
        std::string func_name;
        FuncPreCallback pre;
        FuncPostCallback post;
    };
    std::vector<FuncHookEntry> func_hooks;

    // Lookup helpers
    RebuiltProperty* find_property(const std::string& name);
    RebuiltFunction* find_function(const std::string& name);
    bool has_property(const std::string& name) const;
    bool has_function(const std::string& name) const;

    // Instance management
    void add_instance(ue::UObject* obj);
    void remove_instance(ue::UObject* obj);
    ue::UObject* get_first_instance();
    ue::UObject* get_instance(int index);
    std::vector<ue::UObject*> get_all_instances();
    int instance_count();
};

// ═══ Main rebuilder API ═════════════════════════════════════════════════

// Initialize the rebuilder subsystem
void init();

// Build a live view of a UClass — walks all properties and functions
// Sources offsets dynamically from live UProperty->Offset_Internal
// Caches result — subsequent calls return the cached view
RebuiltClass* rebuild(const std::string& class_name);

// Rebuild from a raw UClass pointer
RebuiltClass* rebuild(ue::UClass* cls);

// Get a previously rebuilt class (returns nullptr if not rebuilt yet)
RebuiltClass* get(const std::string& class_name);

// Install a property write hook on a class (fires for ALL instances)
HookId hook_property(const std::string& class_name, const std::string& prop_name,
                     PropHookCallback callback);

// Install a property write hook on a SPECIFIC instance only
HookId hook_property_instance(ue::UObject* obj, const std::string& prop_name,
                              PropHookCallback callback);

// Install a function hook on a class (fires for ALL instances)
HookId hook_function(const std::string& class_name, const std::string& func_name,
                     FuncPreCallback pre, FuncPostCallback post);

// Install a function hook on a SPECIFIC instance only
HookId hook_function_instance(ue::UObject* obj, const std::string& func_name,
                              FuncPreCallback pre, FuncPostCallback post);

// Remove a hook by ID
void unhook(HookId id);

// Tick function — called from ProcessEvent hook to dispatch property dirty checks
void tick(ue::UObject* self, ue::UFunction* func, void* parms);

// Get the CDO (Class Default Object) for a class
ue::UObject* get_cdo(const std::string& class_name);

// Check if an object is a valid live instance (not CDO, not destroyed)
bool is_valid_instance(ue::UObject* obj);

// Get all rebuilt classes
const std::unordered_map<std::string, RebuiltClass>& get_all();

} // namespace rebuilder
