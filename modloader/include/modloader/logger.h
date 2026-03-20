#pragma once
// modloader/include/modloader/logger.h
// Dual logging: UEModLoader.log (overwrite per session) + logcat
// Thread-safe, log truncated on each boot for clean session output

#include <string>
#include <vector>
#include <cstdint>

namespace logger {

// Must be called once during init — opens log file in write mode (truncates), writes session header
void init(const std::string& log_path);

// Shutdown — flushes and closes file
void shutdown();

// Core logging functions. tag is padded to 8 chars.
// Source is padded to 8 chars (e.g. "BOOT    ", "SYMBOL  ", "LUA     ")
void log_info (const char* source, const char* fmt, ...);
void log_warn (const char* source, const char* fmt, ...);
void log_error(const char* source, const char* fmt, ...);
void log_debug(const char* source, const char* fmt, ...);

// Raw write — no timestamp, no formatting
void log_raw(const char* text);

// Get the last N lines of the log (for ADB bridge log_tail command)
std::string get_tail(int lines);

// Add a stream socket fd for live log streaming
void add_stream_socket(int fd);

// Remove a stream socket
void remove_stream_socket(int fd);

// Get total error count since session start
int get_error_count();

// Get total log lines since session start
int get_line_count();

// Get the log file path
const std::string& get_log_path();

} // namespace logger
