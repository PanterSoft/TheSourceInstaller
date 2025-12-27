#include "package.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/utsname.h>

// Simple JSON parser (minimal implementation)
static char* json_get_string(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) return NULL;

    pos = strchr(pos, ':');
    if (!pos) return NULL;
    pos++; // Skip ':'

    // Skip whitespace
    while (isspace(*pos)) pos++;

    if (*pos != '"') return NULL;
    pos++; // Skip opening quote

    const char *end = strchr(pos, '"');
    if (!end) return NULL;

    size_t len = end - pos;
    char *result = malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, pos, len);
    result[len] = '\0';
    return result;
}

static char** json_get_array(const char *json, const char *key, size_t *count) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) {
        *count = 0;
        return NULL;
    }

    pos = strchr(pos, '[');
    if (!pos) {
        *count = 0;
        return NULL;
    }
    pos++; // Skip '['

    // Count items
    *count = 0;
    const char *p = pos;
    while (*p && *p != ']') {
        if (*p == '"') {
            (*count)++;
            p = strchr(p + 1, '"');
            if (!p) break;
        }
        p++;
    }

    if (*count == 0) return NULL;

    char **result = malloc(sizeof(char*) * (*count));
    if (!result) {
        *count = 0;
        return NULL;
    }

    // Extract items
    size_t idx = 0;
    p = pos;
    while (*p && *p != ']' && idx < *count) {
        if (*p == '"') {
            const char *start = p + 1;
            const char *end = strchr(start, '"');
            if (end) {
                size_t len = end - start;
                result[idx] = malloc(len + 1);
                memcpy(result[idx], start, len);
                result[idx][len] = '\0';
                idx++;
                p = end + 1;
            } else {
                break;
            }
        } else {
            p++;
        }
    }

    *count = idx;
    return result;
}

// Get OS name (darwin, linux, freebsd, openbsd, netbsd, etc.) - used for OS-specific configurations
// Darwin = macOS (Apple's Unix-based OS)
// Linux = All Linux distributions (Debian, Ubuntu, RedHat, Alpine, etc.)
// Other Unix variants are detected via uname()
const char* package_get_os_name(void) {
    static char os_name[64] = {0};
    static bool initialized = false;

    if (initialized) {
        return os_name;
    }

    initialized = true;

#ifdef __APPLE__
    // macOS uses Darwin kernel - this is the official name
    strncpy(os_name, "darwin", sizeof(os_name) - 1);
#else
    struct utsname uts;
    if (uname(&uts) == 0) {
        // Convert to lowercase for consistency
        // This will return: linux, freebsd, openbsd, netbsd, sunos, aix, hp-ux, etc.
        for (int i = 0; uts.sysname[i] && i < (int)sizeof(os_name) - 1; i++) {
            os_name[i] = tolower(uts.sysname[i]);
        }
        os_name[sizeof(os_name) - 1] = '\0';

        // Normalize some common variants
        if (strcmp(os_name, "gnu/linux") == 0 || strcmp(os_name, "gnu") == 0) {
            strncpy(os_name, "linux", sizeof(os_name) - 1);
        } else if (strcmp(os_name, "sunos") == 0) {
            // Solaris/Illumos - could use "solaris" or "sunos"
            strncpy(os_name, "sunos", sizeof(os_name) - 1);
        }
    } else {
        // Fallback: assume Linux (most common Unix-like system)
        strncpy(os_name, "linux", sizeof(os_name) - 1);
    }
#endif

    log_debug("Detected OS name: %s", os_name);
    return os_name;
}

// Parse JSON object (for env, env_darwin, env_linux, etc.)
static bool json_get_object(const char *json, const char *key, char ***keys_out, char ***values_out, size_t *count_out) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos) {
        *count_out = 0;
        *keys_out = NULL;
        *values_out = NULL;
        return false;
    }

    pos = strchr(pos, ':');
    if (!pos) {
        *count_out = 0;
        *keys_out = NULL;
        *values_out = NULL;
        return false;
    }
    pos++; // Skip ':'

    // Skip whitespace
    while (isspace(*pos)) pos++;

    if (*pos != '{') {
        *count_out = 0;
        *keys_out = NULL;
        *values_out = NULL;
        return false;
    }
    pos++; // Skip '{'

    // Count key-value pairs
    size_t count = 0;
    const char *p = pos;
    int brace_depth = 1;
    while (*p && brace_depth > 0) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') brace_depth--;
        else if (*p == '"' && brace_depth == 1) {
            // Found a potential key
            const char *key_start = p + 1;
            const char *key_end = strchr(key_start, '"');
            if (key_end) {
                // Look for value
                const char *colon = strchr(key_end, ':');
                if (colon) {
                    colon++;
                    while (isspace(*colon)) colon++;
                    if (*colon == '"') {
                        // String value
                        const char *value_end = strchr(colon + 1, '"');
                        if (value_end) {
                            count++;
                            p = value_end + 1;
                            continue;
                        }
                    }
                }
            }
        }
        p++;
    }

    if (count == 0) {
        *count_out = 0;
        *keys_out = NULL;
        *values_out = NULL;
        return false;
    }

    // Allocate arrays
    char **keys = malloc(sizeof(char*) * count);
    char **values = malloc(sizeof(char*) * count);
    if (!keys || !values) {
        if (keys) free(keys);
        if (values) free(values);
        *count_out = 0;
        *keys_out = NULL;
        *values_out = NULL;
        return false;
    }

    // Extract key-value pairs
    size_t idx = 0;
    p = pos;
    brace_depth = 1;
    while (*p && brace_depth > 0 && idx < count) {
        if (*p == '{') brace_depth++;
        else if (*p == '}') brace_depth--;
        else if (*p == '"' && brace_depth == 1) {
            // Extract key
            const char *key_start = p + 1;
            const char *key_end = strchr(key_start, '"');
            if (key_end) {
                size_t key_len = key_end - key_start;
                keys[idx] = malloc(key_len + 1);
                memcpy(keys[idx], key_start, key_len);
                keys[idx][key_len] = '\0';

                // Find value
                const char *colon = strchr(key_end, ':');
                if (colon) {
                    colon++;
                    while (isspace(*colon)) colon++;
                    if (*colon == '"') {
                        // Extract string value
                        const char *value_start = colon + 1;
                        const char *value_end = strchr(value_start, '"');
                        if (value_end) {
                            size_t value_len = value_end - value_start;
                            values[idx] = malloc(value_len + 1);
                            memcpy(values[idx], value_start, value_len);
                            values[idx][value_len] = '\0';
                            idx++;
                            p = value_end + 1;
                            continue;
                        }
                    }
                }
            }
        }
        p++;
    }

    *count_out = idx;
    *keys_out = keys;
    *values_out = values;
    return true;
}

// Merge OS-specific env into base env
// Base env is always the default fallback - OS-specific only overrides/adds to it
// If OS-specific config doesn't exist or doesn't have a key, base env value is used
static void merge_env_os_specific(Package *pkg, const char *json_string, const char *os_name) {
    char os_env_key[128];
    snprintf(os_env_key, sizeof(os_env_key), "env_%s", os_name);

    char **os_keys = NULL;
    char **os_values = NULL;
    size_t os_count = 0;

    // Try to load OS-specific env (e.g., env_darwin, env_linux)
    if (json_get_object(json_string, os_env_key, &os_keys, &os_values, &os_count)) {
        log_debug("Found OS-specific env for %s: %zu variables", os_name, os_count);

        // Merge OS-specific env into base env
        // OS-specific values override base values for matching keys
        // New keys from OS-specific are added to base env
        for (size_t i = 0; i < os_count; i++) {
            // Check if key already exists in base env
            bool found = false;
            for (size_t j = 0; j < pkg->env_count; j++) {
                if (pkg->env_keys[j] && strcmp(pkg->env_keys[j], os_keys[i]) == 0) {
                    // Override existing base value with OS-specific value
                    log_debug("Overriding base env %s=%s with OS-specific %s=%s",
                             os_keys[i], pkg->env_values[j], os_keys[i], os_values[i]);
                    free(pkg->env_values[j]);
                    pkg->env_values[j] = strdup(os_values[i]);
                    found = true;
                    break;
                }
            }

            if (!found) {
                // Add new key-value pair from OS-specific config
                log_debug("Adding OS-specific env: %s=%s", os_keys[i], os_values[i]);
                pkg->env_keys = realloc(pkg->env_keys, sizeof(char*) * (pkg->env_count + 1));
                pkg->env_values = realloc(pkg->env_values, sizeof(char*) * (pkg->env_count + 1));
                pkg->env_keys[pkg->env_count] = strdup(os_keys[i]);
                pkg->env_values[pkg->env_count] = strdup(os_values[i]);
                pkg->env_count++;
            }

            free(os_keys[i]);
            free(os_values[i]);
        }
        free(os_keys);
        free(os_values);
    } else {
        // No OS-specific env found - base env is used as-is (default fallback)
        log_debug("No OS-specific env found for %s, using base env as default", os_name);
    }
}

// Merge OS-specific arrays into base arrays
static void merge_array_os_specific(char ***base_array, size_t *base_count, const char *json_string, const char *field_name, const char *os_name) {
    char os_field_key[128];
    snprintf(os_field_key, sizeof(os_field_key), "%s_%s", field_name, os_name);

    char **os_array = NULL;
    size_t os_count = 0;
    os_array = json_get_array(json_string, os_field_key, &os_count);

    if (os_array && os_count > 0) {
        // Append OS-specific items to base array
        *base_array = realloc(*base_array, sizeof(char*) * (*base_count + os_count));
        for (size_t i = 0; i < os_count; i++) {
            (*base_array)[*base_count + i] = strdup(os_array[i]);
            free(os_array[i]);
        }
        free(os_array);
        *base_count += os_count;
    }
}

Package* package_new(void) {
    Package *pkg = calloc(1, sizeof(Package));
    if (!pkg) return NULL;
    return pkg;
}

void package_free(Package *pkg) {
    if (!pkg) return;

    free(pkg->name);
    free(pkg->version);
    free(pkg->description);
    free(pkg->build_system);
    free(pkg->source_type);
    free(pkg->source_url);
    free(pkg->source_branch);
    free(pkg->source_tag);
    free(pkg->source_commit);

    for (size_t i = 0; i < pkg->dependencies_count; i++) {
        free(pkg->dependencies[i]);
    }
    free(pkg->dependencies);

    for (size_t i = 0; i < pkg->build_dependencies_count; i++) {
        free(pkg->build_dependencies[i]);
    }
    free(pkg->build_dependencies);

    for (size_t i = 0; i < pkg->configure_args_count; i++) {
        free(pkg->configure_args[i]);
    }
    free(pkg->configure_args);

    for (size_t i = 0; i < pkg->cmake_args_count; i++) {
        free(pkg->cmake_args[i]);
    }
    free(pkg->cmake_args);

    for (size_t i = 0; i < pkg->make_args_count; i++) {
        free(pkg->make_args[i]);
    }
    free(pkg->make_args);

    for (size_t i = 0; i < pkg->env_count; i++) {
        free(pkg->env_keys[i]);
        free(pkg->env_values[i]);
    }
    free(pkg->env_keys);
    free(pkg->env_values);

    for (size_t i = 0; i < pkg->patches_count; i++) {
        free(pkg->patches[i]);
    }
    free(pkg->patches);

    for (size_t i = 0; i < pkg->build_commands_count; i++) {
        free(pkg->build_commands[i]);
    }
    free(pkg->build_commands);

    free(pkg);
}

bool package_load_from_file(Package *pkg, const char *filename) {
    log_developer("Loading package from file: %s", filename);
    FILE *f = fopen(filename, "r");
    if (!f) {
        log_error("Failed to open package file: %s", filename);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *json = malloc(size + 1);
    if (!json) {
        fclose(f);
        return false;
    }

    fread(json, 1, size, f);
    json[size] = '\0';
    fclose(f);

    bool result = package_load_from_json(pkg, json);
    free(json);
    if (result) {
        log_debug("Package loaded successfully from file: %s (name=%s, version=%s)",
                  filename, pkg->name ? pkg->name : "unknown", pkg->version ? pkg->version : "unknown");
    } else {
        log_error("Failed to parse package JSON from file: %s", filename);
    }
    return result;
}

bool package_load_from_json(Package *pkg, const char *json_string) {
    pkg->name = json_get_string(json_string, "name");
    pkg->version = json_get_string(json_string, "version");
    if (!pkg->version) pkg->version = strdup("latest");

    pkg->description = json_get_string(json_string, "description");
    if (!pkg->description) pkg->description = strdup("");

    pkg->build_system = json_get_string(json_string, "build_system");
    if (!pkg->build_system) pkg->build_system = strdup("autotools");

    // Source
    const char *source_start = strstr(json_string, "\"source\"");
    if (source_start) {
        pkg->source_type = json_get_string(source_start, "type");
        if (!pkg->source_type) pkg->source_type = strdup("git");
        pkg->source_url = json_get_string(source_start, "url");
        pkg->source_branch = json_get_string(source_start, "branch");
        pkg->source_tag = json_get_string(source_start, "tag");
        pkg->source_commit = json_get_string(source_start, "commit");
    } else {
        pkg->source_type = strdup("git");
    }

    // Dependencies
    pkg->dependencies = json_get_array(json_string, "dependencies", &pkg->dependencies_count);
    pkg->build_dependencies = json_get_array(json_string, "build_dependencies", &pkg->build_dependencies_count);

    // Build args
    pkg->configure_args = json_get_array(json_string, "configure_args", &pkg->configure_args_count);
    pkg->cmake_args = json_get_array(json_string, "cmake_args", &pkg->cmake_args_count);
    pkg->make_args = json_get_array(json_string, "make_args", &pkg->make_args_count);

    // Patches
    pkg->patches = json_get_array(json_string, "patches", &pkg->patches_count);

    // Build commands (for custom build system)
    pkg->build_commands = json_get_array(json_string, "build_commands", &pkg->build_commands_count);

    // Environment variables (base) - this is the DEFAULT that is always used
    // Base env is loaded first and serves as the fallback for all systems
    json_get_object(json_string, "env", &pkg->env_keys, &pkg->env_values, &pkg->env_count);

    // Get OS name and merge OS-specific configurations
    const char *os_name = package_get_os_name();
    log_debug("Detected OS: %s", os_name);

    // Merge OS-specific env into base env
    // OS-specific values override base values for matching keys
    // If OS-specific config doesn't exist, base env is used as-is (default fallback)
    merge_env_os_specific(pkg, json_string, os_name);

    // Merge OS-specific arrays (configure_args, cmake_args, make_args)
    merge_array_os_specific(&pkg->configure_args, &pkg->configure_args_count, json_string, "configure_args", os_name);
    merge_array_os_specific(&pkg->cmake_args, &pkg->cmake_args_count, json_string, "cmake_args", os_name);
    merge_array_os_specific(&pkg->make_args, &pkg->make_args_count, json_string, "make_args", os_name);

    return pkg->name != NULL;
}

bool package_has_dependency(const Package *pkg, const char *dep_name) {
    // Check regular dependencies
    for (size_t i = 0; i < pkg->dependencies_count; i++) {
        if (strcmp(pkg->dependencies[i], dep_name) == 0) {
            return true;
        }
    }
    // Check build dependencies
    for (size_t i = 0; i < pkg->build_dependencies_count; i++) {
        if (strcmp(pkg->build_dependencies[i], dep_name) == 0) {
            return true;
        }
    }
    return false;
}

void package_add_dependency(Package *pkg, const char *dep_name) {
    pkg->dependencies = realloc(pkg->dependencies, sizeof(char*) * (pkg->dependencies_count + 1));
    pkg->dependencies[pkg->dependencies_count] = strdup(dep_name);
    pkg->dependencies_count++;
}

void package_add_build_dependency(Package *pkg, const char *dep_name) {
    pkg->build_dependencies = realloc(pkg->build_dependencies, sizeof(char*) * (pkg->build_dependencies_count + 1));
    pkg->build_dependencies[pkg->build_dependencies_count] = strdup(dep_name);
    pkg->build_dependencies_count++;
}

