// modloader/src/core/object_monitor.cpp
// Background thread that watches GUObjectArray for significant growth.
// When new objects appear (level load, asset streaming, Blueprint compilation),
// automatically re-walks reflection and regenerates the SDK so mods always
// have access to the latest classes.
//
// Zero game-thread impact: runs entirely on its own thread with sleep intervals.
// The re-walk itself locks briefly during snapshot but does not block ProcessEvent.

#include "modloader/object_monitor.h"
#include "modloader/reflection_walker.h"
#include "modloader/lua_dump_generator.h"
#include "modloader/logger.h"
#include "modloader/notification.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>

namespace object_monitor {

static std::thread s_thread;
static std::atomic<bool> s_running{false};
static std::atomic<int> s_auto_dump_count{0};
static std::atomic<bool> s_force_redump{false};

static int s_poll_interval_ms = 5000;
static int s_growth_threshold = 500;
static int s_cooldown_ms = 30000;

// Last known count from GUObjectArray (from the last walk, not live)
static int32_t s_last_known_count = 0;
static std::mutex s_monitor_mutex;

static void monitor_loop() {
    logger::log_info("MONITOR", "Object monitor started — poll=%dms, threshold=%d, cooldown=%dms",
                     s_poll_interval_ms, s_growth_threshold, s_cooldown_ms);

    // Initialize with the count from the initial boot walk
    s_last_known_count = reflection::get_object_count();
    logger::log_info("MONITOR", "Baseline object count: %d", s_last_known_count);

    auto last_dump_time = std::chrono::steady_clock::now();

    while (s_running.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(s_poll_interval_ms));

        if (!s_running.load(std::memory_order_relaxed)) break;

        // Check for forced re-dump request (from ADB bridge or Lua)
        bool forced = s_force_redump.exchange(false, std::memory_order_relaxed);

        // Read the LIVE object count from GUObjectArray memory
        int32_t live_count = reflection::get_live_object_count();

        if (live_count <= 0) continue; // GUObjectArray not valid yet

        int32_t growth = live_count - s_last_known_count;

        // Check cooldown
        auto now = std::chrono::steady_clock::now();
        auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_dump_time).count();
        bool cooldown_ok = since_last >= s_cooldown_ms;

        if (forced) {
            logger::log_info("MONITOR", "Forced re-dump requested — live count: %d (was %d, +%d)",
                             live_count, s_last_known_count, growth);
        } else if (growth >= s_growth_threshold && cooldown_ok) {
            logger::log_info("MONITOR", "Object growth detected: %d -> %d (+%d, threshold=%d) — triggering auto re-dump",
                             s_last_known_count, live_count, growth, s_growth_threshold);
        } else {
            // No significant growth and no forced dump — skip
            // Periodic debug log every 60 polls (5 minutes at 5s interval)
            static int poll_count = 0;
            poll_count++;
            if (poll_count % 60 == 0) {
                logger::log_info("MONITOR", "Heartbeat — live objects: %d, last walk: %d, growth: %d",
                                 live_count, s_last_known_count, growth);
            }
            continue;
        }

        // Enforce cooldown even for forced dumps (prevents spam, but allow first forced dump)
        if (!forced && !cooldown_ok) {
            logger::log_info("MONITOR", "Growth detected (+%d) but cooldown active (%lldms/%dms) — skipping",
                             growth, (long long)since_last, s_cooldown_ms);
            continue;
        }

        // ── Perform the re-dump ─────────────────────────────────────────
        {
            std::lock_guard<std::mutex> lock(s_monitor_mutex);

            auto dump_start = std::chrono::steady_clock::now();
            logger::log_info("MONITOR", "Re-walking GUObjectArray and regenerating SDK...");

            int files = sdk_gen::regenerate();

            auto dump_end = std::chrono::steady_clock::now();
            auto dump_ms = std::chrono::duration_cast<std::chrono::milliseconds>(dump_end - dump_start).count();

            int32_t new_count = reflection::get_object_count();
            int new_classes = sdk_gen::class_count();
            int new_structs = sdk_gen::struct_count();
            int new_enums = sdk_gen::enum_count();

            int32_t old_count = s_last_known_count;
            s_last_known_count = new_count;
            last_dump_time = dump_end;
            s_auto_dump_count.fetch_add(1, std::memory_order_relaxed);

            logger::log_info("MONITOR", "Re-dump #%d complete in %lldms — objects: %d->%d (+%d), "
                             "SDK: %d classes, %d structs, %d enums, %d files",
                             s_auto_dump_count.load(), (long long)dump_ms,
                             old_count, new_count, new_count - old_count,
                             new_classes, new_structs, new_enums, files);

            // Post a notification so the user knows the SDK was updated
            char body[256];
            snprintf(body, sizeof(body), "SDK updated: %d classes (+%d objects)",
                     new_classes, new_count - old_count);
            notification::post("UE ModLoader", body, 1);
        }
    }

    logger::log_info("MONITOR", "Object monitor stopped");
}

void start(int poll_interval_ms, int growth_threshold, int cooldown_ms) {
    if (s_running.load()) {
        logger::log_warn("MONITOR", "Monitor already running");
        return;
    }

    s_poll_interval_ms = poll_interval_ms;
    s_growth_threshold = growth_threshold;
    s_cooldown_ms = cooldown_ms;
    s_running.store(true, std::memory_order_relaxed);

    s_thread = std::thread(monitor_loop);
    s_thread.detach();
}

void stop() {
    s_running.store(false, std::memory_order_relaxed);
    logger::log_info("MONITOR", "Object monitor stop requested");
}

void force_redump() {
    s_force_redump.store(true, std::memory_order_relaxed);
    logger::log_info("MONITOR", "Force re-dump queued — will trigger on next poll cycle");
}

int auto_dump_count() {
    return s_auto_dump_count.load(std::memory_order_relaxed);
}

int32_t get_live_count() {
    return reflection::get_live_object_count();
}

} // namespace object_monitor
