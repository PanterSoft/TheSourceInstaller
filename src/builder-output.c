#include "builder.h"
#include "package.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// Helper function to execute command and capture output line by line
static bool execute_with_output(const char *cmd, void (*output_callback)(const char *line, void *userdata), void *userdata) {
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        return false;
    }

    char buffer[1024];
    char line[1024];
    size_t line_pos = 0;

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        // Process buffer character by character to handle partial lines
        for (size_t i = 0; buffer[i] != '\0'; i++) {
            if (buffer[i] == '\n' || buffer[i] == '\r') {
                if (line_pos > 0) {
                    line[line_pos] = '\0';
                    if (output_callback) {
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
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool builder_build_with_output(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir, void (*output_callback)(const char *line, void *userdata), void *userdata) {
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
    snprintf(env, sizeof(env), "PATH=%s/bin:$PATH PKG_CONFIG_PATH=%s/lib/pkgconfig:$PKG_CONFIG_PATH LD_LIBRARY_PATH=%s/lib:$LD_LIBRARY_PATH CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
             main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);

    const char *build_system = pkg->build_system ? pkg->build_system : "autotools";

    if (strcmp(build_system, "autotools") == 0) {
        // Check for configure script
        char configure[512];
        snprintf(configure, sizeof(configure), "%s/configure", source_dir);
        struct stat st;
        if (stat(configure, &st) != 0) {
            snprintf(cmd, sizeof(cmd), "cd '%s' && autoreconf -fiv 2>/dev/null", source_dir);
            system(cmd);
        }

        // Configure
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s ./configure --prefix='%s'", source_dir, env, config->install_dir);
        for (size_t i = 0; i < pkg->configure_args_count; i++) {
            strcat(cmd, " ");
            strcat(cmd, pkg->configure_args[i]);
        }
        if (!execute_with_output(cmd, output_callback, userdata)) {
            return false;
        }

        // Make
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make", source_dir, env);
        for (size_t i = 0; i < pkg->make_args_count; i++) {
            strcat(cmd, " ");
            strcat(cmd, pkg->make_args[i]);
        }
        if (!execute_with_output(cmd, output_callback, userdata)) {
            return false;
        }

    } else if (strcmp(build_system, "cmake") == 0) {
        // CMake configure
        size_t cmd_len = 1024;
        char *cmd_buf = malloc(cmd_len);
        snprintf(cmd_buf, cmd_len, "cd '%s' && %s cmake -S '%s' -B '%s' -DCMAKE_INSTALL_PREFIX='%s'",
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
        bool result = execute_with_output(cmd_buf, output_callback, userdata);
        free(cmd_buf);
        if (!result) {
            return false;
        }

        // CMake build
        cmd_len = 1024;
        cmd_buf = malloc(cmd_len);
        snprintf(cmd_buf, cmd_len, "cd '%s' && %s cmake --build '%s'", build_dir, env, build_dir);
        for (size_t i = 0; i < pkg->make_args_count; i++) {
            size_t needed = strlen(cmd_buf) + strlen(pkg->make_args[i]) + 2;
            if (needed > cmd_len) {
                cmd_len = needed * 2;
                cmd_buf = realloc(cmd_buf, cmd_len);
            }
            strcat(cmd_buf, " ");
            strcat(cmd_buf, pkg->make_args[i]);
        }
        result = execute_with_output(cmd_buf, output_callback, userdata);
        free(cmd_buf);
        if (!result) {
            return false;
        }

    } else if (strcmp(build_system, "make") == 0) {
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
        bool result = execute_with_output(cmd_buf, output_callback, userdata);
        free(cmd_buf);
        if (!result) {
            return false;
        }

    } else if (strcmp(build_system, "meson") == 0) {
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson setup '%s' '%s' --prefix='%s'",
                 build_dir, env, build_dir, source_dir, config->install_dir);
        if (!execute_with_output(cmd, output_callback, userdata)) {
            return false;
        }

        snprintf(cmd, sizeof(cmd), "cd '%s' && %s meson compile -C '%s'", build_dir, env, build_dir);
        if (!execute_with_output(cmd, output_callback, userdata)) {
            return false;
        }
    }

    return true;
}

bool builder_install_with_output(BuilderConfig *config, Package *pkg, const char *source_dir, const char *build_dir, void (*output_callback)(const char *line, void *userdata), void *userdata) {
    if (!config || !pkg || !source_dir) {
        return false;
    }

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
    snprintf(env, sizeof(env), "PATH=%s/bin:$PATH PKG_CONFIG_PATH=%s/lib/pkgconfig:$PKG_CONFIG_PATH LD_LIBRARY_PATH=%s/lib:$LD_LIBRARY_PATH",
             main_install_dir, main_install_dir, main_install_dir);

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

    return execute_with_output(cmd, output_callback, userdata);
}

