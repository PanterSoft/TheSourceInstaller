#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TSI Configuration System
 *
 * Configuration is a central part of TSI's operation. The config file (tsi.cfg)
 * controls core behavior like strict isolation mode and other settings.
 *
 * Config is automatically loaded at startup in main() and is available to all
 * commands and subsystems throughout TSI's execution.
 */

// Configuration structure
typedef struct {
    bool strict_isolation;  // Strict isolation mode (only TSI packages after bootstrap)
    bool initialized;        // Whether config has been loaded
} TsiConfig;

// Get configuration instance (singleton)
TsiConfig* config_get(void);

// Load configuration from file
// Returns true on success, false on failure (uses defaults)
bool config_load(const char *tsi_prefix);

// Check if strict isolation is enabled
bool config_is_strict_isolation(void);

// Get config file path
void config_get_path(char *path, size_t size, const char *tsi_prefix);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_H

