// modloader/src/util/notification.cpp
// Android JNI notification system — posts Quest system notifications
// No root required — works via JNI on the game's existing JavaVM

#include "modloader/notification.h"
#include "modloader/logger.h"

#include <jni.h>
#include <dlfcn.h>
#include <atomic>
#include <mutex>
#include <cstring>

namespace notification {

static JavaVM* s_jvm = nullptr;
static jobject s_context = nullptr;
static bool s_channel_created = false;
static std::mutex s_jni_mutex;
static std::atomic<int> s_auto_id{100};

static const char* CHANNEL_ID = "uemodloader";
static const char* CHANNEL_NAME = "UE ModLoader";

// ═══ Obtain JNI env for the current thread ══════════════════════════════
static JNIEnv* get_env() {
    if (!s_jvm) return nullptr;
    JNIEnv* env = nullptr;
    int status = s_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        if (s_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return nullptr;
        }
    }
    return env;
}

// ═══ Create notification channel (Android 8+) ═══════════════════════════
static bool create_channel_internal(JNIEnv* env, jobject context) {
    if (s_channel_created) return true;

    // NotificationChannel(String id, CharSequence name, int importance)
    jclass channel_cls = env->FindClass("android/app/NotificationChannel");
    if (!channel_cls) {
        logger::log_warn("NOTIFY", "NotificationChannel class not found (pre-Oreo?)");
        s_channel_created = true; // On pre-Oreo, channels aren't needed
        return true;
    }

    jmethodID channel_ctor = env->GetMethodID(channel_cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/CharSequence;I)V");
    if (!channel_ctor) {
        logger::log_error("NOTIFY", "NotificationChannel constructor not found");
        return false;
    }

    jstring j_channel_id = env->NewStringUTF(CHANNEL_ID);
    jstring j_channel_name = env->NewStringUTF(CHANNEL_NAME);

    // IMPORTANCE_DEFAULT = 3
    jobject channel = env->NewObject(channel_cls, channel_ctor, j_channel_id, j_channel_name, 3);

    // Set no sound, no vibration
    jmethodID set_sound = env->GetMethodID(channel_cls, "setSound",
        "(Landroid/net/Uri;Landroid/media/AudioAttributes;)V");
    if (set_sound) {
        env->CallVoidMethod(channel, set_sound, nullptr, nullptr);
    }

    jmethodID enable_vibration = env->GetMethodID(channel_cls, "enableVibration", "(Z)V");
    if (enable_vibration) {
        env->CallVoidMethod(channel, enable_vibration, JNI_FALSE);
    }

    // Get NotificationManager
    jclass context_cls = env->GetObjectClass(context);
    jmethodID get_system_service = env->GetMethodID(context_cls, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring j_ns = env->NewStringUTF("notification");
    jobject nm = env->CallObjectMethod(context, get_system_service, j_ns);

    if (!nm) {
        logger::log_error("NOTIFY", "Failed to get NotificationManager");
        env->DeleteLocalRef(j_channel_id);
        env->DeleteLocalRef(j_channel_name);
        env->DeleteLocalRef(j_ns);
        return false;
    }

    // Create the channel
    jclass nm_cls = env->GetObjectClass(nm);
    jmethodID create_channel_method = env->GetMethodID(nm_cls, "createNotificationChannel",
        "(Landroid/app/NotificationChannel;)V");
    if (create_channel_method) {
        env->CallVoidMethod(nm, create_channel_method, channel);
    }

    // Clean up local refs
    env->DeleteLocalRef(j_channel_id);
    env->DeleteLocalRef(j_channel_name);
    env->DeleteLocalRef(j_ns);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logger::log_error("NOTIFY", "Exception creating notification channel");
        return false;
    }

    s_channel_created = true;
    logger::log_info("NOTIFY", "Notification channel '%s' created", CHANNEL_ID);
    return true;
}

// Declared in main.cpp — stored from JNI_OnLoad
extern "C" JavaVM* get_stored_jvm();

// ═══ Initialize — find JVM and app context ══════════════════════════════
bool init() {
    std::lock_guard<std::mutex> lock(s_jni_mutex);

    // Get the existing JVM — stored from JNI_OnLoad in main.cpp
    s_jvm = get_stored_jvm();
    if (!s_jvm) {
        logger::log_error("NOTIFY", "JavaVM not available — get_stored_jvm() returned null");
        return false;
    }
    logger::log_info("NOTIFY", "JavaVM obtained from JNI_OnLoad storage");

    JNIEnv* env = get_env();
    if (!env) {
        logger::log_error("NOTIFY", "Failed to get JNIEnv");
        return false;
    }

    // Get the application context via ActivityThread.currentApplication()
    jclass activity_thread_cls = env->FindClass("android/app/ActivityThread");
    if (!activity_thread_cls) {
        logger::log_error("NOTIFY", "ActivityThread class not found");
        return false;
    }

    jmethodID current_app = env->GetStaticMethodID(activity_thread_cls,
        "currentApplication", "()Landroid/app/Application;");
    if (!current_app) {
        logger::log_error("NOTIFY", "currentApplication method not found");
        return false;
    }

    jobject app = env->CallStaticObjectMethod(activity_thread_cls, current_app);
    if (!app) {
        logger::log_error("NOTIFY", "currentApplication() returned null");
        return false;
    }

    // Get the application context
    jclass context_cls = env->GetObjectClass(app);
    jmethodID get_app_ctx = env->GetMethodID(context_cls, "getApplicationContext",
        "()Landroid/content/Context;");
    jobject app_ctx = env->CallObjectMethod(app, get_app_ctx);

    if (!app_ctx) {
        // Use the Application object itself as context
        app_ctx = app;
    }

    s_context = env->NewGlobalRef(app_ctx);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logger::log_error("NOTIFY", "Exception during context acquisition");
        return false;
    }

    // Create the notification channel
    create_channel_internal(env, s_context);

    logger::log_info("NOTIFY", "JNI notification system initialized");
    return true;
}

// ═══ Post a notification ════════════════════════════════════════════════
void post(const std::string& title, const std::string& body, int notification_id) {
    std::lock_guard<std::mutex> lock(s_jni_mutex);

    logger::log_info("NOTIFY", "[%d] %s: %s", notification_id, title.c_str(), body.c_str());

    if (!s_jvm || !s_context) {
        logger::log_warn("NOTIFY", "JNI not initialized — notification logged only");
        return;
    }

    JNIEnv* env = get_env();
    if (!env) {
        logger::log_warn("NOTIFY", "Failed to get JNIEnv for notification");
        return;
    }

    // Build Notification.Builder
    jclass builder_cls = env->FindClass("android/app/Notification$Builder");
    if (!builder_cls) {
        logger::log_warn("NOTIFY", "Notification.Builder class not found");
        return;
    }

    // Constructor: Builder(Context, String channelId)
    jmethodID builder_ctor = env->GetMethodID(builder_cls, "<init>",
        "(Landroid/content/Context;Ljava/lang/String;)V");
    if (!builder_ctor) {
        // Try the old constructor without channel (pre-Oreo)
        builder_ctor = env->GetMethodID(builder_cls, "<init>",
            "(Landroid/content/Context;)V");
        if (!builder_ctor) {
            logger::log_warn("NOTIFY", "Notification.Builder constructor not found");
            return;
        }
        jobject builder = env->NewObject(builder_cls, builder_ctor, s_context);
        if (!builder) return;

        // setContentTitle
        jmethodID set_title = env->GetMethodID(builder_cls, "setContentTitle",
            "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;");
        jstring j_title = env->NewStringUTF(title.c_str());
        env->CallObjectMethod(builder, set_title, j_title);

        // setContentText
        jmethodID set_text = env->GetMethodID(builder_cls, "setContentText",
            "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;");
        jstring j_body = env->NewStringUTF(body.c_str());
        env->CallObjectMethod(builder, set_text, j_body);

        // setSmallIcon — use the app's icon
        jmethodID set_icon = env->GetMethodID(builder_cls, "setSmallIcon",
            "(I)Landroid/app/Notification$Builder;");
        // android.R.drawable.ic_dialog_info = 0x01080077
        env->CallObjectMethod(builder, set_icon, 0x01080077);

        // setAutoCancel
        jmethodID set_auto_cancel = env->GetMethodID(builder_cls, "setAutoCancel",
            "(Z)Landroid/app/Notification$Builder;");
        env->CallObjectMethod(builder, set_auto_cancel, JNI_TRUE);

        // Build
        jmethodID build = env->GetMethodID(builder_cls, "build", "()Landroid/app/Notification;");
        jobject notification = env->CallObjectMethod(builder, build);

        // Notify
        jclass context_cls = env->GetObjectClass(s_context);
        jmethodID get_system_service = env->GetMethodID(context_cls, "getSystemService",
            "(Ljava/lang/String;)Ljava/lang/Object;");
        jstring j_ns = env->NewStringUTF("notification");
        jobject nm = env->CallObjectMethod(s_context, get_system_service, j_ns);

        if (nm) {
            jclass nm_cls = env->GetObjectClass(nm);
            jmethodID notify_method = env->GetMethodID(nm_cls, "notify",
                "(ILandroid/app/Notification;)V");
            env->CallVoidMethod(nm, notify_method, notification_id, notification);
        }

        env->DeleteLocalRef(j_title);
        env->DeleteLocalRef(j_body);
        env->DeleteLocalRef(j_ns);

        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            logger::log_warn("NOTIFY", "Exception posting notification (pre-Oreo path)");
        }
        return;
    }

    // Oreo+ path with channel
    jstring j_channel = env->NewStringUTF(CHANNEL_ID);
    jobject builder = env->NewObject(builder_cls, builder_ctor, s_context, j_channel);
    if (!builder) {
        env->DeleteLocalRef(j_channel);
        return;
    }

    // setContentTitle
    jmethodID set_title = env->GetMethodID(builder_cls, "setContentTitle",
        "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;");
    jstring j_title = env->NewStringUTF(title.c_str());
    env->CallObjectMethod(builder, set_title, j_title);

    // setContentText
    jmethodID set_text = env->GetMethodID(builder_cls, "setContentText",
        "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;");
    jstring j_body = env->NewStringUTF(body.c_str());
    env->CallObjectMethod(builder, set_text, j_body);

    // setSmallIcon — android.R.drawable.ic_dialog_info
    jmethodID set_icon = env->GetMethodID(builder_cls, "setSmallIcon",
        "(I)Landroid/app/Notification$Builder;");
    env->CallObjectMethod(builder, set_icon, 0x01080077);

    // setAutoCancel
    jmethodID set_auto_cancel = env->GetMethodID(builder_cls, "setAutoCancel",
        "(Z)Landroid/app/Notification$Builder;");
    env->CallObjectMethod(builder, set_auto_cancel, JNI_TRUE);

    // Build the notification
    jmethodID build = env->GetMethodID(builder_cls, "build", "()Landroid/app/Notification;");
    jobject notif = env->CallObjectMethod(builder, build);

    // Get NotificationManager and post
    jclass context_cls = env->GetObjectClass(s_context);
    jmethodID get_system_service = env->GetMethodID(context_cls, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring j_ns = env->NewStringUTF("notification");
    jobject nm = env->CallObjectMethod(s_context, get_system_service, j_ns);

    if (nm && notif) {
        jclass nm_cls = env->GetObjectClass(nm);
        jmethodID notify_method = env->GetMethodID(nm_cls, "notify",
            "(ILandroid/app/Notification;)V");
        env->CallVoidMethod(nm, notify_method, notification_id, notif);
    }

    // Clean up
    env->DeleteLocalRef(j_channel);
    env->DeleteLocalRef(j_title);
    env->DeleteLocalRef(j_body);
    env->DeleteLocalRef(j_ns);

    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logger::log_warn("NOTIFY", "Exception posting notification");
    }
}

// ═══ Convenience methods ════════════════════════════════════════════════
void post_boot(int mods_loaded, int mods_failed) {
    int total = mods_loaded + mods_failed;
    std::string body;
    if (mods_failed > 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Scripts %d/%d loaded, %d failed",
                 mods_loaded, total, mods_failed);
        body = buf;
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "Scripts %d/%d loaded", mods_loaded, total);
        body = buf;
    }
    post("UE ModLoader", body, NOTIF_ID_BOOT);
}

void post_sdk(int class_count) {
    char buf[128];
    snprintf(buf, sizeof(buf), "SDK ready — %d classes", class_count);
    post("UE ModLoader", std::string(buf), NOTIF_ID_SDK);
}

void post_pak(const std::string& pak_name) {
    post("UE ModLoader", "Mounted: " + pak_name, NOTIF_ID_PAK);
}

void post_reload(const std::string& mod_name) {
    post("UE ModLoader", "Reloaded: " + mod_name, NOTIF_ID_RELOAD);
}

void post_crash() {
    post("UE ModLoader \u26A0", "Crash — see modloader_crash.log", NOTIF_ID_CRASH);
}

} // namespace notification
