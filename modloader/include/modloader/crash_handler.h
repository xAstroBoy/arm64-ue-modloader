#pragma once
// modloader/include/modloader/crash_handler.h
// Signal handler for SIGSEGV, SIGABRT, SIGBUS, SIGFPE
// Writes modloader_crash.log with backtrace + last 500 log lines + fault address

namespace crash_handler {

// Install signal handlers (first time — saves old handlers)
void install();

// Re-install our signal handler on top of whatever replaced it.
// Call this after mods load, after deferred init, and periodically
// to prevent Oculus VR runtime / Frida from permanently replacing our handler.
void reinstall();

} // namespace crash_handler
