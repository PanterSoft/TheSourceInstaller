#ifndef BUILDER_H
#define BUILDER_H

#include <stdbool.h>
#include "package.h"

#ifdef __cplusplus
extern "C" {
#endif

// Builder configuration
typedef struct {
    char *install_dir;
    char *build_dir;
    char *prefix;
} BuilderConfig;

// Builder functions
BuilderConfig* builder_config_new(const char *prefix);
void builder_config_free(BuilderConfig *config);
void builder_config_set_package_dir(BuilderConfig *config, const char *package_name, const char *package_version);
bool builder_build(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir);
bool builder_build_with_output(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir, void (*output_callback)(const char *line, void *userdata), void *userdata);
bool builder_install(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir);
bool builder_install_with_output(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir, void (*output_callback)(const char *line, void *userdata), void *userdata);
bool builder_create_symlinks(const BuilderConfig *config, const char *package_name, const char *package_version);
bool builder_apply_patches(const char *source_dir, char **patches, size_t patches_count);

// Create ls wrapper script for BusyBox systems
// Returns true if wrapper was created successfully, false otherwise
// Sets *wrapper_exists to true if wrapper exists after this function
bool builder_create_ls_wrapper(const char *tsi_bin_dir, const char *coreutils_ls_path, bool *wrapper_exists);

#ifdef __cplusplus
}
#endif

#endif // BUILDER_H

