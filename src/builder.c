#include "builder.h"
#include "package.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

BuilderConfig* builder_config_new(const char *prefix) {
    BuilderConfig *config = malloc(sizeof(BuilderConfig));
    if (!config) return NULL;

    config->prefix = strdup(prefix);

    char install_dir[512];
    snprintf(install_dir, sizeof(install_dir), "%s/install", prefix);
    config->install_dir = strdup(install_dir);

    char build_dir[512];
    snprintf(build_dir, sizeof(build_dir), "%s/build", prefix);
    config->build_dir = strdup(build_dir);

    return config;
}

void builder_config_free(BuilderConfig *config) {
    if (!config) return;
    free(config->install_dir);
    free(config->build_dir);
    free(config->prefix);
    free(config);
}

bool builder_apply_patches(const char *source_dir, char **patches, size_t patches_count) {
    for (size_t i = 0; i < patches_count; i++) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "cd '%s' && patch -p1 -i '%s' 2>/dev/null", source_dir, patches[i]);
        if (system(cmd) != 0) {
            printf("Warning: Failed to apply patch: %s\n", patches[i]);
        }
    }
    return true;
}

bool builder_build(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir) {
    if (!config || !pkg || !source_dir) {
        return false;
    }

    // Create build directory
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", build_dir);
    system(cmd);

    // Apply patches
    if (pkg->patches_count > 0) {
        builder_apply_patches(source_dir, pkg->patches, pkg->patches_count);
    }

    // Set up environment
    char env[4096] = "";
    snprintf(env, sizeof(env), "PATH=%s/bin:$PATH PKG_CONFIG_PATH=%s/lib/pkgconfig:$PKG_CONFIG_PATH LD_LIBRARY_PATH=%s/lib:$LD_LIBRARY_PATH CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
             config->install_dir, config->install_dir, config->install_dir, config->install_dir, config->install_dir);

    const char *build_system = pkg->build_system ? pkg->build_system : "autotools";

    if (strcmp(build_system, "autotools") == 0) {
        // Check for configure script
        char configure[512];
        snprintf(configure, sizeof(configure), "%s/configure", source_dir);
        struct stat st;
        if (stat(configure, &st) != 0) {
            // Try to generate configure
            snprintf(cmd, sizeof(cmd), "cd '%s' && autoreconf -fiv 2>/dev/null", source_dir);
            system(cmd);
        }

        // Configure
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s ./configure --prefix='%s'", source_dir, env, config->install_dir);
        for (size_t i = 0; i < pkg->configure_args_count; i++) {
            strcat(cmd, " ");
            strcat(cmd, pkg->configure_args[i]);
        }
        if (system(cmd) != 0) {
            return false;
        }

        // Make
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make", source_dir, env);
        for (size_t i = 0; i < pkg->make_args_count; i++) {
            strcat(cmd, " ");
            strcat(cmd, pkg->make_args[i]);
        }
        if (system(cmd) != 0) {
            return false;
        }

    } else if (strcmp(build_system, "cmake") == 0) {
        // CMake configure
        size_t cmd_len = 1024;
        char *cmd = malloc(cmd_len);
        snprintf(cmd, cmd_len, "cd '%s' && %s cmake -S '%s' -B '%s' -DCMAKE_INSTALL_PREFIX='%s'",
                 build_dir, env, source_dir, build_dir, config->install_dir);
        for (size_t i = 0; i < pkg->cmake_args_count; i++) {
            size_t needed = strlen(cmd) + strlen(pkg->cmake_args[i]) + 2;
            if (needed > cmd_len) {
                cmd_len = needed * 2;
                cmd = realloc(cmd, cmd_len);
            }
            strcat(cmd, " ");
            strcat(cmd, pkg->cmake_args[i]);
        }
        int result = system(cmd);
        free(cmd);
        if (result != 0) {
            return false;
        }

        // CMake build
        cmd_len = 1024;
        cmd = malloc(cmd_len);
        snprintf(cmd, cmd_len, "cd '%s' && %s cmake --build '%s'", build_dir, env, build_dir);
        for (size_t i = 0; i < pkg->make_args_count; i++) {
            size_t needed = strlen(cmd) + strlen(pkg->make_args[i]) + 2;
            if (needed > cmd_len) {
                cmd_len = needed * 2;
                cmd = realloc(cmd, cmd_len);
            }
            strcat(cmd, " ");
            strcat(cmd, pkg->make_args[i]);
        }
        result = system(cmd);
        free(cmd);
        if (result != 0) {
            return false;
        }

    } else if (strcmp(build_system, "make") == 0) {
        // Plain Makefile
        size_t cmd_len = 1024;
        char *cmd = malloc(cmd_len);
        snprintf(cmd, cmd_len, "cd '%s' && %s make", source_dir, env);
        for (size_t i = 0; i < pkg->make_args_count; i++) {
            size_t needed = strlen(cmd) + strlen(pkg->make_args[i]) + 2;
            if (needed > cmd_len) {
                cmd_len = needed * 2;
                cmd = realloc(cmd, cmd_len);
            }
            strcat(cmd, " ");
            strcat(cmd, pkg->make_args[i]);
        }
        int result = system(cmd);
        free(cmd);
        if (result != 0) {
            return false;
        }

    } else if (strcmp(build_system, "meson") == 0) {
        // Meson setup
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson setup '%s' '%s' --prefix='%s'",
                 build_dir, env, build_dir, source_dir, config->install_dir);
        if (system(cmd) != 0) {
            return false;
        }

        // Meson compile
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson compile -C '%s'", build_dir, env, build_dir);
        if (system(cmd) != 0) {
            return false;
        }
    }

    return true;
}

bool builder_install(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir) {
    if (!config || !pkg || !source_dir) {
        return false;
    }

    char env[4096] = "";
    snprintf(env, sizeof(env), "PATH=%s/bin:$PATH PKG_CONFIG_PATH=%s/lib/pkgconfig:$PKG_CONFIG_PATH LD_LIBRARY_PATH=%s/lib:$LD_LIBRARY_PATH",
             config->install_dir, config->install_dir, config->install_dir);

    const char *build_system = pkg->build_system ? pkg->build_system : "autotools";
    char cmd[1024];

    if (strcmp(build_system, "autotools") == 0) {
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make install", source_dir, env);
    } else if (strcmp(build_system, "cmake") == 0) {
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s cmake --install '%s'", build_dir, env, build_dir);
    } else if (strcmp(build_system, "meson") == 0) {
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson install -C '%s'", build_dir, env, build_dir);
    } else if (strcmp(build_system, "make") == 0) {
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make install PREFIX='%s'", source_dir, env, config->install_dir);
    } else {
        return false;
    }

    return system(cmd) == 0;
}

