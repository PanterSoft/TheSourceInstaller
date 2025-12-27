/**
 * TSI Configuration System
 *
 * This module implements TSI's configuration management, which is central to
 * how TSI operates. Configuration is loaded from tsi.cfg and controls core
 * behavior like strict isolation mode.
 *
 * Config is loaded early in main() and is available throughout TSI's execution.
 */

#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // For strcasecmp
#include <sys/stat.h>
#include <unistd.h>

// Global configuration instance (singleton)
// This is initialized with defaults and loaded from tsi.cfg at startup
static TsiConfig g_config = {
    .strict_isolation = false,
    .initialized = false
};

TsiConfig* config_get(void) {
    return &g_config;
}

void config_get_path(char *path, size_t size, const char *tsi_prefix) {
    if (!tsi_prefix || !path || size == 0) {
        if (path && size > 0) {
            path[0] = '\0';
        }
        return;
    }
    snprintf(path, size, "%s/tsi.cfg", tsi_prefix);
}

// Create default config file if it doesn't exist
// IMPORTANT: Never overwrites existing config file - user modifications are preserved
static bool config_create_default(const char *tsi_prefix) {
    if (!tsi_prefix) {
        return false;
    }

    char config_path[1024];
    config_get_path(config_path, sizeof(config_path), tsi_prefix);

    // Check if config file already exists - NEVER overwrite existing config
    struct stat st;
    if (stat(config_path, &st) == 0) {
        log_debug("Config file already exists: %s (preserving user configuration)", config_path);
        return true; // Already exists, preserve user configuration
    }

    // Create default config file only if it doesn't exist
    // Double-check file doesn't exist (race condition protection)
    if (stat(config_path, &st) == 0) {
        log_debug("Config file was created between checks, preserving it");
        return true; // File exists now, preserve it
    }

    FILE *fp = fopen(config_path, "w");
    if (!fp) {
        // Final check - file might have been created between stat and fopen
        if (stat(config_path, &st) == 0) {
            log_debug("Config file was created by another process, preserving it");
            return true; // File exists now, preserve it
        }
        log_warning("Failed to create default config file: %s", config_path);
        return false;
    }

    fprintf(fp, "# TSI Configuration File\n");
    fprintf(fp, "# This file controls TSI behavior\n");
    fprintf(fp, "#\n");
    fprintf(fp, "# Strict Isolation Mode\n");
    fprintf(fp, "# When enabled, TSI will only use TSI-installed packages after bootstrap\n");
    fprintf(fp, "# During bootstrap, minimal system tools (gcc, /bin/sh) are still used\n");
    fprintf(fp, "# Set to 'true' to enable strict isolation, 'false' to disable (default)\n");
    fprintf(fp, "strict_isolation=false\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Logging Configuration\n");
    fprintf(fp, "# Log level: DEVELOPER, DEBUG, INFO, WARNING, ERROR, or NONE\n");
    fprintf(fp, "# Default: INFO (shows INFO, WARNING, ERROR messages)\n");
    fprintf(fp, "log_level=INFO\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Enable console logging (output to stderr)\n");
    fprintf(fp, "# Set to 'true' to enable, 'false' to disable\n");
    fprintf(fp, "# Default: true (enabled)\n");
    fprintf(fp, "log_to_console=true\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Enable file logging (output to log file)\n");
    fprintf(fp, "# Set to 'true' to enable, 'false' to disable\n");
    fprintf(fp, "# Default: false (disabled to prevent hangs on some systems)\n");
    fprintf(fp, "log_to_file=false\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Log file path (only used if log_to_file=true)\n");
    fprintf(fp, "# Default: $HOME/.tsi/tsi.log or /opt/tsi/logs/tsi.log\n");
    fprintf(fp, "# log_file_path=\n");

    fclose(fp);
    log_info("Created default config file: %s", config_path);
    return true;
}

bool config_load(const char *tsi_prefix) {
    if (!tsi_prefix) {
        log_debug("No TSI prefix provided, using default config (strict_isolation=false)");
        g_config.strict_isolation = false;
        g_config.initialized = true;
        return true;
    }

    char config_path[1024];
    config_get_path(config_path, sizeof(config_path), tsi_prefix);

    // Check if config file exists, create default if it doesn't
    struct stat st;
    if (stat(config_path, &st) != 0) {
        log_debug("Config file not found: %s, creating default config", config_path);
        // Try to create default config file
        if (!config_create_default(tsi_prefix)) {
            // If creation failed, use defaults
            log_debug("Using default config (strict_isolation=false)");
            g_config.strict_isolation = false;
            g_config.initialized = true;
            return true; // Not an error - defaults are fine
        }
        // Config file was created, now load it
    }

    log_debug("Loading config from: %s", config_path);

    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        log_warning("Failed to open config file: %s (using defaults)", config_path);
        g_config.strict_isolation = false;
        g_config.initialized = true;
        return true; // Not an error - defaults are fine
    }

    // Parse config file (simple key=value format)
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }
        if (len > 0 && line[len - 1] == '\r') {
            line[len - 1] = '\0';
            len--;
        }

        // Skip empty lines and comments
        if (len == 0 || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Parse key=value
        char *equals = strchr(line, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        char *key = line;
        char *value = equals + 1;

        // Trim whitespace
        while (*key == ' ' || *key == '\t') key++;
        while (*value == ' ' || *value == '\t') value++;
        char *key_end = key + strlen(key) - 1;
        while (key_end > key && (*key_end == ' ' || *key_end == '\t')) {
            *key_end = '\0';
            key_end--;
        }
        char *value_end = value + strlen(value) - 1;
        while (value_end > value && (*value_end == ' ' || *value_end == '\t')) {
            *value_end = '\0';
            value_end--;
        }

        // Parse strict_isolation
        if (strcmp(key, "strict_isolation") == 0) {
            if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 || strcmp(value, "yes") == 0) {
                g_config.strict_isolation = true;
                log_info("Strict isolation mode enabled");
            } else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0 || strcmp(value, "no") == 0) {
                g_config.strict_isolation = false;
                log_info("Strict isolation mode disabled");
            } else {
                log_warning("Invalid value for strict_isolation in config: %s (expected true/false/1/0/yes/no)", value);
            }
        }
        // Parse logging settings (stored for later application)
        else if (strcmp(key, "log_level") == 0) {
            LogLevel level = log_level_from_string(value);
            if (level != LOG_LEVEL_NONE || strcasecmp(value, "NONE") == 0) {
                log_set_level(level);
                log_debug("Log level set to: %s", value);
            } else {
                log_warning("Invalid log level in config: %s (expected DEVELOPER/DEBUG/INFO/WARNING/ERROR/NONE)", value);
            }
        }
        else if (strcmp(key, "log_to_console") == 0) {
            bool enable = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 || strcmp(value, "yes") == 0);
            log_set_console(enable);
            log_debug("Console logging %s", enable ? "enabled" : "disabled");
        }
        else if (strcmp(key, "log_to_file") == 0) {
            bool enable = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 || strcmp(value, "yes") == 0);
            log_set_file(enable);
            log_debug("File logging %s", enable ? "enabled" : "disabled");
        }
        else if (strcmp(key, "log_file_path") == 0 && strlen(value) > 0) {
            if (log_set_file_path(value) == 0) {
                log_debug("Log file path set to: %s", value);
            } else {
                log_warning("Failed to set log file path: %s", value);
            }
        }
    }

    fclose(fp);
    g_config.initialized = true;
    log_debug("Config loaded successfully (strict_isolation=%s)", g_config.strict_isolation ? "true" : "false");
    return true;
}

// Apply logging settings from config file
// This is called after config_load to ensure logging settings from tsi.cfg are applied
void config_apply_logging_settings(void) {
    // Settings are already applied during config_load parsing
    // This function exists for future extensibility and to make the API clear
    log_debug("Logging settings applied from config file");
}

bool config_is_strict_isolation(void) {
    if (!g_config.initialized) {
        // If not initialized, return false (default)
        return false;
    }
    return g_config.strict_isolation;
}

