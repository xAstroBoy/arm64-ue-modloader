#pragma once
// modloader/include/modloader/notification.h
// Android notification system via JNI
// Works on Quest without root

#include <string>
#include <cstdint>

namespace notification {

// Reserved notification IDs
constexpr int NOTIF_ID_BOOT       = 0;
constexpr int NOTIF_ID_SDK        = 1;
constexpr int NOTIF_ID_PAK        = 2;
constexpr int NOTIF_ID_RELOAD     = 3;
constexpr int NOTIF_ID_CRASH      = 99;
constexpr int NOTIF_ID_MOD_BASE   = 100;

// Initialize JNI context — find JavaVM, create notification channel
bool init();

// Post a notification to the Quest notification shade
void post(const std::string& title, const std::string& body, int id = -1);

// Post the boot notification (ID 0)
void post_boot(int mods_loaded, int mods_failed);

// Post the SDK dump complete notification (ID 1)
void post_sdk(int class_count);

// Post PAK mount notification (ID 2)
void post_pak(const std::string& pak_name);

// Post mod reload notification (ID 3)
void post_reload(const std::string& mod_name);

// Post crash notification (ID 99)
void post_crash();

} // namespace notification
