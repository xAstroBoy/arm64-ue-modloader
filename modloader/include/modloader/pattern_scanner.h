#pragma once
// modloader/include/modloader/pattern_scanner.h
// AOB/pattern scanner for libUE4.so mapped memory
// Pattern format: "48 8B 05 ?? ?? ?? ?? 48 85 C0" where ?? = wildcard

#include <string>
#include <cstdint>
#include <vector>

namespace pattern {

// Initialize — finds libUE4.so memory region via /proc/self/maps
void init();

// Scan for a byte pattern with wildcards in libUE4.so mapped memory
// Pattern format: hex bytes separated by spaces, ?? for wildcard
// Returns first match address, or nullptr if not found
void* scan(const std::string& pattern_str);

// Scan and resolve a PC-relative offset (ARM64 ADRP+ADD or LDR)
// rip_offset = offset into the instruction where the displacement starts
// instr_size = total size of the instruction(s) being decoded
void* scan_rip(const std::string& pattern_str, int rip_offset, int instr_size);

// Scan all matches (returns vector of all addresses that match)
std::vector<void*> scan_all(const std::string& pattern_str);

// Get the start and end of libUE4.so executable segment
uintptr_t text_start();
uintptr_t text_end();
uintptr_t data_start();
uintptr_t data_end();

} // namespace pattern
