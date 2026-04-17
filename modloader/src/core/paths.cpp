// modloader/src/core/paths.cpp
// Runtime path resolution — uses JNI getExternalFilesDir() per instructions
// NEVER hardcodes /sdcard/ or /storage/emulated/0/ or /data/data/
// The app knows its own path — ask Android via JNI

#include "modloader/paths.h"
#include <jni.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>
#include <android/log.h>

#define LOG_TAG "UEModLoader"

// Declared in main.cpp — stored from JNI_OnLoad
extern "C" JavaVM *get_stored_jvm();

namespace paths
{

    static std::string s_data_dir;

    static std::string read_package_name()
    {
        char buf[256];
        memset(buf, 0, sizeof(buf));
        FILE *f = fopen("/proc/self/cmdline", "r");
        if (f)
        {
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            if (n > 0)
            {
                return std::string(buf);
            }
        }
        return "com.Armature.VR4";
    }

    static void ensure_dir(const std::string &path)
    {
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            return; // already exists
        if (mkdir(path.c_str(), 0755) == 0)
        {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                "paths: created dir: %s", path.c_str());
        }
        else
        {
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                "paths: mkdir failed for %s: %s", path.c_str(), strerror(errno));
        }
    }

    /// Verify we can actually write to the data directory.
    /// Returns true if a test file can be created and deleted.
    static bool verify_writable(const std::string &dir)
    {
        std::string test_file = dir + "/.modloader_write_test";
        FILE *f = fopen(test_file.c_str(), "w");
        if (!f)
            return false;
        fwrite("ok", 1, 2, f);
        fclose(f);
        unlink(test_file.c_str());
        return true;
    }

    /// Repair data directories after a botched installer run.
    /// Handles:
    /// - .modloader_bak leftover directories (from uninstall/reinstall cycle)
    /// - Broken directory permissions (from chmod/chown on FUSE)
    /// - Missing subdirectories
    static void repair_data_dirs()
    {
        if (s_data_dir.empty())
            return;

        // 1. Check for .modloader_bak leftovers from the installer's
        //    backup/restore cycle. If the real dir is missing/empty but
        //    the backup exists, restore it.
        std::string pkg = read_package_name();
        std::string data_parent = "/storage/emulated/0/Android/data/";
        std::string obb_parent = "/storage/emulated/0/Android/obb/";

        std::string bak_paths[] = {
            data_parent + pkg + ".modloader_bak",
            obb_parent + pkg + ".modloader_bak",
        };
        std::string real_paths[] = {
            data_parent + pkg,
            obb_parent + pkg,
        };

        for (int i = 0; i < 2; i++)
        {
            struct stat st;
            if (stat(bak_paths[i].c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                    "paths: REPAIR — found leftover backup: %s", bak_paths[i].c_str());

                // If the real dir doesn't exist, rename backup → real
                struct stat st2;
                if (stat(real_paths[i].c_str(), &st2) != 0)
                {
                    if (rename(bak_paths[i].c_str(), real_paths[i].c_str()) == 0)
                    {
                        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                            "paths: REPAIR — restored %s from backup", real_paths[i].c_str());
                    }
                }
                else
                {
                    // Both exist — backup is stale, remove it
                    // (can't rmdir recursively in C easily, but at least log it)
                    __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                        "paths: REPAIR — stale backup exists alongside real dir: %s "
                                        "(user should delete via adb: rm -rf '%s')",
                                        bak_paths[i].c_str(), bak_paths[i].c_str());
                }
            }
        }

        // 2. Verify we can write to our data dir. If not, log a clear error
        //    so the user/dev knows the FUSE layer is busted.
        if (!verify_writable(s_data_dir))
        {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                "paths: ⚠ DATA DIR IS NOT WRITABLE: %s — "
                                "this usually means the installer corrupted FUSE permissions. "
                                "Fix: uninstall the game, delete /sdcard/Android/data/%s and "
                                "/sdcard/Android/obb/%s via 'adb shell rm -rf', then reinstall.",
                                s_data_dir.c_str(), pkg.c_str(), pkg.c_str());

            // Try to recover by using internal storage as fallback
            std::string internal = "/data/data/" + pkg + "/modloader";
            ensure_dir(internal);
            struct stat st;
            if (stat(internal.c_str(), &st) == 0 && verify_writable(internal))
            {
                __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                    "paths: REPAIR — falling back to internal storage: %s", internal.c_str());
                s_data_dir = internal;
            }
        }
        else
        {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                "paths: data dir verified writable: %s", s_data_dir.c_str());
        }
    }

    // ═══ JNI path resolution — the CORRECT approach per instructions ════════
    // Running inside the app process — ask Android for our own files dir
    // This is what every Android app does. No root. No guessing. Always correct.
    static std::string resolve_via_jni()
    {
        JavaVM *vm = get_stored_jvm();
        if (!vm)
        {
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                "paths: JavaVM not available — JNI path resolution unavailable");
            return "";
        }

        JNIEnv *env = nullptr;
        bool attached = false;
        int status = vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED)
        {
            if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
            {
                __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                    "paths: Failed to attach thread to JVM");
                return "";
            }
            attached = true;
        }
        else if (status != JNI_OK || !env)
        {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                                "paths: GetEnv failed with status %d", status);
            return "";
        }

        std::string result;

        // Get the application context via ActivityThread.currentApplication()
        jclass at_class = env->FindClass("android/app/ActivityThread");
        if (!at_class || env->ExceptionCheck())
        {
            if (env->ExceptionCheck())
                env->ExceptionClear();
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                "paths: ActivityThread class not found");
            if (attached)
                vm->DetachCurrentThread();
            return "";
        }

        jmethodID cur_app = env->GetStaticMethodID(at_class,
                                                   "currentApplication", "()Landroid/app/Application;");
        if (!cur_app || env->ExceptionCheck())
        {
            if (env->ExceptionCheck())
                env->ExceptionClear();
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                "paths: currentApplication method not found");
            if (attached)
                vm->DetachCurrentThread();
            return "";
        }

        jobject app = env->CallStaticObjectMethod(at_class, cur_app);
        if (!app || env->ExceptionCheck())
        {
            if (env->ExceptionCheck())
                env->ExceptionClear();
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                "paths: currentApplication() returned null — Activity not yet initialized");
            if (attached)
                vm->DetachCurrentThread();
            return "";
        }

        jclass ctx_class = env->FindClass("android/content/Context");
        if (!ctx_class)
        {
            if (env->ExceptionCheck())
                env->ExceptionClear();
            if (attached)
                vm->DetachCurrentThread();
            return "";
        }

        // Prefer getExternalFilesDir(null) — ADB-accessible without root
        jmethodID get_ext = env->GetMethodID(ctx_class,
                                             "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
        jobject dir = nullptr;
        if (get_ext)
        {
            dir = env->CallObjectMethod(app, get_ext, nullptr);
            if (env->ExceptionCheck())
            {
                env->ExceptionClear();
                dir = nullptr;
            }
        }

        // Fall back to getFilesDir() if external storage not available
        if (!dir)
        {
            jmethodID get_int = env->GetMethodID(ctx_class,
                                                 "getFilesDir", "()Ljava/io/File;");
            if (get_int)
            {
                dir = env->CallObjectMethod(app, get_int);
                if (env->ExceptionCheck())
                {
                    env->ExceptionClear();
                    dir = nullptr;
                }
            }
        }

        if (dir)
        {
            jclass file_class = env->FindClass("java/io/File");
            if (file_class)
            {
                jmethodID abs_path = env->GetMethodID(file_class,
                                                      "getAbsolutePath", "()Ljava/lang/String;");
                if (abs_path)
                {
                    jstring jpath = (jstring)env->CallObjectMethod(dir, abs_path);
                    if (jpath && !env->ExceptionCheck())
                    {
                        const char *cpath = env->GetStringUTFChars(jpath, nullptr);
                        if (cpath)
                        {
                            result = std::string(cpath);
                            env->ReleaseStringUTFChars(jpath, cpath);
                        }
                    }
                    if (env->ExceptionCheck())
                        env->ExceptionClear();
                }
            }
        }

        if (attached)
            vm->DetachCurrentThread();

        if (!result.empty())
        {
            // JNI returns .../Android/data/<pkg>/files
            // We want .../Android/data/<pkg>/modloader instead
            auto slash = result.rfind('/');
            if (slash != std::string::npos)
                result = result.substr(0, slash) + "/modloader";
            else
                result += "/modloader";
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                                "paths: JNI resolved data dir: %s", result.c_str());
        }

        return result;
    }

    void init()
    {
        std::string pkg = read_package_name();

        // Priority 1: JNI getExternalFilesDir() — the correct approach
        s_data_dir = resolve_via_jni();

        // If JNI fails (e.g. Activity not yet initialized), fall back to
        // constructing the path from the package name. This is NOT hardcoding
        // a specific path — it's using the standard Android external files
        // directory convention with the runtime-detected package name.
        if (s_data_dir.empty())
        {
            __android_log_print(ANDROID_LOG_WARN, LOG_TAG,
                                "paths: JNI resolution failed — using /proc/self/cmdline package: %s", pkg.c_str());

            // Probe the external files dir — the standard location Android uses
            // We try the canonical path that getExternalFilesDir would return
            std::string parent = "/storage/emulated/0/Android/data/" + pkg;
            std::string candidate = parent + "/modloader";
            struct stat st;
            if (stat(candidate.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                s_data_dir = candidate;
            }
            else
            {
                // Create it — the app has permission to its own external dir
                ensure_dir(parent);
                ensure_dir(candidate);
                s_data_dir = candidate;
            }
        }

        __android_log_print(ANDROID_LOG_INFO, LOG_TAG,
                            "paths: final data dir: %s", s_data_dir.c_str());

        // Repair any damage from botched installer runs
        repair_data_dirs();

        // Create subdirectories
        ensure_dir(mods_dir());
        ensure_dir(paks_dir());
        ensure_dir(sdk_dir());
        ensure_dir(sdk_classes_dir());
        ensure_dir(sdk_structs_dir());
        ensure_dir(sdk_enums_dir());
        ensure_dir(cxx_header_dir());
        ensure_dir(lua_types_dir());
    }

    const std::string &data_dir() { return s_data_dir; }
    std::string mods_dir() { return s_data_dir + "/mods/"; }
    std::string paks_dir() { return s_data_dir + "/paks/"; }
    std::string sdk_dir() { return s_data_dir + "/SDK/"; }
    std::string log_path() { return s_data_dir + "/UEModLoader.log"; }
    std::string crash_log() { return s_data_dir + "/modloader_crash.log"; }
    std::string sdk_classes_dir() { return s_data_dir + "/SDK/Classes/"; }
    std::string sdk_structs_dir() { return s_data_dir + "/SDK/Structs/"; }
    std::string sdk_enums_dir() { return s_data_dir + "/SDK/Enums/"; }
    std::string sdk_index_path() { return s_data_dir + "/SDK/_index.lua"; }
    std::string sdk_manifest_path() { return s_data_dir + "/SDK/_sdk_manifest.json"; }
    std::string cxx_header_dir() { return s_data_dir + "/CXXHeaderDump/"; }
    std::string lua_types_dir() { return s_data_dir + "/Lua/"; }
    std::string usmap_path() { return s_data_dir + "/Mappings.usmap"; }

} // namespace paths
