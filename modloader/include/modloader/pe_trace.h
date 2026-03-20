#pragma once
// modloader/include/modloader/pe_trace.h
// ProcessEvent trace logger — toggle via ADB, anti-flood, writes to pe_trace.log
// Records unique OwnerClass:FuncName pairs with call counts for fast class discovery

#include <string>
#include "modloader/types.h"

namespace pe_trace {

// Start tracing PE calls. Optional filter = substring match on class or func name.
// If filter is empty, traces everything.
void start(const std::string& filter = "");

// Stop tracing (keeps accumulated data)
void stop();

// Clear all accumulated trace data
void clear();

// Record a PE call (called from hooked_process_event — cheap early-out if not active)
void record(ue::UObject* self, ue::UFunction* func);

// Check if tracing is active
bool is_active();

// Get status string (active/inactive, duration, unique count, total count)
std::string status();

// Get top N entries sorted by call count, formatted as text
std::string top(int n = 50);

// Dump all accumulated data to pe_trace.log on device, returns path + summary
std::string dump_to_file();

} // namespace pe_trace
