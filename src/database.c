#include "database.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

Database* database_new(const char *db_dir) {
    Database *db = calloc(1, sizeof(Database));
    if (!db) return NULL;

    db->db_path = malloc(strlen(db_dir) + 20);
    snprintf(db->db_path, strlen(db_dir) + 20, "%s/installed.json", db_dir);

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(db_dir, &st) == -1) {
        // Create directory (simple mkdir, no recursive)
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", db_dir);
        system(cmd);
    }

    database_load(db);
    return db;
}

void database_free(Database *db) {
    if (!db) return;

    free(db->db_path);

    for (size_t i = 0; i < db->packages_count; i++) {
        free(db->packages[i].name);
        free(db->packages[i].version);
        free(db->packages[i].install_path);
        for (size_t j = 0; j < db->packages[i].dependencies_count; j++) {
            free(db->packages[i].dependencies[j]);
        }
        free(db->packages[i].dependencies);
    }
    free(db->packages);
    free(db);
}

// Simple helper to skip whitespace
static void skip_whitespace(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF && (c == ' ' || c == '\t' || c == '\n' || c == '\r')) {
        // Skip
    }
    if (c != EOF) ungetc(c, f);
}

// Simple helper to read a quoted string
static char* read_quoted_string(FILE *f) {
    skip_whitespace(f);
    if (fgetc(f) != '"') return NULL;

    char *str = NULL;
    size_t len = 0;
    size_t capacity = 32;
    str = malloc(capacity);
    if (!str) return NULL;

    int c;
    while ((c = fgetc(f)) != EOF && c != '"') {
        if (c == '\\') {
            c = fgetc(f);
            if (c == EOF) break;
        }
        if (len + 1 >= capacity) {
            capacity *= 2;
            str = realloc(str, capacity);
            if (!str) return NULL;
        }
        str[len++] = c;
    }
    str[len] = '\0';
    return str;
}

// Simple helper to extract quoted string value from a line
static char* extract_string_value(const char *line, const char *key) {
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char *pos = strstr(line, pattern);
    if (!pos) return NULL;

    pos = strchr(pos, ':');
    if (!pos) return NULL;
    pos++; // Skip ':'

    // Skip whitespace
    while (*pos == ' ' || *pos == '\t') pos++;

    // Find opening quote
    if (*pos != '"') return NULL;
    pos++;

    // Find end of string
    char *end = strchr(pos, '"');
    if (!end) return NULL;

    size_t len = end - pos;
    char *result = malloc(len + 1);
    if (!result) return NULL;
    strncpy(result, pos, len);
    result[len] = '\0';
    return result;
}

bool database_load(Database *db) {
    FILE *f = fopen(db->db_path, "r");
    if (!f) {
        db->packages_count = 0;
        db->packages = NULL;
        return true; // No database yet is OK
    }

    db->packages_count = 0;
    db->packages = NULL;

    // Check if file is empty
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size == 0) {
        fclose(f);
        return true;
    }

    // Simple JSON parsing - read line by line
    char line[2048];
    bool in_installed = false;
    InstalledPackage *current_pkg = NULL;

    while (fgets(line, sizeof(line), f)) {
        // Remove trailing newline and carriage return
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[len-1] = '\0';
            len--;
        }

        // Skip empty lines
        if (len == 0) continue;

        // Check if we're in the installed array
        if (strstr(line, "\"installed\"")) {
            in_installed = true;
            continue;
        }

        // Skip lines before installed array
        if (!in_installed) continue;

        // Skip the opening bracket line
        if (strchr(line, '[') && !strchr(line, '{')) {
            continue;
        }

        // Start of package object - check for opening brace (may have whitespace)
        char *brace_pos = strchr(line, '{');
        if (brace_pos && !current_pkg) {
            db->packages = realloc(db->packages, sizeof(InstalledPackage) * (db->packages_count + 1));
            current_pkg = &db->packages[db->packages_count];
            current_pkg->name = NULL;
            current_pkg->version = NULL;
            current_pkg->install_path = NULL;
            current_pkg->installed_at = 0;
            current_pkg->dependencies = NULL;
            current_pkg->dependencies_count = 0;
            continue;
        }

        // End of package object - check for closing brace (may have whitespace)
        char *close_brace = strchr(line, '}');
        if (close_brace && current_pkg) {
            // Only increment if we have at least a name
            if (current_pkg->name) {
                db->packages_count++;
            } else {
                // Free incomplete package
                if (current_pkg->version) free(current_pkg->version);
                if (current_pkg->install_path) free(current_pkg->install_path);
                if (current_pkg->dependencies) {
                    for (size_t i = 0; i < current_pkg->dependencies_count; i++) {
                        if (current_pkg->dependencies[i]) free(current_pkg->dependencies[i]);
                    }
                    free(current_pkg->dependencies);
                }
            }
            current_pkg = NULL;
            continue;
        }

        // End of installed array - check for closing bracket
        if (strchr(line, ']')) {
            break;
        }

        if (current_pkg) {
            // Parse fields
            char *value;
            if ((value = extract_string_value(line, "name"))) {
                current_pkg->name = value;
            } else if ((value = extract_string_value(line, "version"))) {
                current_pkg->version = value;
            } else if ((value = extract_string_value(line, "install_path"))) {
                current_pkg->install_path = value;
            } else if (strstr(line, "\"installed_at\"")) {
                // Parse timestamp
                char *p = strstr(line, ":");
                if (p) {
                    // Skip whitespace after colon
                    p++;
                    while (*p == ' ' || *p == '\t') p++;
                    current_pkg->installed_at = (time_t)strtol(p, NULL, 10);
                }
            } else if (strstr(line, "\"dependencies\"")) {
                // Parse dependencies array - simple extraction
                char *start = strstr(line, "[");
                if (start) {
                    start++; // Skip '['
                    char *end = strstr(start, "]");
                    if (end) {
                        char saved = *end;
                        *end = '\0'; // Temporarily terminate at ']'

                        // Count dependencies
                        size_t deps_capacity = 8;
                        current_pkg->dependencies = malloc(sizeof(char*) * deps_capacity);
                        current_pkg->dependencies_count = 0;

                        // Parse comma-separated quoted strings
                        char *p = start;
                        while (*p) {
                            // Skip whitespace and commas
                            while (*p == ' ' || *p == '\t' || *p == ',') p++;
                            if (!*p) break;

                            // Find quoted string
                            if (*p == '"') {
                                p++; // Skip opening quote
                                char *dep_start = p;
                                while (*p && *p != '"') p++;
                                if (*p == '"') {
                                    size_t dep_len = p - dep_start;
                                    char *dep = malloc(dep_len + 1);
                                    if (dep) {
                                        strncpy(dep, dep_start, dep_len);
                                        dep[dep_len] = '\0';

                                        if (current_pkg->dependencies_count >= deps_capacity) {
                                            deps_capacity *= 2;
                                            current_pkg->dependencies = realloc(current_pkg->dependencies, sizeof(char*) * deps_capacity);
                                        }
                                        current_pkg->dependencies[current_pkg->dependencies_count++] = dep;
                                    }
                                    p++; // Skip closing quote
                                }
                            } else {
                                break;
                            }
                        }
                        *end = saved; // Restore character
                    }
                }
            }
        }
    }

    fclose(f);
    return true;
}

bool database_save(Database *db) {
    FILE *f = fopen(db->db_path, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"installed\": [\n");

    for (size_t i = 0; i < db->packages_count; i++) {
        InstalledPackage *pkg = &db->packages[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", pkg->name);
        fprintf(f, "      \"version\": \"%s\",\n", pkg->version);
        fprintf(f, "      \"install_path\": \"%s\",\n", pkg->install_path);
        fprintf(f, "      \"installed_at\": %ld,\n", (long)pkg->installed_at);
        fprintf(f, "      \"dependencies\": [");
        for (size_t j = 0; j < pkg->dependencies_count; j++) {
            fprintf(f, "\"%s\"", pkg->dependencies[j]);
            if (j < pkg->dependencies_count - 1) fprintf(f, ", ");
        }
        fprintf(f, "]\n");
        fprintf(f, "    }");
        if (i < db->packages_count - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}

bool database_is_installed(const Database *db, const char *package_name) {
    for (size_t i = 0; i < db->packages_count; i++) {
        if (strcmp(db->packages[i].name, package_name) == 0) {
            return true;
        }
    }
    return false;
}

bool database_add_package(Database *db, const char *name, const char *version, const char *install_path, const char **deps, size_t deps_count) {
    // Check if already exists
    if (database_is_installed(db, name)) {
        return false;
    }

    db->packages = realloc(db->packages, sizeof(InstalledPackage) * (db->packages_count + 1));
    InstalledPackage *pkg = &db->packages[db->packages_count];

    pkg->name = strdup(name);
    pkg->version = strdup(version ? version : "unknown");
    pkg->install_path = strdup(install_path ? install_path : "");
    pkg->installed_at = time(NULL);
    pkg->dependencies_count = deps_count;
    pkg->dependencies = NULL;

    if (deps_count > 0) {
        pkg->dependencies = malloc(sizeof(char*) * deps_count);
        for (size_t i = 0; i < deps_count; i++) {
            pkg->dependencies[i] = strdup(deps[i]);
        }
    }

    db->packages_count++;
    return database_save(db);
}

bool database_remove_package(Database *db, const char *package_name) {
    for (size_t i = 0; i < db->packages_count; i++) {
        if (strcmp(db->packages[i].name, package_name) == 0) {
            // Free package data
            free(db->packages[i].name);
            free(db->packages[i].version);
            free(db->packages[i].install_path);
            for (size_t j = 0; j < db->packages[i].dependencies_count; j++) {
                free(db->packages[i].dependencies[j]);
            }
            free(db->packages[i].dependencies);

            // Move remaining packages
            if (i < db->packages_count - 1) {
                memmove(&db->packages[i], &db->packages[i + 1],
                       sizeof(InstalledPackage) * (db->packages_count - i - 1));
            }

            db->packages_count--;
            db->packages = realloc(db->packages, sizeof(InstalledPackage) * db->packages_count);
            return database_save(db);
        }
    }
    return false;
}

InstalledPackage* database_get_package(const Database *db, const char *package_name) {
    for (size_t i = 0; i < db->packages_count; i++) {
        if (strcmp(db->packages[i].name, package_name) == 0) {
            return &db->packages[i];
        }
    }
    return NULL;
}

char** database_list_installed(const Database *db, size_t *count) {
    *count = db->packages_count;
    if (db->packages_count == 0) return NULL;

    char **list = malloc(sizeof(char*) * db->packages_count);
    for (size_t i = 0; i < db->packages_count; i++) {
        list[i] = strdup(db->packages[i].name);
    }
    return list;
}

