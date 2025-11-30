#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Log levels
typedef enum {
    LOG_LEVEL_DEVELOPER = 0,  // Most verbose - for development/debugging
    LOG_LEVEL_DEBUG,          // Detailed debugging information
    LOG_LEVEL_INFO,           // General informational messages
    LOG_LEVEL_WARNING,        // Warning messages
    LOG_LEVEL_ERROR,          // Error messages
    LOG_LEVEL_NONE            // Disable all logging
} LogLevel;

// Log configuration
typedef struct {
    LogLevel level;           // Minimum log level to output
    bool to_console;          // Output to stderr/stdout
    bool to_file;             // Output to log file
    char *log_file_path;      // Path to log file
    FILE *log_file;           // File handle for log file
    bool use_timestamps;      // Include timestamps in log messages
    bool use_colors;          // Use colors in console output
    bool rotation_enabled;    // Enable log file rotation
    size_t max_file_size;     // Maximum log file size before rotation (bytes)
    int max_rotated_files;   // Maximum number of rotated log files to keep
} LogConfig;

// Initialize logging system
// Returns 0 on success, -1 on error
int log_init(LogLevel level, bool to_console, bool to_file, const char *log_file_path);

// Initialize logging from environment variables
// Checks TSI_LOG_LEVEL, TSI_LOG_FILE, TSI_LOG_TO_CONSOLE, TSI_LOG_TO_FILE
int log_init_from_env(void);

// Set log level
void log_set_level(LogLevel level);

// Get current log level
LogLevel log_get_level(void);

// Enable/disable console output
void log_set_console(bool enable);

// Enable/disable file output
void log_set_file(bool enable);

// Set log file path (closes old file if open)
int log_set_file_path(const char *path);

// Configure log rotation
void log_set_rotation(bool enable, size_t max_file_size, int max_rotated_files);

// Enable/disable timestamps
void log_set_timestamps(bool enable);

// Enable/disable colors
void log_set_colors(bool enable);

// Core logging functions
void log_developer(const char *format, ...);  // Most verbose - for development
void log_debug(const char *format, ...);
void log_info(const char *format, ...);
void log_warning(const char *format, ...);
void log_error(const char *format, ...);

// Log with explicit level
void log_message(LogLevel level, const char *format, ...);

// Log with va_list (for internal use)
void log_vmessage(LogLevel level, const char *format, va_list args);

// Flush log output
void log_flush(void);

// Close logging system and free resources
void log_cleanup(void);

// Get log level name as string
const char *log_level_name(LogLevel level);

// Parse log level from string (case-insensitive)
// Returns LOG_LEVEL_NONE if invalid
LogLevel log_level_from_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif // LOG_H
