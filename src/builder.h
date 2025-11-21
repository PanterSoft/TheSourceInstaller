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
bool builder_build(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir);
bool builder_install(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir);
bool builder_apply_patches(const char *source_dir, char **patches, size_t patches_count);

#ifdef __cplusplus
}
#endif

#endif // BUILDER_H

