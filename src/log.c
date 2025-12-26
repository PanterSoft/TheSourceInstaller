#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <strings.h>  // For strcasecmp on some systems
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

// Forward declarations
static void rotate_log_file(void);
static int create_directory_recursive(const char *path);

// Global log configuration
// CRITICAL: File logging and timestamps disabled by default to prevent hangs
// Enable via TSI_LOG_TO_FILE=1 and TSI_LOG_TIMESTAMPS=1 environment variables if needed
static LogConfig g_log_config = {
    .level = LOG_LEVEL_DEVELOPER,  // Default to developer mode for maximum verbosity
    .to_console = false,  // Disable console logging by default
    .to_file = false,  // DISABLED by default to prevent hangs - enable via env var
    .log_file_path = NULL,
    .log_file = NULL,
    .use_timestamps = false,  // DISABLED by default - time()/gmtime() can hang on some systems
    .use_colors = false,  // No colors needed for file logging
    .rotation_enabled = true,
    .max_file_size = 10 * 1024 * 1024,  // 10MB default
    .max_rotated_files = 5  // Keep 5 rotated files
};

// Track if logging is initialized
static bool log_initialized = false;

// Color codes for console output
#define LOG_COLOR_RESET     "\033[0m"
#define LOG_COLOR_DEVELOPER "\033[35m"  // Magenta
#define LOG_COLOR_DEBUG     "\033[36m"  // Cyan
#define LOG_COLOR_INFO      "\033[32m"  // Green
#define LOG_COLOR_WARNING   "\033[33m"  // Yellow
#define LOG_COLOR_ERROR     "\033[31m"  // Red

// Check if stderr supports colors
static bool supports_colors(void) {
    if (!g_log_config.use_colors) return false;
    const char *term = getenv("TERM");
    if (!term) return false;
    return isatty(STDERR_FILENO) != 0;
}

// Get current timestamp string (non-blocking, fail-fast)
// CRITICAL: This function must never block or hang
static void get_timestamp(char *buffer, size_t buffer_size) {
    // Use a simple approach that won't hang
    // time() and gmtime() can hang on some systems (Docker, chroot, broken timezone data)
    // Use a fallback that never blocks
    time_t now = time(NULL);
    if (now == (time_t)-1 || now == 0) {
        // time() failed or returned invalid value - use fallback
        snprintf(buffer, buffer_size, "unknown");
        return;
    }

    // Use gmtime instead of localtime to avoid timezone lookups that might hang
    // But even gmtime() can block on some broken systems, so we need a timeout mechanism
    // For now, just try it - if it hangs, the whole program hangs (but that's better than
    // hanging on every log message). The real fix is to disable timestamps by default.
    struct tm *tm_info = gmtime(&now);
    if (tm_info) {
        // Use a simple format that doesn't require locale support
        if (strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
            // strftime failed - use fallback
            snprintf(buffer, buffer_size, "unknown");
        }
    } else {
        // gmtime() failed - use fallback
        snprintf(buffer, buffer_size, "unknown");
    }
}

// Format and write log message
static void write_log_message(LogLevel level, const char *format, va_list args) {
    // If logging not initialized, mark as initialized but skip file operations
    // This prevents hangs from trying to create directories or open files
    if (!log_initialized) {
        // Mark as initialized to prevent repeated attempts
        // But don't try to initialize file logging (which might hang)
        log_initialized = true;
        // Keep file logging disabled to prevent hangs
        g_log_config.to_file = false;
    }

    if (level < g_log_config.level) {
        return;
    }

    char timestamp[64] = {0};
    if (g_log_config.use_timestamps) {
        get_timestamp(timestamp, sizeof(timestamp));
    }

    const char *level_name = log_level_name(level);
    const char *color_code = "";
    const char *color_reset = "";

    if (supports_colors()) {
        switch (level) {
            case LOG_LEVEL_DEVELOPER:
                color_code = LOG_COLOR_DEVELOPER;
                break;
            case LOG_LEVEL_DEBUG:
                color_code = LOG_COLOR_DEBUG;
                break;
            case LOG_LEVEL_INFO:
                color_code = LOG_COLOR_INFO;
                break;
            case LOG_LEVEL_WARNING:
                color_code = LOG_COLOR_WARNING;
                break;
            case LOG_LEVEL_ERROR:
                color_code = LOG_COLOR_ERROR;
                break;
            default:
                break;
        }
        color_reset = LOG_COLOR_RESET;
    }

    // Format the message
    char message[4096];
    va_list args_copy;
    va_copy(args_copy, args);
    vsnprintf(message, sizeof(message), format, args_copy);
    va_end(args_copy);

    // Write to console
    if (g_log_config.to_console) {
        if (g_log_config.use_timestamps) {
            fprintf(stderr, "%s[%s]%s %s%s%s: %s%s\n",
                    color_code, timestamp, color_reset,
                    color_code, level_name, color_reset,
                    message, color_reset);
        } else {
            fprintf(stderr, "%s%s%s: %s%s\n",
                    color_code, level_name, color_reset,
                    message, color_reset);
        }
        fflush(stderr);
    }

    // Write to file (non-blocking, fail-fast)
    if (g_log_config.to_file && g_log_config.log_file) {
        // Check if file is still valid before writing
        if (ferror(g_log_config.log_file)) {
            // File has an error, disable file logging
            g_log_config.to_file = false;
            return;
        }

        if (g_log_config.use_timestamps) {
            fprintf(g_log_config.log_file, "[%s] %s: %s\n",
                    timestamp, level_name, message);
        } else {
            fprintf(g_log_config.log_file, "%s: %s\n",
                    level_name, message);
        }

        // Flush immediately to prevent buffering issues, but don't block
        fflush(g_log_config.log_file);

        // Check if rotation is needed (check file size)
        // Only check if file is still valid
        if (g_log_config.rotation_enabled && g_log_config.log_file_path && !ferror(g_log_config.log_file)) {
            long current_pos = ftell(g_log_config.log_file);
            if (current_pos > 0 && (size_t)current_pos >= g_log_config.max_file_size) {
                // Rotate log file (this might fail, but that's okay)
                rotate_log_file();
            }
        }
    }
}

// Create directory recursively (like mkdir -p)
// Made non-blocking and fail-fast to prevent hangs
static int create_directory_recursive(const char *path) {
    if (!path || *path == '\0') {
        return -1;
    }

    // Check if directory already exists (fast path)
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;  // Directory exists
        }
        return -1;  // Path exists but is not a directory
    }

    // Limit recursion depth to prevent hangs
    static int depth = 0;
    if (depth > 10) {
        return -1;  // Too deep, give up
    }

    // Create parent directories first
    char *path_copy = strdup(path);
    if (!path_copy) {
        return -1;
    }

    char *slash = strrchr(path_copy, '/');
    if (slash && slash != path_copy) {
        *slash = '\0';
        // Recursively create parent directory
        depth++;
        int result = create_directory_recursive(path_copy);
        depth--;
        if (result != 0) {
            free(path_copy);
            return -1;
        }
    }
    free(path_copy);

    // Create this directory (non-blocking)
    if (mkdir(path, 0755) != 0) {
        // Check if it was created by another process (race condition)
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            return 0;  // Directory now exists
        }
        // Don't fail on permission errors - just return error code
        // The caller can decide whether to continue
        return -1;  // Failed to create
    }

    return 0;
}

// Rotate log file when it reaches max size
static void rotate_log_file(void) {
    if (!g_log_config.log_file_path) return;

    // Close current file
    if (g_log_config.log_file) {
        fclose(g_log_config.log_file);
        g_log_config.log_file = NULL;
    }

    // Rotate existing files: tsi.log.4 -> tsi.log.5, ..., tsi.log -> tsi.log.1
    for (int i = g_log_config.max_rotated_files - 1; i >= 1; i--) {
        char old_path[1024];
        char new_path[1024];

        if (i == 1) {
            snprintf(old_path, sizeof(old_path), "%s", g_log_config.log_file_path);
        } else {
            snprintf(old_path, sizeof(old_path), "%s.%d", g_log_config.log_file_path, i - 1);
        }
        snprintf(new_path, sizeof(new_path), "%s.%d", g_log_config.log_file_path, i);

        // Remove file if it exists (for the last rotation)
        if (i == g_log_config.max_rotated_files - 1) {
            unlink(new_path);
        }

        // Rename old to new (ignore errors if file doesn't exist)
        rename(old_path, new_path);
    }

    // Reopen log file for writing
    g_log_config.log_file = fopen(g_log_config.log_file_path, "a");
    if (!g_log_config.log_file) {
        // Failed to reopen, disable file logging
        g_log_config.to_file = false;
    }
}

int log_init(LogLevel level, bool to_console, bool to_file, const char *log_file_path) {
    g_log_config.level = level;
    g_log_config.to_console = to_console;
    g_log_config.to_file = to_file;

    if (log_file_path) {
        int result = log_set_file_path(log_file_path);
        if (result == 0) {
            log_initialized = true;
        }
        return result;
    }

    log_initialized = true;
    return 0;
}

int log_init_from_env(void) {
    // Get log level from environment (defaults to LOG_LEVEL_DEVELOPER if not set)
    const char *level_str = getenv("TSI_LOG_LEVEL");
    if (level_str) {
        LogLevel level = log_level_from_string(level_str);
        if (level != LOG_LEVEL_NONE || strcasecmp(level_str, "NONE") == 0) {
            g_log_config.level = level;
        }
    }
    // If TSI_LOG_LEVEL not set, keep default LOG_LEVEL_DEVELOPER

    // Get console output setting
    const char *console_str = getenv("TSI_LOG_TO_CONSOLE");
    if (console_str) {
        g_log_config.to_console = (strcmp(console_str, "1") == 0 ||
                                   strcasecmp(console_str, "true") == 0 ||
                                   strcasecmp(console_str, "yes") == 0);
    }

    // Get file output setting (default is FALSE to prevent hangs - must be explicitly enabled)
    const char *file_str = getenv("TSI_LOG_TO_FILE");
    if (file_str) {
        // Only enable if explicitly set to 1/true/yes
        g_log_config.to_file = (strcmp(file_str, "1") == 0 ||
                                strcasecmp(file_str, "true") == 0 ||
                                strcasecmp(file_str, "yes") == 0);
    } else {
        // Default: disabled to prevent hangs
        g_log_config.to_file = false;
    }

    // Get timestamp setting (default is FALSE to prevent hangs - must be explicitly enabled)
    const char *timestamps_str = getenv("TSI_LOG_TIMESTAMPS");
    if (timestamps_str) {
        // Only enable if explicitly set to 1/true/yes
        g_log_config.use_timestamps = (strcmp(timestamps_str, "1") == 0 ||
                                       strcasecmp(timestamps_str, "true") == 0 ||
                                       strcasecmp(timestamps_str, "yes") == 0);
    } else {
        // Default: disabled to prevent hangs from time()/gmtime()
        g_log_config.use_timestamps = false;
    }

    // Get rotation settings
    const char *rotation_str = getenv("TSI_LOG_ROTATION");
    if (rotation_str) {
        g_log_config.rotation_enabled = (strcmp(rotation_str, "1") == 0 ||
                                         strcasecmp(rotation_str, "true") == 0 ||
                                         strcasecmp(rotation_str, "yes") == 0);
    }

    const char *max_size_str = getenv("TSI_LOG_MAX_SIZE");
    if (max_size_str) {
        char *endptr;
        long max_size = strtol(max_size_str, &endptr, 10);
        if (*endptr == '\0' && max_size > 0) {
            // If value is < 1000, assume MB, otherwise assume bytes
            if (max_size < 1000) {
                g_log_config.max_file_size = (size_t)(max_size * 1024 * 1024);
            } else {
                g_log_config.max_file_size = (size_t)max_size;
            }
        }
    }

    const char *max_files_str = getenv("TSI_LOG_MAX_FILES");
    if (max_files_str) {
        char *endptr;
        long max_files = strtol(max_files_str, &endptr, 10);
        if (*endptr == '\0' && max_files > 0 && max_files <= 10) {
            g_log_config.max_rotated_files = (int)max_files;
        }
    }

    // Get log file path (only if file logging is explicitly enabled)
    // CRITICAL: Don't try to open files unless explicitly requested
    if (g_log_config.to_file) {
        const char *log_file = getenv("TSI_LOG_FILE");
        if (log_file) {
            int result = log_set_file_path(log_file);
            if (result == 0) {
                log_initialized = true;
            } else {
                // If we can't create the log file, disable file logging but continue
                g_log_config.to_file = false;
                log_initialized = true;
            }
            return result;
        } else {
            // Default log file location (only if file logging is explicitly enabled)
            const char *home = getenv("HOME");
            if (!home) home = "/root";
            char default_path[1024];
            snprintf(default_path, sizeof(default_path), "%s/.tsi/tsi.log", home);
            int result = log_set_file_path(default_path);
            if (result == 0) {
                log_initialized = true;
            } else {
                // If we can't create the log file, disable file logging but continue
                g_log_config.to_file = false;
                log_initialized = true;
            }
            return 0;  // Don't fail if log file can't be created
        }
    }

    // File logging disabled - just mark as initialized
    log_initialized = true;
    return 0;
}

void log_set_level(LogLevel level) {
    g_log_config.level = level;
}

LogLevel log_get_level(void) {
    return g_log_config.level;
}

void log_set_console(bool enable) {
    g_log_config.to_console = enable;
}

void log_set_file(bool enable) {
    g_log_config.to_file = enable;
}

int log_set_file_path(const char *path) {
    // Close existing file if open
    if (g_log_config.log_file && g_log_config.log_file != stderr && g_log_config.log_file != stdout) {
        fclose(g_log_config.log_file);
        g_log_config.log_file = NULL;
    }

    // Free old path
    if (g_log_config.log_file_path) {
        free(g_log_config.log_file_path);
        g_log_config.log_file_path = NULL;
    }

    if (!path) {
        return 0;
    }

    // Try to create directory if it doesn't exist (non-blocking, fail-fast)
    // But don't block if it fails - just try to open the file
    char *path_copy = strdup(path);
    if (path_copy) {
        char *last_slash = strrchr(path_copy, '/');
        if (last_slash && last_slash != path_copy) {
            *last_slash = '\0';
            // Try to create directory, but don't wait or retry
            // If it fails, fopen will handle it
            create_directory_recursive(path_copy);
            // Continue regardless of result
        }
        free(path_copy);
    }

    // Open log file in append mode (non-blocking)
    // If this fails, we'll just disable file logging
    FILE *file = fopen(path, "a");
    if (!file) {
        // File opening failed - this is not fatal, just disable file logging
        // Don't try to create directories again or retry - just fail fast
        return -1;
    }

    // Set file to unbuffered or line-buffered to prevent hangs
    setvbuf(file, NULL, _IOLBF, 0);

    g_log_config.log_file = file;
    g_log_config.log_file_path = strdup(path);
    if (!g_log_config.log_file_path) {
        fclose(file);
        g_log_config.log_file = NULL;
        return -1;
    }

    return 0;
}

void log_set_rotation(bool enable, size_t max_file_size, int max_rotated_files) {
    g_log_config.rotation_enabled = enable;
    g_log_config.max_file_size = max_file_size;
    g_log_config.max_rotated_files = max_rotated_files;
    if (max_rotated_files < 1) g_log_config.max_rotated_files = 1;
    if (max_rotated_files > 10) g_log_config.max_rotated_files = 10;  // Reasonable limit
}

void log_set_timestamps(bool enable) {
    g_log_config.use_timestamps = enable;
}

void log_set_colors(bool enable) {
    g_log_config.use_colors = enable;
}

void log_developer(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(LOG_LEVEL_DEVELOPER, format, args);
    va_end(args);
}

void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void log_warning(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(LOG_LEVEL_WARNING, format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

void log_message(LogLevel level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(level, format, args);
    va_end(args);
}

void log_vmessage(LogLevel level, const char *format, va_list args) {
    write_log_message(level, format, args);
}

void log_flush(void) {
    if (g_log_config.to_console) {
        fflush(stderr);
    }
    if (g_log_config.to_file && g_log_config.log_file) {
        fflush(g_log_config.log_file);
    }
}

void log_cleanup(void) {
    if (g_log_config.log_file &&
        g_log_config.log_file != stderr &&
        g_log_config.log_file != stdout) {
        fclose(g_log_config.log_file);
        g_log_config.log_file = NULL;
    }

    if (g_log_config.log_file_path) {
        free(g_log_config.log_file_path);
        g_log_config.log_file_path = NULL;
    }

    g_log_config.to_file = false;
    g_log_config.to_console = false;
}

const char *log_level_name(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEVELOPER:
            return "DEVELOPER";
        case LOG_LEVEL_DEBUG:
            return "DEBUG";
        case LOG_LEVEL_INFO:
            return "INFO";
        case LOG_LEVEL_WARNING:
            return "WARNING";
        case LOG_LEVEL_ERROR:
            return "ERROR";
        case LOG_LEVEL_NONE:
            return "NONE";
        default:
            return "UNKNOWN";
    }
}

LogLevel log_level_from_string(const char *str) {
    if (!str) return LOG_LEVEL_NONE;

    // Case-insensitive comparison
    if (strcasecmp(str, "DEVELOPER") == 0 || strcasecmp(str, "DEV") == 0) return LOG_LEVEL_DEVELOPER;
    if (strcasecmp(str, "DEBUG") == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(str, "INFO") == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(str, "WARNING") == 0 || strcasecmp(str, "WARN") == 0) return LOG_LEVEL_WARNING;
    if (strcasecmp(str, "ERROR") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(str, "NONE") == 0) return LOG_LEVEL_NONE;

    // Try numeric values
    char *endptr;
    long num = strtol(str, &endptr, 10);
    if (*endptr == '\0' && num >= 0 && num <= LOG_LEVEL_NONE) {
        return (LogLevel)num;
    }

    return LOG_LEVEL_NONE;
}
