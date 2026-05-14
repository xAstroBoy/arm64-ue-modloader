// modloader/src/util/logger.cpp
// Dual logging: UEModLoader.log (overwrite per session) + Android logcat
// Thread-safe via mutex. Log is truncated on each boot — clean per session.

#include "modloader/logger.h"
#include <android/log.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <mutex>
#include <vector>
#include <deque>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>

#define LOGCAT_TAG "UEModLoader"

namespace logger
{

    static FILE *s_file = nullptr;
    static std::string s_log_path;
    static std::mutex s_mutex;
    static int s_error_count = 0;
    static int s_line_count = 0;

    // Circular buffer for recent lines (for log_tail)
    static std::deque<std::string> s_recent_lines;
    static constexpr int MAX_RECENT = 2000;

    // Active stream file descriptors
    static std::vector<int> s_stream_fds;
    static std::mutex s_stream_mutex;

    // Boot timestamp
    static std::chrono::steady_clock::time_point s_boot_time;
    static std::chrono::steady_clock::time_point s_last_file_check;

    static void ensure_parent_dir(const std::string &file_path)
    {
        size_t slash = file_path.find_last_of('/');
        if (slash == std::string::npos)
            return;
        std::string dir = file_path.substr(0, slash);
        if (dir.empty())
            return;

        struct stat st;
        if (stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            return;

        mkdir(dir.c_str(), 0755);
    }

    // Must be called with s_mutex held.
    static bool open_log_file_locked(bool truncate)
    {
        if (s_log_path.empty())
            return false;

        ensure_parent_dir(s_log_path);
        const char *mode = truncate ? "w" : "a";
        s_file = fopen(s_log_path.c_str(), mode);
        if (!s_file)
        {
            int open_errno = errno;
            __android_log_print(ANDROID_LOG_ERROR, LOGCAT_TAG,
                                "Failed to open log file: %s (errno=%d)", s_log_path.c_str(), open_errno);

            if (open_errno == EACCES)
            {
                if (unlink(s_log_path.c_str()) == 0)
                {
                    s_file = fopen(s_log_path.c_str(), mode);
                    if (s_file)
                    {
                        __android_log_print(ANDROID_LOG_WARN, LOGCAT_TAG,
                                            "Recovered log file by unlink+recreate: %s", s_log_path.c_str());
                        return true;
                    }
                }
            }
            return false;
        }

        return true;
    }

    // Must be called with s_mutex held.
    static void ensure_log_file_open_locked()
    {
        if (s_log_path.empty())
            return;

        // Throttle filesystem checks to avoid per-line syscall overhead.
        auto now = std::chrono::steady_clock::now();
        if (s_last_file_check.time_since_epoch().count() != 0)
        {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_file_check).count();
            if (elapsed < 500 && s_file)
                return;
        }
        s_last_file_check = now;

        bool missing_on_path = (access(s_log_path.c_str(), F_OK) != 0);
        if (s_file && !missing_on_path)
            return;

        if (s_file)
        {
            fclose(s_file);
            s_file = nullptr;
        }

        if (open_log_file_locked(false))
        {
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            struct tm tm_info;
            localtime_r(&tv.tv_sec, &tm_info);
            char date_buf[64];
            strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", &tm_info);
            fprintf(s_file, "--- LOG FILE RECREATED %s ---\n", date_buf);
            fflush(s_file);
            __android_log_print(ANDROID_LOG_WARN, LOGCAT_TAG,
                                "Log file was missing and has been recreated: %s", s_log_path.c_str());
        }
    }

    static void get_timestamp(char *buf, size_t len)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_boot_time);
        auto total_ms = elapsed.count();
        int hours = static_cast<int>((total_ms / 3600000) % 24);
        int mins = static_cast<int>((total_ms / 60000) % 60);
        int secs = static_cast<int>((total_ms / 1000) % 60);
        int ms = static_cast<int>(total_ms % 1000);
        snprintf(buf, len, "%02d:%02d:%02d.%03d", hours, mins, secs, ms);
    }

    void init(const std::string &log_path)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_log_path = log_path;
        s_boot_time = std::chrono::steady_clock::now();
        s_last_file_check = std::chrono::steady_clock::time_point{};
        s_error_count = 0;
        s_line_count = 0;
        s_recent_lines.clear();

        // Overwrite (truncate) the log on every boot — user requested clean log per session
        if (!open_log_file_locked(true))
            return;

        // Write session header
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        struct tm tm_info;
        localtime_r(&tv.tv_sec, &tm_info);
        char date_buf[64];
        strftime(date_buf, sizeof(date_buf), "%Y-%m-%d %H:%M:%S", &tm_info);

        fprintf(s_file, "--- SESSION START %s ---\n", date_buf);
        fflush(s_file);
    }

    void shutdown()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_file)
        {
            fprintf(s_file, "--- SESSION END ---\n");
            fflush(s_file);
            fclose(s_file);
            s_file = nullptr;
        }
    }

    static void write_line(const char *level, const char *source, const char *msg)
    {
        ensure_log_file_open_locked();

        char ts[32];
        get_timestamp(ts, sizeof(ts));

        // Pad source to 8 chars
        char src_padded[12];
        snprintf(src_padded, sizeof(src_padded), "%-8s", source);

        char line[4096];
        snprintf(line, sizeof(line), "[%s] [%-5s] [%s] %s", ts, level, src_padded, msg);

        // Write to file
        if (s_file)
        {
            fprintf(s_file, "%s\n", line);
            fflush(s_file);
        }

        // Buffer recent lines
        std::string line_str(line);
        s_recent_lines.push_back(line_str);
        if (static_cast<int>(s_recent_lines.size()) > MAX_RECENT)
        {
            s_recent_lines.pop_front();
        }
        s_line_count++;

        // Stream to any connected sockets
        {
            std::lock_guard<std::mutex> slock(s_stream_mutex);
            if (!s_stream_fds.empty())
            {
                std::string with_nl = line_str + "\n";
                std::vector<int> dead;
                for (int fd : s_stream_fds)
                {
                    ssize_t written = write(fd, with_nl.c_str(), with_nl.size());
                    if (written < 0)
                    {
                        dead.push_back(fd);
                    }
                }
                for (int fd : dead)
                {
                    s_stream_fds.erase(
                        std::remove(s_stream_fds.begin(), s_stream_fds.end(), fd),
                        s_stream_fds.end());
                }
            }
        }

        // Android logcat level
        int prio = ANDROID_LOG_INFO;
        if (strcmp(level, "WARN") == 0)
            prio = ANDROID_LOG_WARN;
        else if (strcmp(level, "ERROR") == 0)
            prio = ANDROID_LOG_ERROR;
        else if (strcmp(level, "DEBUG") == 0)
            prio = ANDROID_LOG_DEBUG;
        __android_log_print(prio, LOGCAT_TAG, "%s", line);
    }

    void log_info(const char *source, const char *fmt, ...)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write_line("INFO", source, buf);
    }

    void log_warn(const char *source, const char *fmt, ...)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write_line("WARN", source, buf);
    }

    void log_error(const char *source, const char *fmt, ...)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_error_count++;
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write_line("ERROR", source, buf);
    }

    void log_debug(const char *source, const char *fmt, ...)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        char buf[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        write_line("DEBUG", source, buf);
    }

    void log_raw(const char *text)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        ensure_log_file_open_locked();
        if (s_file)
        {
            fprintf(s_file, "%s\n", text);
            fflush(s_file);
        }
        __android_log_print(ANDROID_LOG_INFO, LOGCAT_TAG, "%s", text);
    }

    std::string get_tail(int lines)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        std::string result;
        int start = static_cast<int>(s_recent_lines.size()) - lines;
        if (start < 0)
            start = 0;
        for (int i = start; i < static_cast<int>(s_recent_lines.size()); i++)
        {
            result += s_recent_lines[static_cast<size_t>(i)];
            result += "\n";
        }
        return result;
    }

    void add_stream_socket(int fd)
    {
        std::lock_guard<std::mutex> lock(s_stream_mutex);
        s_stream_fds.push_back(fd);
    }

    void remove_stream_socket(int fd)
    {
        std::lock_guard<std::mutex> lock(s_stream_mutex);
        s_stream_fds.erase(
            std::remove(s_stream_fds.begin(), s_stream_fds.end(), fd),
            s_stream_fds.end());
    }

    int get_error_count()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_error_count;
    }

    int get_line_count()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_line_count;
    }

    const std::string &get_log_path()
    {
        return s_log_path;
    }

} // namespace logger
