#include "package.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

    free(pkg);
}

bool package_load_from_file(Package *pkg, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return false;

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

    return pkg->name != NULL;
}

bool package_has_dependency(const Package *pkg, const char *dep_name) {
    for (size_t i = 0; i < pkg->dependencies_count; i++) {
        if (strcmp(pkg->dependencies[i], dep_name) == 0) {
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

