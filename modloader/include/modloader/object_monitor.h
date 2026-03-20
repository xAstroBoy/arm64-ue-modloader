#pragma once
// modloader/include/modloader/object_monitor.h
// Background thread that monitors GUObjectArray for new objects
// When the object count grows significantly (level load, streaming), 
// automatically re-walks reflection and regenerates the SDK.
// Zero performance impact on the game — runs on its own thread with sleep intervals.

#include <cstdint>

namespace object_monitor {

// Start the background monitor thread
// poll_interval_ms: how often to check GUObjectArray count (default 5000ms = 5s)
// growth_threshold: minimum new objects to trigger a re-dump (default 500)
// cooldown_ms: minimum time between re-dumps (default 30000ms = 30s)
void start(int poll_interval_ms = 5000, int growth_threshold = 500, int cooldown_ms = 30000);

// Stop the monitor thread
void stop();

// Force a re-dump now (called from ADB bridge or Lua)
void force_redump();

// Get the number of auto-dumps triggered since boot
int auto_dump_count();

// Get the current live GUObjectArray object count (reads memory directly, no walk)
int32_t get_live_count();

} // namespace object_monitor
