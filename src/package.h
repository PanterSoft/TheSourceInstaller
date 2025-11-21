#ifndef PACKAGE_H
#define PACKAGE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Package structure
typedef struct {
    char *name;
    char *version;
    char *description;
    char *build_system;

    // Source information
    char *source_type;
    char *source_url;
    char *source_branch;
    char *source_tag;
    char *source_commit;

    // Dependencies
    char **dependencies;
    size_t dependencies_count;
    char **build_dependencies;
    size_t build_dependencies_count;

    // Build configuration
    char **configure_args;
    size_t configure_args_count;
    char **cmake_args;
    size_t cmake_args_count;
    char **make_args;
    size_t make_args_count;

    // Environment variables
    char **env_keys;
    char **env_values;
    size_t env_count;

    // Patches
    char **patches;
    size_t patches_count;
} Package;

// Package functions
Package* package_new(void);
void package_free(Package *pkg);
bool package_load_from_file(Package *pkg, const char *filename);
bool package_load_from_json(Package *pkg, const char *json_string);
char* package_to_json(const Package *pkg);
bool package_has_dependency(const Package *pkg, const char *dep_name);
void package_add_dependency(Package *pkg, const char *dep_name);
void package_add_build_dependency(Package *pkg, const char *dep_name);

#ifdef __cplusplus
}
#endif

#endif // PACKAGE_H

