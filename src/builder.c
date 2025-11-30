#include "builder.h"
#include "package.h"
#include "tui.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

BuilderConfig* builder_config_new(const char *prefix) {
    log_developer("builder_config_new called with prefix='%s'", prefix);
    BuilderConfig *config = malloc(sizeof(BuilderConfig));
    if (!config) {
        log_error("Failed to allocate memory for BuilderConfig");
        return NULL;
    }

    config->prefix = strdup(prefix);

    char install_dir[512];
    snprintf(install_dir, sizeof(install_dir), "%s/install", prefix);
    config->install_dir = strdup(install_dir);

    char build_dir[512];
    snprintf(build_dir, sizeof(build_dir), "%s/build", prefix);
    config->build_dir = strdup(build_dir);

    log_debug("BuilderConfig initialized: prefix=%s, install_dir=%s, build_dir=%s",
              prefix, config->install_dir, config->build_dir);
    return config;
}

void builder_config_free(BuilderConfig *config) {
    if (!config) return;
    free(config->install_dir);
    free(config->build_dir);
    free(config->prefix);
    free(config);
}

void builder_config_set_package_dir(BuilderConfig *config, const char *package_name, const char *package_version) {
    if (!config || !package_name) return;

    // Free old install_dir
    if (config->install_dir) {
        free(config->install_dir);
    }

    // Create package-specific directory: ~/.tsi/install/package-version/
    char package_dir[1024];
    if (package_version && strlen(package_version) > 0) {
        snprintf(package_dir, sizeof(package_dir), "%s/install/%s-%s", config->prefix, package_name, package_version);
    } else {
        snprintf(package_dir, sizeof(package_dir), "%s/install/%s", config->prefix, package_name);
    }
    config->install_dir = strdup(package_dir);
}

bool builder_apply_patches(const char *source_dir, char **patches, size_t patches_count) {
    for (size_t i = 0; i < patches_count; i++) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "cd '%s' && patch -p1 -i '%s' 2>/dev/null", source_dir, patches[i]);
        if (system(cmd) != 0) {
            char warn_msg[512];
            snprintf(warn_msg, sizeof(warn_msg), "Failed to apply patch: %s", patches[i]);
            print_warning(warn_msg);
        }
    }
    return true;
}

bool builder_build(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir) {
    if (!config || !pkg || !source_dir) {
        log_error("builder_build called with invalid parameters");
        return false;
    }

    log_info("Building package: %s@%s (source_dir=%s, build_dir=%s)",
             pkg->name, pkg->version ? pkg->version : "latest", source_dir, build_dir);

    // Create build directory
    log_developer("Creating build directory: %s", build_dir);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", build_dir);
    system(cmd);

    // Apply patches
    if (pkg->patches_count > 0) {
        log_debug("Applying %zu patches to source", pkg->patches_count);
        builder_apply_patches(source_dir, pkg->patches, pkg->patches_count);
    }

    // Set up environment - use main install directory (parent of package-specific dir) for PATH
    // Package dir is like ~/.tsi/install/package-version, main install dir is ~/.tsi/install
    char main_install_dir[1024];
    char *last_slash = strrchr(config->install_dir, '/');
    if (last_slash) {
        // Check if this is a package-specific directory (contains package name)
        // If install_dir ends with /install, it's already the main dir
        if (strstr(config->install_dir, "/install/") != NULL) {
            // Package-specific: ~/.tsi/install/package-version -> ~/.tsi/install
            size_t len = strstr(config->install_dir, "/install/") - config->install_dir + strlen("/install");
            strncpy(main_install_dir, config->install_dir, len);
            main_install_dir[len] = '\0';
        } else {
            // Already main install directory
            strncpy(main_install_dir, config->install_dir, sizeof(main_install_dir) - 1);
            main_install_dir[sizeof(main_install_dir) - 1] = '\0';
        }
    } else {
        strncpy(main_install_dir, config->install_dir, sizeof(main_install_dir) - 1);
        main_install_dir[sizeof(main_install_dir) - 1] = '\0';
    }

    char env[4096] = "";
    // Only use TSI-installed packages and tools - no system packages
    // PATH only includes TSI's bin directory (build tools like make, gcc, sed must be installed via TSI first)
    // Restrict all paths to only TSI to ensure complete isolation from system packages
    snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
             main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);

    const char *build_system = pkg->build_system ? pkg->build_system : "autotools";
    log_debug("Using build system: %s", build_system);
    log_developer("Build environment: %s", env);

    if (strcmp(build_system, "autotools") == 0) {
        // Check for configure script
        char configure[512];
        snprintf(configure, sizeof(configure), "%s/configure", source_dir);
        struct stat st;
        if (stat(configure, &st) != 0) {
            log_debug("Configure script not found, running autoreconf");
            // Try to generate configure
            snprintf(cmd, sizeof(cmd), "cd '%s' && autoreconf -fiv 2>/dev/null", source_dir);
            system(cmd);
        }

        // Configure
        log_debug("Running configure for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s ./configure --prefix='%s' >/dev/null 2>&1", source_dir, env, config->install_dir);
        for (size_t i = 0; i < pkg->configure_args_count; i++) {
            strcat(cmd, " ");
            strcat(cmd, pkg->configure_args[i]);
        }
        log_developer("Configure command: %s", cmd);
        if (system(cmd) != 0) {
            log_error("Configure failed for package: %s", pkg->name);
            return false;
        }
        log_debug("Configure completed successfully for package: %s", pkg->name);

        // Make
        log_debug("Running make for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make >/dev/null 2>&1", source_dir, env);
        for (size_t i = 0; i < pkg->make_args_count; i++) {
            strcat(cmd, " ");
            strcat(cmd, pkg->make_args[i]);
        }
        log_developer("Make command: %s", cmd);
        if (system(cmd) != 0) {
            log_error("Make failed for package: %s", pkg->name);
            return false;
        }
        log_debug("Make completed successfully for package: %s", pkg->name);

    } else if (strcmp(build_system, "cmake") == 0) {
        // CMake configure
        size_t cmd_len = 1024;
        char *cmd = malloc(cmd_len);
        snprintf(cmd, cmd_len, "cd '%s' && %s cmake -S '%s' -B '%s' -DCMAKE_INSTALL_PREFIX='%s' >/dev/null 2>&1",
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
        snprintf(cmd, cmd_len, "cd '%s' && %s cmake --build '%s' >/dev/null 2>&1", build_dir, env, build_dir);
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
        snprintf(cmd, cmd_len, "cd '%s' && %s make >/dev/null 2>&1", source_dir, env);
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
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson setup '%s' '%s' --prefix='%s' >/dev/null 2>&1",
                 build_dir, env, build_dir, source_dir, config->install_dir);
        if (system(cmd) != 0) {
            return false;
        }

        // Meson compile
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson compile -C '%s' >/dev/null 2>&1", build_dir, env, build_dir);
        if (system(cmd) != 0) {
            return false;
        }
    } else if (strcmp(build_system, "custom") == 0) {
        // Custom build commands
        if (pkg->build_commands_count > 0) {
            // Expand environment variables in commands
            char expanded_env[4096];
            snprintf(expanded_env, sizeof(expanded_env), "%s TSI_INSTALL_DIR='%s'", env, config->install_dir);

            for (size_t i = 0; i < pkg->build_commands_count; i++) {
                // Replace $TSI_INSTALL_DIR in command
                char *cmd_expanded = strdup(pkg->build_commands[i]);
                if (!cmd_expanded) continue;

                // Simple variable substitution
                char *tsi_var = strstr(cmd_expanded, "$TSI_INSTALL_DIR");
                if (tsi_var) {
                    size_t prefix_len = tsi_var - cmd_expanded;
                    size_t suffix_len = strlen(tsi_var + strlen("$TSI_INSTALL_DIR"));
                    size_t new_len = prefix_len + strlen(config->install_dir) + suffix_len + 1;
                    char *new_cmd = malloc(new_len);
                    if (new_cmd) {
                        memcpy(new_cmd, cmd_expanded, prefix_len);
                        memcpy(new_cmd + prefix_len, config->install_dir, strlen(config->install_dir));
                        memcpy(new_cmd + prefix_len + strlen(config->install_dir),
                               tsi_var + strlen("$TSI_INSTALL_DIR"), suffix_len);
                        new_cmd[new_len - 1] = '\0';
                        free(cmd_expanded);
                        cmd_expanded = new_cmd;
                    }
                }

                // Execute command in source directory
                size_t cmd_len = strlen(cmd_expanded) + strlen(source_dir) + strlen(expanded_env) + 64;
                char *full_cmd = malloc(cmd_len);
                if (full_cmd) {
                    snprintf(full_cmd, cmd_len, "cd '%s' && %s %s >/dev/null 2>&1",
                            source_dir, expanded_env, cmd_expanded);
                    int result = system(full_cmd);
                    free(full_cmd);
                    free(cmd_expanded);
                    if (result != 0) {
                        // Command failed - return error
                        return false;
                    }
                } else {
                    free(cmd_expanded);
                    return false;
                }
            }
            // All commands succeeded
            return true;
        } else {
            // No build commands specified, just return success
            return true;
        }
    }

    return true;
}

bool builder_install(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir) {
    if (!config || !pkg || !source_dir) {
        log_error("builder_install called with invalid parameters");
        return false;
    }

    log_info("Installing package: %s@%s (install_dir=%s)",
             pkg->name, pkg->version ? pkg->version : "latest", config->install_dir);

    // Set up environment - use main install directory for PATH
    // Package dir is like ~/.tsi/install/package-version, main install dir is ~/.tsi/install
    char main_install_dir[1024];
    char *install_pos = strstr(config->install_dir, "/install/");
    if (install_pos) {
        // Package-specific: ~/.tsi/install/package-version -> ~/.tsi/install
        size_t len = install_pos - config->install_dir + strlen("/install");
        strncpy(main_install_dir, config->install_dir, len);
        main_install_dir[len] = '\0';
    } else {
        // Already main install directory
        strncpy(main_install_dir, config->install_dir, sizeof(main_install_dir) - 1);
        main_install_dir[sizeof(main_install_dir) - 1] = '\0';
    }

    char env[4096] = "";
    // Only use TSI-installed packages and tools - no system packages
    // PATH only includes TSI's bin directory (build tools must be installed via TSI first)
    // Restrict all paths to only TSI to ensure complete isolation from system packages
    snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib",
             main_install_dir, main_install_dir, main_install_dir);

    const char *build_system = pkg->build_system ? pkg->build_system : "autotools";
    log_debug("Using build system for install: %s", build_system);
    log_developer("Install environment: %s", env);
    char cmd[1024];

    if (strcmp(build_system, "autotools") == 0) {
        log_debug("Running make install for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make install >/dev/null 2>&1", source_dir, env);
        log_developer("Install command: %s", cmd);
    } else if (strcmp(build_system, "cmake") == 0) {
        log_debug("Running cmake --install for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s cmake --install '%s' >/dev/null 2>&1", build_dir, env, build_dir);
        log_developer("Install command: %s", cmd);
    } else if (strcmp(build_system, "meson") == 0) {
        log_debug("Running meson install for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson install -C '%s' >/dev/null 2>&1", build_dir, env, build_dir);
        log_developer("Install command: %s", cmd);
    } else if (strcmp(build_system, "make") == 0) {
        log_debug("Running make install for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make install PREFIX='%s' >/dev/null 2>&1", source_dir, env, config->install_dir);
        log_developer("Install command: %s", cmd);
    } else if (strcmp(build_system, "custom") == 0) {
        log_debug("Using custom install method for package: %s", pkg->name);
        // For custom builds, installation is typically handled in build_commands
        // But we can try to copy common directories if they exist
        char install_cmd[2048];
        snprintf(install_cmd, sizeof(install_cmd),
                "mkdir -p '%s' && "
                "(cp -r '%s'/bin '%s'/ 2>/dev/null || true) && "
                "(cp -r '%s'/lib '%s'/ 2>/dev/null || true) && "
                "(cp -r '%s'/include '%s'/ 2>/dev/null || true) && "
                "(cp -r '%s'/share '%s'/ 2>/dev/null || true)",
                config->install_dir,
                source_dir, config->install_dir,
                source_dir, config->install_dir,
                source_dir, config->install_dir,
                source_dir, config->install_dir);
        log_developer("Custom install command: %s", install_cmd);
        system(install_cmd);
        log_info("Custom install completed for package: %s", pkg->name);
        return true; // Custom builds might handle installation themselves
    } else {
        log_error("Unknown build system for install: %s", build_system);
        return false;
    }

    int result = system(cmd);
    if (result == 0) {
        log_info("Install completed successfully for package: %s", pkg->name);
    } else {
        log_error("Install failed for package: %s (exit code: %d)", pkg->name, result);
    }
    return result == 0;
}

bool builder_create_symlinks(const BuilderConfig *config, const char *package_name, const char *package_version) {
    (void)package_version; // May be used in future for version-specific symlinks
    if (!config || !package_name) return false;

    // Get the main install directory (parent of package-specific directory)
    char main_install_dir[1024];
    char *last_slash = strrchr(config->install_dir, '/');
    if (last_slash) {
        size_t len = last_slash - config->install_dir;
        strncpy(main_install_dir, config->install_dir, len);
        main_install_dir[len] = '\0';
    } else {
        return false;
    }

    // Create main install directories if they don't exist
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s/bin' '%s/lib' '%s/include' '%s/share'",
             main_install_dir, main_install_dir, main_install_dir, main_install_dir);
    system(cmd);

    // Create symlinks for binaries
    char package_bin[1024];
    snprintf(package_bin, sizeof(package_bin), "%s/bin", config->install_dir);
    struct stat st;
    if (stat(package_bin, &st) == 0 && S_ISDIR(st.st_mode)) {
        // Find all binaries in package bin directory and symlink them
        snprintf(cmd, sizeof(cmd),
                 "for f in '%s'/*; do "
                 "if [ -f \"$f\" ] && [ -x \"$f\" ]; then "
                 "ln -sf \"$f\" '%s/bin/$(basename \"$f\")\" 2>/dev/null; "
                 "fi; "
                 "done",
                 package_bin, main_install_dir);
        system(cmd);
    }

    // Create symlinks for libraries
    char package_lib[1024];
    snprintf(package_lib, sizeof(package_lib), "%s/lib", config->install_dir);
    if (stat(package_lib, &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(cmd, sizeof(cmd),
                 "for f in '%s'/*; do "
                 "if [ -f \"$f\" ]; then "
                 "ln -sf \"$f\" '%s/lib/$(basename \"$f\")\" 2>/dev/null; "
                 "fi; "
                 "done",
                 package_lib, main_install_dir);
        system(cmd);
    }

    // Create symlinks for include files
    char package_include[1024];
    snprintf(package_include, sizeof(package_include), "%s/include", config->install_dir);
    if (stat(package_include, &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(cmd, sizeof(cmd),
                 "for f in '%s'/*; do "
                 "if [ -f \"$f\" ] || [ -d \"$f\" ]; then "
                 "ln -sf \"$f\" '%s/include/$(basename \"$f\")\" 2>/dev/null; "
                 "fi; "
                 "done",
                 package_include, main_install_dir);
        system(cmd);
    }

    return true;
}

