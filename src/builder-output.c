#include "builder.h"
#include "package.h"
#include "log.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

// Helper function to get C compiler directory
// Returns the directory containing the C compiler (gcc, clang, or cc)
static void get_compiler_dir(char *compiler_dir, size_t compiler_dir_size) {
    if (!compiler_dir || compiler_dir_size == 0) {
        return;
    }

    compiler_dir[0] = '\0';

    // Find C compiler location (gcc, clang, or cc)
    const char *compilers[] = {"gcc", "clang", "cc"};
    char compiler_path[512] = "";

    for (size_t i = 0; i < sizeof(compilers) / sizeof(compilers[0]); i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "which %s 2>/dev/null", compilers[i]);
        FILE *pipe = popen(cmd, "r");
        if (pipe) {
            if (fgets(compiler_path, sizeof(compiler_path), pipe)) {
                // Remove newline
                size_t len = strlen(compiler_path);
                if (len > 0 && compiler_path[len - 1] == '\n') {
                    compiler_path[len - 1] = '\0';
                }
                // Extract directory
                char *last_slash = strrchr(compiler_path, '/');
                if (last_slash) {
                    *last_slash = '\0';
                    if (strlen(compiler_path) > 0) {
                        strncpy(compiler_dir, compiler_path, compiler_dir_size - 1);
                        compiler_dir[compiler_dir_size - 1] = '\0';
                        log_developer("Found C compiler (%s) in: %s", compilers[i], compiler_path);
                        break;
                    }
                }
            }
            pclose(pipe);
        }
    }
}

// Helper function to get minimal bootstrap PATH (only essential system tools)
// This is ONLY used for building minimal bootstrap packages (make, coreutils, sed)
// Only includes tools that are typically available on a minimal system with just a C compiler:
// - C compiler location (gcc, clang, or cc)
// - /bin (for basic POSIX shell and minimal utilities)
// Does NOT include /usr/local/bin (user-installed, not system-provided)
static void get_bootstrap_path(char *bootstrap_path, size_t bootstrap_size) {
    if (!bootstrap_path || bootstrap_size == 0) {
        return;
    }

    bootstrap_path[0] = '\0';

    // Get C compiler directory
    char compiler_dir[512] = "";
    get_compiler_dir(compiler_dir, sizeof(compiler_dir));
    if (strlen(compiler_dir) > 0) {
        strncpy(bootstrap_path, compiler_dir, bootstrap_size - 1);
        bootstrap_path[bootstrap_size - 1] = '\0';
    }

    // Add /bin for basic POSIX shell (sh) and minimal utilities
    // This is typically provided by the system, not user-installed
    struct stat st;
    if (stat("/bin", &st) == 0 && S_ISDIR(st.st_mode)) {
        if (strlen(bootstrap_path) > 0) {
            strncat(bootstrap_path, ":/bin", bootstrap_size - strlen(bootstrap_path) - 1);
        } else {
            strncpy(bootstrap_path, "/bin", bootstrap_size - 1);
            bootstrap_path[bootstrap_size - 1] = '\0';
        }
    }

    log_developer("Bootstrap PATH (C compiler + /bin only): %s", bootstrap_path);
}

// Helper function to execute command and capture output line by line
static bool execute_with_output(const char *cmd, const char *step_name, const char *package_name, void (*output_callback)(const char *line, void *userdata), void *userdata) {
    log_developer("Executing %s command for package: %s", step_name ? step_name : "build", package_name);
    log_developer("Command: %s", cmd);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        log_error("Failed to open pipe for %s command (errno: %d): %s", step_name ? step_name : "build", errno, cmd);
        return false;
    }

    char buffer[1024];
    char line[1024];
    size_t line_pos = 0;

    // Set line buffering for immediate output
    setvbuf(pipe, NULL, _IOLBF, 0);

    // Track output for logging (especially errors)
    char error_output[8192] = "";
    size_t error_output_len = 0;
    size_t line_count = 0;
    const size_t max_error_lines = 50;  // Limit error output to last 50 lines

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        // Process buffer character by character to handle partial lines
        for (size_t i = 0; buffer[i] != '\0'; i++) {
            if (buffer[i] == '\n' || buffer[i] == '\r') {
                if (line_pos > 0) {
                    line[line_pos] = '\0';
                    line_count++;

                    // Log each line at DEBUG level
                    log_debug("%s output: %s", step_name ? step_name : "build", line);

                    // Keep last max_error_lines for error logging
                    if (line_count > max_error_lines) {
                        // Remove first line from error_output buffer
                        char *first_newline = strchr(error_output, '\n');
                        if (first_newline) {
                            size_t remaining = strlen(first_newline + 1);
                            memmove(error_output, first_newline + 1, remaining + 1);
                            error_output_len = remaining;
                        } else {
                            error_output[0] = '\0';
                            error_output_len = 0;
                        }
                    }

                    // Append to error output buffer
                    size_t line_len = strlen(line);
                    if (error_output_len + line_len + 2 < sizeof(error_output)) {
                        if (error_output_len > 0) {
                            error_output[error_output_len++] = '\n';
                        }
                        memcpy(error_output + error_output_len, line, line_len);
                        error_output_len += line_len;
                        error_output[error_output_len] = '\0';
                    }

                    // Only call callback for non-empty lines
                    if (line_pos > 0 && output_callback) {
                        output_callback(line, userdata);
                    }
                    line_pos = 0;
                }
            } else if (line_pos < sizeof(line) - 1) {
                line[line_pos++] = buffer[i];
            }
        }
    }

    // Handle last line if no newline
    if (line_pos > 0) {
        line[line_pos] = '\0';
        if (output_callback) {
            output_callback(line, userdata);
        }
    }

    int status = pclose(pipe);
    int exit_code;
    bool success = false;

    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
        success = (exit_code == 0);
        if (success) {
            log_debug("%s completed successfully for package: %s (exit code: %d)", step_name ? step_name : "build", package_name, exit_code);
        } else {
            log_error("%s failed for package: %s (exit code: %d)", step_name ? step_name : "build", package_name, exit_code);
            // Log error output if available
            if (error_output_len > 0) {
                log_error("Error output from %s:", step_name ? step_name : "build");
                // Log error output line by line
                char *error_line = error_output;
                size_t logged_lines = 0;
                while (error_line && *error_line && logged_lines < max_error_lines) {
                    char *newline = strchr(error_line, '\n');
                    if (newline) {
                        *newline = '\0';
                    }
                    if (strlen(error_line) > 0) {
                        log_error("  %s", error_line);
                        logged_lines++;
                    }
                    if (newline) {
                        *newline = '\n';
                        error_line = newline + 1;
                    } else {
                        break;
                    }
                }
                if (logged_lines >= max_error_lines) {
                    log_error("  ... (output truncated)");
                }
            }
        }
    } else if (WIFSIGNALED(status)) {
        exit_code = WTERMSIG(status);
        log_error("%s was terminated by signal %d for package: %s", step_name ? step_name : "build", exit_code, package_name);
        // Log error output if available
        if (error_output_len > 0) {
            log_error("Output before termination:");
            char *error_line = error_output;
            size_t logged_lines = 0;
            while (error_line && *error_line && logged_lines < max_error_lines) {
                char *newline = strchr(error_line, '\n');
                if (newline) {
                    *newline = '\0';
                }
                if (strlen(error_line) > 0) {
                    log_error("  %s", error_line);
                    logged_lines++;
                }
                if (newline) {
                    *newline = '\n';
                    error_line = newline + 1;
                } else {
                    break;
                }
            }
        }
        success = false;
    } else {
        log_error("%s failed with unknown status for package: %s", step_name ? step_name : "build", package_name);
        success = false;
    }

    return success;
}

bool builder_build_with_output(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir, void (*output_callback)(const char *line, void *userdata), void *userdata) {
    if (!config || !pkg || !source_dir) {
        log_error("builder_build_with_output called with invalid parameters");
        return false;
    }

    log_info("Building package: %s@%s (source_dir=%s, build_dir=%s)",
             pkg->name, pkg->version ? pkg->version : "latest", source_dir, build_dir);

    // Create build directory
    log_developer("Creating build directory: %s", build_dir);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", build_dir);
    int mkdir_result = system(cmd);
    if (mkdir_result != 0) {
        log_error("Failed to create build directory: %s (exit code: %d)", build_dir, WEXITSTATUS(mkdir_result));
        return false;
    }
    log_developer("Build directory created successfully: %s", build_dir);

    // Apply patches
    if (pkg->patches_count > 0) {
        log_debug("Applying %zu patches to source", pkg->patches_count);
        builder_apply_patches(source_dir, pkg->patches, pkg->patches_count);
    }

    // Set up environment
    char main_install_dir[1024];
    char *last_slash = strrchr(config->install_dir, '/');
    if (last_slash) {
        if (strstr(config->install_dir, "/install/") != NULL) {
            size_t len = strstr(config->install_dir, "/install/") - config->install_dir + strlen("/install");
            strncpy(main_install_dir, config->install_dir, len);
            main_install_dir[len] = '\0';
        } else {
            strncpy(main_install_dir, config->install_dir, sizeof(main_install_dir) - 1);
            main_install_dir[sizeof(main_install_dir) - 1] = '\0';
        }
    } else {
        strncpy(main_install_dir, config->install_dir, sizeof(main_install_dir) - 1);
        main_install_dir[sizeof(main_install_dir) - 1] = '\0';
    }

    char env[4096] = "";
    bool strict_isolation = config_is_strict_isolation();

    // Bootstrap handling: For essential bootstrap tools, we need minimal system tools
    // We ONLY use essential system directories (/usr/bin, /bin, /usr/local/bin) - NOT the full system PATH
    // Once these are installed, all subsequent builds use only TSI packages (completely isolated)
    // Bootstrap packages in order: m4, ncurses, bash, coreutils, diffutils, gawk, grep, sed, make, patch, tar, gzip, xz
    bool is_bootstrap_pkg = (strcmp(pkg->name, "m4") == 0 || strcmp(pkg->name, "ncurses") == 0 ||
                             strcmp(pkg->name, "bash") == 0 || strcmp(pkg->name, "coreutils") == 0 ||
                             strcmp(pkg->name, "diffutils") == 0 || strcmp(pkg->name, "gawk") == 0 ||
                             strcmp(pkg->name, "grep") == 0 || strcmp(pkg->name, "sed") == 0 ||
                             strcmp(pkg->name, "make") == 0 || strcmp(pkg->name, "patch") == 0 ||
                             strcmp(pkg->name, "tar") == 0 || strcmp(pkg->name, "gzip") == 0 ||
                             strcmp(pkg->name, "xz") == 0);

    if (is_bootstrap_pkg) {
        // Bootstrap: Use only essential system directories + TSI PATH
        char bootstrap_path[512] = "";
        get_bootstrap_path(bootstrap_path, sizeof(bootstrap_path));

        if (bootstrap_path[0] != '\0') {
            log_developer("Bootstrap mode: Building %s, using minimal essential system directories for bootstrap", pkg->name);
            if (strict_isolation) {
                log_info("Strict isolation: Bootstrap phase - using minimal system tools (gcc, /bin/sh) only");
            }
            snprintf(env, sizeof(env), "PATH=%s/bin:%s PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
                     main_install_dir, bootstrap_path, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
        } else {
            log_warning("No essential system directories found, using only TSI PATH for bootstrap");
            snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
                     main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
        }
    } else {
        // Normal mode: Check strict isolation setting
        if (strict_isolation) {
            // Strict isolation: Only use TSI packages, no system tools (except /bin/sh if needed)
            log_info("Strict isolation: Building %s - using only TSI-installed packages", pkg->name);
            struct stat st;
            bool has_bin = (stat("/bin", &st) == 0 && S_ISDIR(st.st_mode));

            // In strict mode, only include /bin for sh (POSIX requirement)
            // All other tools must come from TSI
            if (has_bin) {
                snprintf(env, sizeof(env), "PATH=%s/bin:/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
                         main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            } else {
                snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
                         main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            }
        } else {
            // Normal mode: Use TSI-installed packages and tools + system C compiler + /bin (for sh)
            // Always include C compiler and /bin in PATH (these are basic system tools, not TSI packages)
            char compiler_dir[512] = "";
            get_compiler_dir(compiler_dir, sizeof(compiler_dir));

            // Build PATH: TSI bin, compiler dir, /bin (for sh and basic POSIX utilities)
            struct stat st;
            bool has_bin = (stat("/bin", &st) == 0 && S_ISDIR(st.st_mode));

            if (strlen(compiler_dir) > 0 && has_bin) {
                snprintf(env, sizeof(env), "PATH=%s/bin:%s:/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
                         main_install_dir, compiler_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            } else if (strlen(compiler_dir) > 0) {
                snprintf(env, sizeof(env), "PATH=%s/bin:%s PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
                         main_install_dir, compiler_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            } else if (has_bin) {
                snprintf(env, sizeof(env), "PATH=%s/bin:/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
                         main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            } else {
                // Fallback: use TSI PATH only (shouldn't happen if system has a compiler and /bin)
                log_warning("C compiler and /bin not found, using only TSI PATH");
                snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
                         main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            }
        }
    }

    // Apply package-specific environment variables
    if (pkg->env_count > 0) {
        for (size_t i = 0; i < pkg->env_count; i++) {
            if (pkg->env_keys[i] && pkg->env_values[i]) {
                // Append to env string: KEY=VALUE
                size_t env_len = strlen(env);
                size_t needed = env_len + strlen(pkg->env_keys[i]) + strlen(pkg->env_values[i]) + 2; // +2 for = and space
                if (needed < sizeof(env)) {
                    if (env_len > 0) {
                        strcat(env, " ");
                    }
                    strcat(env, pkg->env_keys[i]);
                    strcat(env, "=");
                    strcat(env, pkg->env_values[i]);
                    log_developer("Added package env: %s=%s", pkg->env_keys[i], pkg->env_values[i]);
                }
            }
        }
    }

    const char *build_system = pkg->build_system ? pkg->build_system : "autotools";
    log_info("Using build system: %s for package: %s", build_system, pkg->name);
    log_developer("Build environment: %s", env);
    log_developer("Source directory: %s", source_dir);
    log_developer("Build directory: %s", build_dir);
    log_developer("Install directory: %s", config->install_dir);

    if (strcmp(build_system, "autotools") == 0) {
        // Check for configure script
        char configure[512];
        snprintf(configure, sizeof(configure), "%s/configure", source_dir);
        struct stat st;
        if (stat(configure, &st) != 0) {
            log_debug("Configure script not found, running autoreconf");
            snprintf(cmd, sizeof(cmd), "cd '%s' && autoreconf -fiv", source_dir);
            int autoreconf_result = system(cmd);
            if (autoreconf_result != 0) {
                log_warning("autoreconf failed (exit code: %d), continuing anyway", WEXITSTATUS(autoreconf_result));
            }
        }

        // Configure
        log_debug("Running configure for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s ./configure --prefix='%s' 2>&1", source_dir, env, config->install_dir);
        for (size_t i = 0; i < pkg->configure_args_count; i++) {
            strcat(cmd, " ");
            strcat(cmd, pkg->configure_args[i]);
        }
        if (!execute_with_output(cmd, "configure", pkg->name, output_callback, userdata)) {
            log_error("Configure failed for package: %s", pkg->name);
            return false;
        }

        // Make
        log_debug("Running make for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make 2>&1", source_dir, env);
        for (size_t i = 0; i < pkg->make_args_count; i++) {
            strcat(cmd, " ");
            strcat(cmd, pkg->make_args[i]);
        }
        if (!execute_with_output(cmd, "make", pkg->name, output_callback, userdata)) {
            log_error("Make failed for package: %s", pkg->name);
            return false;
        }

    } else if (strcmp(build_system, "cmake") == 0) {
        // CMake configure
        log_debug("Running cmake configure for package: %s", pkg->name);
        size_t cmd_len = 1024;
        char *cmd_buf = malloc(cmd_len);
        snprintf(cmd_buf, cmd_len, "cd '%s' && %s cmake -S '%s' -B '%s' -DCMAKE_INSTALL_PREFIX='%s' 2>&1",
                 build_dir, env, source_dir, build_dir, config->install_dir);
        for (size_t i = 0; i < pkg->cmake_args_count; i++) {
            size_t needed = strlen(cmd_buf) + strlen(pkg->cmake_args[i]) + 2;
            if (needed > cmd_len) {
                cmd_len = needed * 2;
                cmd_buf = realloc(cmd_buf, cmd_len);
            }
            strcat(cmd_buf, " ");
            strcat(cmd_buf, pkg->cmake_args[i]);
        }
        bool result = execute_with_output(cmd_buf, "cmake configure", pkg->name, output_callback, userdata);
        free(cmd_buf);
        if (!result) {
            log_error("CMake configure failed for package: %s", pkg->name);
            return false;
        }

        // CMake build
        log_debug("Running cmake build for package: %s", pkg->name);
        cmd_len = 1024;
        cmd_buf = malloc(cmd_len);
        snprintf(cmd_buf, cmd_len, "cd '%s' && %s cmake --build '%s' 2>&1", build_dir, env, build_dir);
        for (size_t i = 0; i < pkg->make_args_count; i++) {
            size_t needed = strlen(cmd_buf) + strlen(pkg->make_args[i]) + 2;
            if (needed > cmd_len) {
                cmd_len = needed * 2;
                cmd_buf = realloc(cmd_buf, cmd_len);
            }
            strcat(cmd_buf, " ");
            strcat(cmd_buf, pkg->make_args[i]);
        }
        result = execute_with_output(cmd_buf, "cmake build", pkg->name, output_callback, userdata);
        free(cmd_buf);
        if (!result) {
            log_error("CMake build failed for package: %s", pkg->name);
            return false;
        }

    } else if (strcmp(build_system, "make") == 0) {
        log_debug("Running make for package: %s", pkg->name);
        size_t cmd_len = 1024;
        char *cmd_buf = malloc(cmd_len);
        snprintf(cmd_buf, cmd_len, "cd '%s' && %s make", source_dir, env);
        for (size_t i = 0; i < pkg->make_args_count; i++) {
            size_t needed = strlen(cmd_buf) + strlen(pkg->make_args[i]) + 2;
            if (needed > cmd_len) {
                cmd_len = needed * 2;
                cmd_buf = realloc(cmd_buf, cmd_len);
            }
            strcat(cmd_buf, " ");
            strcat(cmd_buf, pkg->make_args[i]);
        }
        // Add stderr redirection
        size_t needed = strlen(cmd_buf) + 5;
        if (needed > cmd_len) {
            cmd_len = needed * 2;
            cmd_buf = realloc(cmd_buf, cmd_len);
        }
        strcat(cmd_buf, " 2>&1");
        bool result = execute_with_output(cmd_buf, "make", pkg->name, output_callback, userdata);
        free(cmd_buf);
        if (!result) {
            log_error("Make failed for package: %s", pkg->name);
            return false;
        }

    } else if (strcmp(build_system, "meson") == 0) {
        log_debug("Running meson setup for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson setup '%s' '%s' --prefix='%s' 2>&1",
                 build_dir, env, build_dir, source_dir, config->install_dir);
        if (!execute_with_output(cmd, "meson setup", pkg->name, output_callback, userdata)) {
            log_error("Meson setup failed for package: %s", pkg->name);
            return false;
        }

        log_debug("Running meson compile for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson compile -C '%s' 2>&1", build_dir, env, build_dir);
        if (!execute_with_output(cmd, "meson compile", pkg->name, output_callback, userdata)) {
            log_error("Meson compile failed for package: %s", pkg->name);
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
                if (!cmd_expanded) {
                    return false;
                }

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
                    } else {
                        free(cmd_expanded);
                        return false;
                    }
                }

                // Execute command in source directory with output capture
                size_t cmd_len = strlen(cmd_expanded) + strlen(source_dir) + strlen(expanded_env) + 64;
                char *full_cmd = malloc(cmd_len);
                if (full_cmd) {
                    snprintf(full_cmd, cmd_len, "cd '%s' && %s %s 2>&1",
                            source_dir, expanded_env, cmd_expanded);
                    char step_name[256];
                    snprintf(step_name, sizeof(step_name), "custom build command %zu", i + 1);
                    if (!execute_with_output(full_cmd, step_name, pkg->name, output_callback, userdata)) {
                        log_error("Custom build command %zu failed for package: %s", i + 1, pkg->name);
                        free(full_cmd);
                        free(cmd_expanded);
                        return false;
                    }
                    free(full_cmd);
                } else {
                    log_error("Failed to allocate memory for custom build command %zu", i + 1);
                    free(cmd_expanded);
                    return false;
                }
                free(cmd_expanded);
            }
            // All commands succeeded
            log_info("All custom build commands completed successfully for package: %s", pkg->name);
            return true;
        } else {
            // No build commands specified, just return success
            log_warning("No build commands specified for custom build system, assuming success for package: %s", pkg->name);
            return true;
        }
    } else {
        log_error("Unknown or unsupported build system: %s for package: %s", build_system, pkg->name);
        return false;
    }

    log_info("Build completed successfully for package: %s", pkg->name);
    return true;
}

bool builder_install_with_output(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir, void (*output_callback)(const char *line, void *userdata), void *userdata) {
    if (!config || !pkg || !source_dir) {
        log_error("builder_install_with_output called with invalid parameters");
        return false;
    }

    log_info("Installing package: %s@%s (install_dir=%s)",
             pkg->name, pkg->version ? pkg->version : "latest", config->install_dir);

    char main_install_dir[1024];
    char *install_pos = strstr(config->install_dir, "/install/");
    if (install_pos) {
        size_t len = install_pos - config->install_dir + strlen("/install");
        strncpy(main_install_dir, config->install_dir, len);
        main_install_dir[len] = '\0';
    } else {
        strncpy(main_install_dir, config->install_dir, sizeof(main_install_dir) - 1);
        main_install_dir[sizeof(main_install_dir) - 1] = '\0';
    }

    char env[4096] = "";
    // Bootstrap handling: For essential bootstrap tools install, we need minimal system tools
    // We ONLY use essential system directories (/usr/bin, /bin, /usr/local/bin) - NOT the full system PATH
    // Once these are installed, all subsequent installs use only TSI's tools (completely isolated)
    // Bootstrap packages: make, coreutils, sed, grep, gawk, bash, m4
    if (strcmp(pkg->name, "make") == 0 || strcmp(pkg->name, "coreutils") == 0 || strcmp(pkg->name, "sed") == 0 ||
        strcmp(pkg->name, "grep") == 0 || strcmp(pkg->name, "gawk") == 0 || strcmp(pkg->name, "bash") == 0 || strcmp(pkg->name, "m4") == 0) {
        // Bootstrap: Use only essential system directories + TSI PATH
        char bootstrap_path[512] = "";
        get_bootstrap_path(bootstrap_path, sizeof(bootstrap_path));

        if (bootstrap_path[0] != '\0') {
            log_developer("Bootstrap mode: Installing %s, using minimal essential system directories for bootstrap", pkg->name);
            snprintf(env, sizeof(env), "PATH=%s/bin:%s PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib",
                     main_install_dir, bootstrap_path, main_install_dir, main_install_dir);
        } else {
            log_warning("No essential system directories found, using only TSI PATH for bootstrap install");
    snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib",
             main_install_dir, main_install_dir, main_install_dir);
        }
    } else {
        // Normal mode: Use TSI-installed packages and tools + system C compiler + /bin (for sh)
        // Always include C compiler and /bin in PATH (these are basic system tools, not TSI packages)
        char compiler_dir[512] = "";
        get_compiler_dir(compiler_dir, sizeof(compiler_dir));

        // Build PATH: TSI bin, compiler dir, /bin (for sh and basic POSIX utilities)
        struct stat st;
        bool has_bin = (stat("/bin", &st) == 0 && S_ISDIR(st.st_mode));

        if (strlen(compiler_dir) > 0 && has_bin) {
            snprintf(env, sizeof(env), "PATH=%s/bin:%s:/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib",
                     main_install_dir, compiler_dir, main_install_dir, main_install_dir);
        } else if (strlen(compiler_dir) > 0) {
            snprintf(env, sizeof(env), "PATH=%s/bin:%s PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib",
                     main_install_dir, compiler_dir, main_install_dir, main_install_dir);
        } else if (has_bin) {
            snprintf(env, sizeof(env), "PATH=%s/bin:/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib",
                     main_install_dir, main_install_dir, main_install_dir);
        } else {
            // Fallback: use TSI PATH only
            log_warning("C compiler and /bin not found, using only TSI PATH for install");
            snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib",
                     main_install_dir, main_install_dir, main_install_dir);
        }
    }

    // Apply package-specific environment variables
    if (pkg->env_count > 0) {
        for (size_t i = 0; i < pkg->env_count; i++) {
            if (pkg->env_keys[i] && pkg->env_values[i]) {
                // Append to env string: KEY=VALUE
                size_t env_len = strlen(env);
                size_t needed = env_len + strlen(pkg->env_keys[i]) + strlen(pkg->env_values[i]) + 2; // +2 for = and space
                if (needed < sizeof(env)) {
                    if (env_len > 0) {
                        strcat(env, " ");
                    }
                    strcat(env, pkg->env_keys[i]);
                    strcat(env, "=");
                    strcat(env, pkg->env_values[i]);
                    log_developer("Added package env for install: %s=%s", pkg->env_keys[i], pkg->env_values[i]);
                }
            }
        }
    }

    const char *build_system = pkg->build_system ? pkg->build_system : "autotools";
    log_debug("Using build system for install: %s", build_system);
    log_developer("Install environment: %s", env);
    char cmd[1024];

    if (strcmp(build_system, "autotools") == 0) {
        log_debug("Running make install for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make install 2>&1", source_dir, env);
    } else if (strcmp(build_system, "cmake") == 0) {
        log_debug("Running cmake --install for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s cmake --install '%s' 2>&1", build_dir, env, build_dir);
    } else if (strcmp(build_system, "meson") == 0) {
        log_debug("Running meson install for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson install -C '%s' 2>&1", build_dir, env, build_dir);
    } else if (strcmp(build_system, "make") == 0) {
        log_debug("Running make install for package: %s", pkg->name);
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make install PREFIX='%s' 2>&1", source_dir, env, config->install_dir);
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
        bool result = execute_with_output(install_cmd, "custom install", pkg->name, output_callback, userdata);
        if (result) {
            log_info("Custom install completed for package: %s", pkg->name);
        } else {
            log_warning("Custom install command failed (may be normal for custom builds)");
        }
        return result;
    } else {
        log_error("Unknown build system for install: %s", build_system);
        return false;
    }

    bool result = execute_with_output(cmd, "install", pkg->name, output_callback, userdata);
    if (result) {
        log_info("Install completed successfully for package: %s", pkg->name);
    } else {
        log_error("Install failed for package: %s", pkg->name);
    }
    return result;
}

