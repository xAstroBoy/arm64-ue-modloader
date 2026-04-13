#pragma once
// modloader/include/modloader/pattern_scanner.h
// AOB/pattern scanner for engine library mapped memory.
// Supports any engine library (libUE4.so, libUnreal.so, etc.) via game profile.
// Pattern format: "48 8B 05 ?? ?? ?? ?? 48 85 C0" where ?? = wildcard
//
// Also provides string scanning for engine version detection.
// Scans .rodata/.data segments for known version markers.

#include <string>
#include <cstdint>
#include <vector>

namespace pattern
{

    // Initialize — finds engine library memory region via /proc/self/maps.
    // Uses game_profile::engine_lib_name() to determine which library to scan.
    void init();

    // Initialize with an explicit library name (for testing or override)
    void init(const std::string &lib_name);

    // Scan for a byte pattern with wildcards in libUE4.so mapped memory
    // Pattern format: hex bytes separated by spaces, ?? for wildcard
    // Returns first match address, or nullptr if not found
    void *scan(const std::string &pattern_str);

    // Scan and resolve a PC-relative offset (ARM64 ADRP+ADD or LDR)
    // rip_offset = offset into the instruction where the displacement starts
    // instr_size = total size of the instruction(s) being decoded
    void *scan_rip(const std::string &pattern_str, int rip_offset, int instr_size);

    // Scan all matches (returns vector of all addresses that match)
    std::vector<void *> scan_all(const std::string &pattern_str);

    // ═══ String scanning for version detection ═════════════════════════════
    // Search for a null-terminated string in all readable (non-executable) regions.
    // Returns the address of the first match, or nullptr if not found.
    void *find_string(const char *needle);

    // Search for a string and return all occurrences.
    std::vector<void *> find_string_all(const char *needle);

    // Get the start and end of libUE4.so executable segment
    uintptr_t text_start();
    uintptr_t text_end();
    uintptr_t data_start();
    uintptr_t data_end();

} // namespace pattern
