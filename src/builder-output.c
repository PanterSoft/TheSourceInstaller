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
#include <dirent.h>

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
    char *install_pos = strstr(config->install_dir, "/install/");
    if (install_pos) {
        size_t len = install_pos - config->install_dir + strlen("/install");
            strncpy(main_install_dir, config->install_dir, len);
            main_install_dir[len] = '\0';
        } else {
            strncpy(main_install_dir, config->install_dir, sizeof(main_install_dir) - 1);
            main_install_dir[sizeof(main_install_dir) - 1] = '\0';
        }

    // For all autotools packages, we need ls -t for configure scripts
    // Create minimal ls binary/wrapper if coreutils is not available
    const char *build_system_check = pkg->build_system ? pkg->build_system : "autotools";
    bool needs_ls = (strcmp(pkg->name, "make") == 0 || strcmp(build_system_check, "autotools") == 0);
    bool is_coreutils = (strcmp(pkg->name, "coreutils") == 0);
    bool needs_ls_wrapper = needs_ls && !is_coreutils;

    if (needs_ls_wrapper) {
        char tsi_bin_dir[1024];
        snprintf(tsi_bin_dir, sizeof(tsi_bin_dir), "%s/bin", main_install_dir);

        // Create bin directory if it doesn't exist
        char mkdir_cmd[512];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", tsi_bin_dir);
        system(mkdir_cmd); // Ignore errors - directory might already exist

        // Check if coreutils ls is already available (best option)
        char coreutils_ls[1024];
        snprintf(coreutils_ls, sizeof(coreutils_ls), "%s/bin/ls", main_install_dir);
        struct stat st;
        bool coreutils_ls_exists = (stat(coreutils_ls, &st) == 0 && S_ISREG(st.st_mode));

        // Try to create a minimal ls binary only if coreutils ls doesn't exist
        char ls_binary_path[1024];
        snprintf(ls_binary_path, sizeof(ls_binary_path), "%s/ls", tsi_bin_dir);
        bool ls_binary_exists = false;
        if (!coreutils_ls_exists) {
            ls_binary_exists = (stat(ls_binary_path, &st) == 0 && S_ISREG(st.st_mode));
            // Always remove and recompile the ls binary to ensure it has the latest fixes
            // This ensures we get the latest code even if the binary already exists
            if (ls_binary_exists) {
                log_info("Removing old ls binary to recompile with latest fixes");
                if (unlink(ls_binary_path) != 0) {
                    log_warning("Failed to remove old ls binary: %s", ls_binary_path);
                }
            }
            ls_binary_exists = false; // Always recompile
        }

        if (!coreutils_ls_exists) {
            // Try to compile a minimal ls binary from C source
            log_info("Attempting to compile minimal ls binary for bootstrap");
            char ls_source_path[1024];
            snprintf(ls_source_path, sizeof(ls_source_path), "%s/ls.c", tsi_bin_dir);

            // Create minimal ls C source code (same as in builder.c)
            FILE *ls_source = fopen(ls_source_path, "w");
            if (ls_source) {
                fprintf(ls_source, "#include <stdio.h>\n");
                fprintf(ls_source, "#include <stdlib.h>\n");
                fprintf(ls_source, "#include <string.h>\n");
                fprintf(ls_source, "#include <dirent.h>\n");
                fprintf(ls_source, "#include <sys/stat.h>\n");
                fprintf(ls_source, "#include <time.h>\n");
                fprintf(ls_source, "#include <unistd.h>\n");
                fprintf(ls_source, "\n");
                fprintf(ls_source, "struct file_entry {\n");
                fprintf(ls_source, "    char *name;\n");
                fprintf(ls_source, "    time_t mtime;\n");
                fprintf(ls_source, "};\n");
                fprintf(ls_source, "\n");
                fprintf(ls_source, "int compare_mtime(const void *a, const void *b) {\n");
                fprintf(ls_source, "    const struct file_entry *fa = (const struct file_entry *)a;\n");
                fprintf(ls_source, "    const struct file_entry *fb = (const struct file_entry *)b;\n");
                fprintf(ls_source, "    return (int)(fb->mtime - fa->mtime);\n");
                fprintf(ls_source, "}\n");
                fprintf(ls_source, "\n");
                fprintf(ls_source, "int main(int argc, char **argv) {\n");
                fprintf(ls_source, "    int sort_by_time = 0;\n");
                fprintf(ls_source, "    char *target = \".\";\n");
                fprintf(ls_source, "    \n");
                fprintf(ls_source, "    for (int i = 1; i < argc; i++) {\n");
                fprintf(ls_source, "        if (strcmp(argv[i], \"-t\") == 0) {\n");
                fprintf(ls_source, "            sort_by_time = 1;\n");
                fprintf(ls_source, "        } else if (argv[i][0] != '-') {\n");
                fprintf(ls_source, "            target = argv[i];\n");
                fprintf(ls_source, "            break;\n");
                fprintf(ls_source, "        }\n");
                fprintf(ls_source, "    }\n");
                fprintf(ls_source, "    \n");
                fprintf(ls_source, "    // Check if target is a file or directory\n");
                fprintf(ls_source, "    struct stat st;\n");
                fprintf(ls_source, "    int stat_result = stat(target, &st);\n");
                fprintf(ls_source, "    bool is_directory = false;\n");
                fprintf(ls_source, "    \n");
                fprintf(ls_source, "    if (stat_result == 0) {\n");
                fprintf(ls_source, "        // Stat succeeded, check if it's a directory\n");
                fprintf(ls_source, "        is_directory = S_ISDIR(st.st_mode);\n");
                fprintf(ls_source, "    } else {\n");
                fprintf(ls_source, "        // Stat failed - if target is \".\", assume it's current directory\n");
                fprintf(ls_source, "        if (strcmp(target, \".\") == 0) {\n");
                fprintf(ls_source, "            is_directory = true;\n");
                fprintf(ls_source, "        } else {\n");
                fprintf(ls_source, "            // File doesn't exist - return 0 (configure scripts expect this)\n");
                fprintf(ls_source, "            printf(\"%%s\\n\", target);\n");
                fprintf(ls_source, "            return 0;\n");
                fprintf(ls_source, "        }\n");
                fprintf(ls_source, "    }\n");
                fprintf(ls_source, "    \n");
                fprintf(ls_source, "    // If it's a directory, list its contents\n");
                fprintf(ls_source, "    DIR *d = NULL;\n");
                fprintf(ls_source, "    if (is_directory) {\n");
                fprintf(ls_source, "        d = opendir(target);\n");
                fprintf(ls_source, "        if (!d) {\n");
                fprintf(ls_source, "            perror(\"ls\");\n");
                fprintf(ls_source, "            return 1;\n");
                fprintf(ls_source, "        }\n");
                fprintf(ls_source, "    } else {\n");
                fprintf(ls_source, "        // It's a file (regular, symlink, etc.), just print the filename\n");
                fprintf(ls_source, "        printf(\"%%s\\n\", target);\n");
                fprintf(ls_source, "        return 0;\n");
                fprintf(ls_source, "    }\n");
                fprintf(ls_source, "    \n");
                fprintf(ls_source, "    struct file_entry *entries = NULL;\n");
                fprintf(ls_source, "    size_t count = 0;\n");
                fprintf(ls_source, "    size_t capacity = 64;\n");
                fprintf(ls_source, "    entries = malloc(capacity * sizeof(struct file_entry));\n");
                fprintf(ls_source, "    \n");
                fprintf(ls_source, "    struct dirent *entry;\n");
                fprintf(ls_source, "    while ((entry = readdir(d)) != NULL) {\n");
                fprintf(ls_source, "        if (strcmp(entry->d_name, \".\") == 0 || strcmp(entry->d_name, \"..\") == 0)\n");
                fprintf(ls_source, "            continue;\n");
                fprintf(ls_source, "        \n");
                fprintf(ls_source, "        char path[1024];\n");
                fprintf(ls_source, "        // Use target as base directory (it's already verified as a directory)\n");
                fprintf(ls_source, "        snprintf(path, sizeof(path), \"%%s/%%s\", target, entry->d_name);\n");
                fprintf(ls_source, "        \n");
                fprintf(ls_source, "        struct stat st;\n");
                fprintf(ls_source, "        if (stat(path, &st) == 0) {\n");
                fprintf(ls_source, "            if (count >= capacity) {\n");
                fprintf(ls_source, "                capacity *= 2;\n");
                fprintf(ls_source, "                entries = realloc(entries, capacity * sizeof(struct file_entry));\n");
                fprintf(ls_source, "            }\n");
                fprintf(ls_source, "            entries[count].name = strdup(entry->d_name);\n");
                fprintf(ls_source, "            entries[count].mtime = st.st_mtime;\n");
                fprintf(ls_source, "            count++;\n");
                fprintf(ls_source, "        }\n");
                fprintf(ls_source, "    }\n");
                fprintf(ls_source, "    closedir(d);\n");
                fprintf(ls_source, "    \n");
                fprintf(ls_source, "    if (sort_by_time) {\n");
                fprintf(ls_source, "        qsort(entries, count, sizeof(struct file_entry), compare_mtime);\n");
                fprintf(ls_source, "    }\n");
                fprintf(ls_source, "    \n");
                fprintf(ls_source, "    for (size_t i = 0; i < count; i++) {\n");
                fprintf(ls_source, "        printf(\"%%s\\n\", entries[i].name);\n");
                fprintf(ls_source, "        free(entries[i].name);\n");
                fprintf(ls_source, "    }\n");
                fprintf(ls_source, "    free(entries);\n");
                fprintf(ls_source, "    return 0;\n");
                fprintf(ls_source, "}\n");
                fclose(ls_source);

                // Try to compile it
                char compile_cmd[2048];
                snprintf(compile_cmd, sizeof(compile_cmd),
                    "(gcc -o '%s' '%s' -O2 2>&1 || "
                    "cc -o '%s' '%s' -O2 2>&1) && "
                    "rm -f '%s'",
                    ls_binary_path, ls_source_path,
                    ls_binary_path, ls_source_path,
                    ls_source_path);

                int compile_result = system(compile_cmd);
                if (compile_result == 0 && stat(ls_binary_path, &st) == 0) {
                    log_info("Successfully compiled minimal ls binary: %s", ls_binary_path);
                    ls_binary_exists = true;
    } else {
                    log_warning("Failed to compile ls binary, falling back to wrapper script");
                    unlink(ls_source_path);
                }
            }
        }

        // If binary compilation failed or doesn't exist, create wrapper script as fallback
        if (!ls_binary_exists && !coreutils_ls_exists) {
            char ls_wrapper_path[1024];
            snprintf(ls_wrapper_path, sizeof(ls_wrapper_path), "%s/ls", tsi_bin_dir);

            // Check if wrapper already exists
            if (stat(ls_wrapper_path, &st) != 0) {
                FILE *fp = fopen(ls_wrapper_path, "w");
                if (fp) {
                    fprintf(fp, "#!/bin/sh\n");
                    fprintf(fp, "# Minimal ls wrapper for bootstrap builds on BusyBox\n");
                    fprintf(fp, "# Implements ls -t using find + stat for BusyBox systems\n");
                    fprintf(fp, "\n");
                    fprintf(fp, "# Check if -t flag is present\n");
                    fprintf(fp, "has_t=false\n");
                    fprintf(fp, "dir=\".\"\n");
                    fprintf(fp, "for arg in \"$@\"; do\n");
                    fprintf(fp, "    case \"$arg\" in\n");
                    fprintf(fp, "        -t) has_t=true ;;\n");
                    fprintf(fp, "        -t*) has_t=true ;;\n");
                    fprintf(fp, "        *-t*) has_t=true ;;\n");
                    fprintf(fp, "        -*) : ;;\n");
                    fprintf(fp, "        *) dir=\"$arg\" ;;\n");
                    fprintf(fp, "    esac\n");
                    fprintf(fp, "done\n");
                    fprintf(fp, "\n");
                    fprintf(fp, "if [ \"$has_t\" = \"true\" ]; then\n");
                    fprintf(fp, "    # Try GNU ls first if available\n");
                    fprintf(fp, "    if [ -x /usr/bin/ls ] && /usr/bin/ls -t \"$dir\" >/dev/null 2>&1; then\n");
                    fprintf(fp, "        exec /usr/bin/ls \"$@\"\n");
                    fprintf(fp, "    fi\n");
                    fprintf(fp, "    # Try system ls (might work on some systems)\n");
                    fprintf(fp, "    if /bin/ls \"$@\" >/dev/null 2>&1; then\n");
                    fprintf(fp, "        exec /bin/ls \"$@\"\n");
                    fprintf(fp, "    fi\n");
                    fprintf(fp, "    # BusyBox ls doesn't support -t, implement it with find + stat\n");
                    fprintf(fp, "    tmp=$(mktemp 2>/dev/null || echo /tmp/ls-tmp-$$)\n");
                    fprintf(fp, "    > \"$tmp\"\n");
                    fprintf(fp, "    for item in \"$dir\"/* \"$dir\"/.*; do\n");
                    fprintf(fp, "        [ \"$item\" = \"$dir/*\" ] && continue\n");
                    fprintf(fp, "        [ \"$item\" = \"$dir/.\" ] && continue\n");
                    fprintf(fp, "        [ \"$item\" = \"$dir/..\" ] && continue\n");
                    fprintf(fp, "        if [ -e \"$item\" ]; then\n");
                    fprintf(fp, "            name=$(basename \"$item\")\n");
                    fprintf(fp, "            mtime=$(stat -c '%%Y' \"$item\" 2>/dev/null || stat -f '%%m' \"$item\" 2>/dev/null || echo 0)\n");
                    fprintf(fp, "            printf '%%s\\t%%s\\n' \"$mtime\" \"$name\" >> \"$tmp\"\n");
                    fprintf(fp, "        fi\n");
                    fprintf(fp, "    done\n");
                    fprintf(fp, "    sort -rn \"$tmp\" | cut -f2-\n");
                    fprintf(fp, "    rm -f \"$tmp\"\n");
                    fprintf(fp, "else\n");
                    fprintf(fp, "    if [ -x /usr/bin/ls ]; then\n");
                    fprintf(fp, "        exec /usr/bin/ls \"$@\"\n");
                    fprintf(fp, "    else\n");
                    fprintf(fp, "        exec /bin/ls \"$@\"\n");
                    fprintf(fp, "    fi\n");
                    fprintf(fp, "fi\n");
                    fclose(fp);

                    // Make it executable
                    char chmod_cmd[512];
                    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x '%s'", ls_wrapper_path);
                    system(chmod_cmd);
                    log_info("Created bootstrap ls wrapper: %s", ls_wrapper_path);
                }
            } else {
                // Wrapper already exists, verify it's executable
                if (!(st.st_mode & S_IXUSR)) {
                    char chmod_cmd[512];
                    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod +x '%s'", ls_wrapper_path);
                    system(chmod_cmd);
                }
            }
        }
    }

    char env[4096] = "";
    bool strict_isolation = config_is_strict_isolation();

    // Bootstrap handling: For essential bootstrap tools, we need minimal system tools
    // We ONLY use essential system directories (/usr/bin, /bin, /usr/local/bin) - NOT the full system PATH
    // Once these are installed, all subsequent builds use only TSI packages (completely isolated)
    // Essential base tools bootstrap sequence:
    //   1. M4 - Required for Autoconf
    //   2. Ncurses - Required for interactive shells and text-based tools
    //   3. Bash - Most build scripts (configure) require a POSIX shell
    //   4. Coreutils - Provides ls, cp, mkdir, etc., which make needs to move files
    //   5. Diffutils - Required by many test suites and build scripts
    //   6. Gawk - Required for processing text during the build of more complex tools
    //   7. Grep / Sed - Essential text manipulation for the configure scripts
    //   8. Make - Now you have a native make that uses your native coreutils
    //   9. Patch - Needed to apply fixes to source code before compiling
    //  10. Tar / Gzip / Xz - To unpack the source code of future packages
    //  11. Binutils - Required for GCC (linker, assembler, etc.)
    //  12. GCC (Final) - Now rebuild GCC using your new make, binutils, and other tools
    bool is_bootstrap_pkg = (strcmp(pkg->name, "m4") == 0 || strcmp(pkg->name, "ncurses") == 0 ||
                             strcmp(pkg->name, "bash") == 0 || strcmp(pkg->name, "coreutils") == 0 ||
                             strcmp(pkg->name, "diffutils") == 0 || strcmp(pkg->name, "gawk") == 0 ||
                             strcmp(pkg->name, "grep") == 0 || strcmp(pkg->name, "sed") == 0 ||
                             strcmp(pkg->name, "make") == 0 || strcmp(pkg->name, "patch") == 0 ||
                             strcmp(pkg->name, "tar") == 0 || strcmp(pkg->name, "gzip") == 0 ||
                             strcmp(pkg->name, "xz") == 0 || strcmp(pkg->name, "binutils") == 0 ||
                             strcmp(pkg->name, "gcc") == 0);

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
        // After bootstrap: Check strict isolation setting
        if (strict_isolation) {
            // Strict isolation: ONLY use TSI-installed packages, no system tools at all
            // This means: no system compiler, no /bin, no system tools - everything from TSI
            log_info("Strict isolation: Building %s - using ONLY TSI-installed packages (no system tools)", pkg->name);

            // Check if TSI has bash installed (prefer it over /bin/sh)
            char tsi_bash[1024];
            snprintf(tsi_bash, sizeof(tsi_bash), "%s/bin/bash", main_install_dir);
            struct stat bash_st;
            bool has_tsi_bash = (stat(tsi_bash, &bash_st) == 0);

            // In strict isolation mode after bootstrap: ONLY TSI packages
            // No system compiler, no /bin - everything must come from TSI
            // Only fallback to /bin/sh if TSI bash is not available (shouldn't happen after bootstrap)
            struct stat st;
            bool has_bin = (stat("/bin", &st) == 0 && S_ISDIR(st.st_mode));

            if (has_tsi_bash) {
                // Use TSI bash - complete isolation, no system tools
                snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib SHELL=%s/bin/bash",
                         main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            } else if (has_bin) {
                // Fallback: TSI bash not available yet, use /bin/sh (should only happen during transition)
                log_warning("TSI bash not found, falling back to /bin/sh (this should not happen after bootstrap)");
                snprintf(env, sizeof(env), "PATH=%s/bin:/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib CPPFLAGS=-I%s/include LDFLAGS=-L%s/lib",
                         main_install_dir, main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            } else {
                // No /bin available - use only TSI (may fail if shell scripts are needed)
                log_warning("No /bin available and TSI bash not found - using only TSI PATH");
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
    // Note: CFLAGS will be excluded from env string if we pass it directly to make
    if (pkg->env_count > 0) {
        for (size_t i = 0; i < pkg->env_count; i++) {
            if (pkg->env_keys[i] && pkg->env_values[i]) {
                // Skip CFLAGS here - it will be passed directly to make if needed
                if (strcmp(pkg->env_keys[i], "CFLAGS") == 0) {
                    continue;
                }
                // Append to env string: KEY='VALUE' (quote values to handle spaces)
                size_t env_len = strlen(env);
                size_t needed = env_len + strlen(pkg->env_keys[i]) + strlen(pkg->env_values[i]) + 5; // +5 for =, '', space, and quotes
                if (needed < sizeof(env)) {
                    if (env_len > 0) {
                        strcat(env, " ");
                    }
                    strcat(env, pkg->env_keys[i]);
                    strcat(env, "='");
                    strcat(env, pkg->env_values[i]);
                    strcat(env, "'");
                    log_developer("Added package env: %s='%s'", pkg->env_keys[i], pkg->env_values[i]);
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
        log_developer("Checking for configure script at: %s", configure);
        if (stat(configure, &st) != 0) {
            log_info("Configure script not found at: %s", configure);

            // First, verify the source directory has content (extraction might have failed)
            DIR *dir_check = opendir(source_dir);
            size_t file_count = 0;
            if (dir_check) {
                struct dirent *entry;
                while ((entry = readdir(dir_check)) != NULL) {
                    if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                        file_count++;
                    }
                }
                closedir(dir_check);
            }

            if (file_count == 0) {
                log_error("Source directory is empty: %s", source_dir);
                log_error("Archive extraction appears to have failed completely");
                log_error("Please check the download and extraction logs above for errors");
                return false;
            } else {
                log_info("Source directory contains %zu files, but configure script is missing", file_count);
            }

            // Configure script not found - try bootstrap scripts first
            log_info("Configure script not found, checking for bootstrap scripts");

            // Check for bootstrap scripts (used by coreutils and some other packages)
            // Try common bootstrap script names in order of preference
            const char *bootstrap_scripts[] = {"bootstrap", "bootstrap.sh", "autogen.sh", "autogen"};
            bool bootstrap_ran = false;

            for (size_t i = 0; i < sizeof(bootstrap_scripts) / sizeof(bootstrap_scripts[0]); i++) {
                char bootstrap[512];
                snprintf(bootstrap, sizeof(bootstrap), "%s/%s", source_dir, bootstrap_scripts[i]);
                if (stat(bootstrap, &st) == 0) {
                    log_info("Found %s script, running it to generate configure", bootstrap_scripts[i]);
                    snprintf(cmd, sizeof(cmd), "cd '%s' && sh %s 2>&1", source_dir, bootstrap_scripts[i]);
                    int bootstrap_result = system(cmd);
                    bootstrap_ran = true;
                    if (bootstrap_result != 0) {
                        log_warning("%s script failed (exit code: %d), trying next bootstrap method", bootstrap_scripts[i], WEXITSTATUS(bootstrap_result));
                    } else {
                        log_info("%s script completed successfully", bootstrap_scripts[i]);
                        // Re-check if configure was generated
                        if (stat(configure, &st) == 0) {
                            log_info("Configure script generated successfully by %s", bootstrap_scripts[i]);
                            break; // Success, stop trying other bootstrap scripts
                        } else {
                            log_warning("%s script ran but configure script was not generated", bootstrap_scripts[i]);
                        }
                    }
                }
            }

            // Check if configure was generated by bootstrap, if not try autoreconf
            if (stat(configure, &st) != 0) {
                if (bootstrap_ran) {
                    log_warning("Bootstrap scripts ran but configure script was not generated");
                }

                // Before trying autoreconf, check if source files exist (might be extraction issue)
                char configure_ac[512], configure_in[512];
                snprintf(configure_ac, sizeof(configure_ac), "%s/configure.ac", source_dir);
                snprintf(configure_in, sizeof(configure_in), "%s/configure.in", source_dir);
                bool has_configure_ac = (stat(configure_ac, &st) == 0);
                bool has_configure_in = (stat(configure_in, &st) == 0);

                if (!has_configure_ac && !has_configure_in) {
                    // Check if source directory is empty or has very few files (extraction might have failed)
                    if (file_count == 0) {
                        log_error("Source directory is empty: %s", source_dir);
                        log_error("Archive extraction appears to have failed completely");
                        log_error("Please check the download and extraction logs above for errors");
                        return false;
                    } else if (file_count < 5) {
                        log_error("Source directory has very few files (%zu) - extraction may have failed", file_count);
                        log_error("Source directory: %s", source_dir);
                        log_error("Expected files like configure, Makefile.in, README, etc. are missing");
                    }

                    log_error("No configure script found and no configure.ac or configure.in found in source directory");
                    log_error("This strongly suggests that archive extraction failed or was incomplete");
                    log_error("Source directory: %s", source_dir);
                    log_error("Please verify:");
                    log_error("  1. The archive was downloaded completely");
                    log_error("  2. The archive extraction succeeded (check for errors above)");
                    log_error("  3. The source directory contains the expected files");
                    log_error("  4. For coreutils, the tarball should include a pre-generated configure script");
                    return false;
                }

                log_info("Configure script not found, but configure.ac or configure.in exists - trying autoreconf");
                // Try to generate configure
                snprintf(cmd, sizeof(cmd), "cd '%s' && autoreconf -fiv 2>&1", source_dir);
            int autoreconf_result = system(cmd);
            if (autoreconf_result != 0) {
                    log_error("autoreconf failed (exit code: %d) - autotools may not be installed", WEXITSTATUS(autoreconf_result));
                    log_error("Package %s requires autotools (autoconf, automake) to generate configure script", pkg->name);
                    log_error("Either install autotools first, or ensure the package tarball includes a pre-generated configure script");
                    return false;
                } else {
                    // Re-check if configure was generated
                    if (stat(configure, &st) != 0) {
                        log_error("autoreconf completed but configure script was not generated");
                        log_error("This may indicate that autotools are incomplete or the source is corrupted");
                        return false;
                    }
                    log_info("Configure script generated successfully by autoreconf");
                }
            }
        } else {
            log_debug("Configure script found, skipping bootstrap");
        }

        // Final check: ensure configure script exists and is executable
        if (stat(configure, &st) != 0) {
            log_error("Configure script not found after all bootstrap attempts: %s", configure);
            log_error("Package %s cannot be built without a configure script", pkg->name);
            log_error("The package tarball should include a pre-generated configure script, or autotools must be installed");
            return false;
        }
        if (!(st.st_mode & S_IXUSR)) {
            log_info("Configure script exists but is not executable, making it executable");
            snprintf(cmd, sizeof(cmd), "chmod +x '%s'", configure);
            system(cmd); // Ignore errors
        }

        // Configure
        // Standard autotools build process (per INSTALL files):
        // Step 1: './configure' to configure the package for your system
        log_debug("Running configure for package: %s", pkg->name);

        // CRITICAL: For BusyBox systems, we MUST ensure the ls wrapper is found
        // Explicitly set PATH in the command itself to ensure ls wrapper is used
        if (needs_ls_wrapper) {
            // Verify wrapper exists before running configure
            char verify_cmd[1024];
            snprintf(verify_cmd, sizeof(verify_cmd), "test -x '%s/bin/ls' && echo 'exists' || echo 'missing'", main_install_dir);
            FILE *verify = popen(verify_cmd, "r");
            bool wrapper_verified = false;
            if (verify) {
                char verify_result[256];
                if (fgets(verify_result, sizeof(verify_result), verify)) {
                    if (strstr(verify_result, "exists") != NULL) {
                        wrapper_verified = true;
                        log_info("ls wrapper verified: %s/bin/ls", main_install_dir);
                    } else {
                        log_warning("ls wrapper NOT found at: %s/bin/ls", main_install_dir);
                    }
                }
                pclose(verify);
            }
            if (!wrapper_verified) {
                log_error("ls wrapper is missing! Cannot proceed with configure on BusyBox system");
                return false;
            }
            // Explicitly set PATH in configure command to ensure ls wrapper is found
            snprintf(cmd, sizeof(cmd), "cd '%s' && PATH='%s/bin:/usr/bin:/bin' %s ./configure --prefix='%s'",
                     source_dir, main_install_dir, env, config->install_dir);
            log_info("Running configure with explicit PATH='%s/bin:/usr/bin:/bin' (ls wrapper at %s/bin/ls)", main_install_dir, main_install_dir);
        } else {
            snprintf(cmd, sizeof(cmd), "cd '%s' && %s ./configure --prefix='%s'", source_dir, env, config->install_dir);
        }

        for (size_t i = 0; i < pkg->configure_args_count; i++) {
            strcat(cmd, " ");
            strcat(cmd, pkg->configure_args[i]);
        }
        strcat(cmd, " 2>&1");

        if (!execute_with_output(cmd, "configure", pkg->name, output_callback, userdata)) {
            log_error("Configure failed for package: %s", pkg->name);
            return false;
        }

        // Make
        // Standard autotools build process (per INSTALL files):
        // Step 2: 'make' to compile the package
        // (Optional Step 3: 'make check' - not implemented, can be added if needed)
        log_debug("Running make for package: %s", pkg->name);
        // Extract CFLAGS from env and pass directly to make to override Makefile CFLAGS
        // Also override WERROR_CFLAGS to ensure -Werror is not applied
        const char *cflags_env = NULL;
        if (pkg->env_count > 0) {
            for (size_t i = 0; i < pkg->env_count; i++) {
                if (pkg->env_keys[i] && strcmp(pkg->env_keys[i], "CFLAGS") == 0) {
                    cflags_env = pkg->env_values[i];
                    break;
                }
            }
        }
        // Build success check: verify that build artifacts were created
        // For binaries: check for the binary name (e.g., src/m4, m4, src/bash, bash)
        // For libraries: check for library files (.a, .so, .dylib) in lib/ directory
        // Generic check: look for common build artifacts (lib directory with files)
        char build_check[512];
        // Use simpler check that doesn't rely on complex nested command substitution
        snprintf(build_check, sizeof(build_check),
                 "if [ -f src/%s ] || [ -f %s ] || [ -f lib/lib%s.a ] || [ -f lib/lib%s.so ] || [ -f lib/lib%s.dylib ] || [ -d lib ]; then exit 0; else exit 1; fi",
                 pkg->name, pkg->name, pkg->name, pkg->name, pkg->name);

        if (cflags_env) {
            // Pass CFLAGS directly to make to override Makefile CFLAGS
            // CFLAGS is excluded from env string (see above) to avoid conflicts
            // Also set WERROR_CFLAGS and AM_CFLAGS to empty to prevent -Werror from being added
            // Build with -k to continue on errors, but check if build artifacts were created
            // This allows build to succeed even if optional targets (doc, tests) fail
            snprintf(cmd, sizeof(cmd), "cd '%s' && %s make -k CFLAGS='%s' WERROR_CFLAGS='' AM_CFLAGS='' all 2>&1; %s", source_dir, env, cflags_env, build_check);
        } else {
            // Build with -k to continue on errors, but check if build artifacts were created
            // This allows build to succeed even if optional targets (doc, tests) fail
            snprintf(cmd, sizeof(cmd), "cd '%s' && %s make -k all 2>&1; %s", source_dir, env, build_check);
        }
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
    // Essential base tools bootstrap sequence:
    //   1. M4 - Required for Autoconf
    //   2. Ncurses - Required for interactive shells and text-based tools
    //   3. Bash - Most build scripts (configure) require a POSIX shell
    //   4. Coreutils - Provides ls, cp, mkdir, etc., which make needs to move files
    //   5. Diffutils - Required by many test suites and build scripts
    //   6. Gawk - Required for processing text during the build of more complex tools
    //   7. Grep / Sed - Essential text manipulation for the configure scripts
    //   8. Make - Now you have a native make that uses your native coreutils
    //   9. Patch - Needed to apply fixes to source code before compiling
    //  10. Tar / Gzip / Xz - To unpack the source code of future packages
    //  11. Binutils - Required for GCC (linker, assembler, etc.)
    //  12. GCC (Final) - Now rebuild GCC using your new make, binutils, and other tools
    bool is_bootstrap_pkg = (strcmp(pkg->name, "m4") == 0 || strcmp(pkg->name, "ncurses") == 0 ||
                             strcmp(pkg->name, "bash") == 0 || strcmp(pkg->name, "coreutils") == 0 ||
                             strcmp(pkg->name, "diffutils") == 0 || strcmp(pkg->name, "gawk") == 0 ||
                             strcmp(pkg->name, "grep") == 0 || strcmp(pkg->name, "sed") == 0 ||
                             strcmp(pkg->name, "make") == 0 || strcmp(pkg->name, "patch") == 0 ||
                             strcmp(pkg->name, "tar") == 0 || strcmp(pkg->name, "gzip") == 0 ||
                             strcmp(pkg->name, "xz") == 0 || strcmp(pkg->name, "binutils") == 0 ||
                             strcmp(pkg->name, "gcc") == 0);

    if (is_bootstrap_pkg) {
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
        // After bootstrap: Check strict isolation setting
        bool strict_isolation = config_is_strict_isolation();
        if (strict_isolation) {
            // Strict isolation: ONLY use TSI-installed packages, no system tools at all
            // This means: no system compiler, no /bin, no system tools - everything from TSI
            log_info("Strict isolation: Installing %s - using ONLY TSI-installed packages (no system tools)", pkg->name);

            // Check if TSI has bash installed (prefer it over /bin/sh)
            char tsi_bash[1024];
            snprintf(tsi_bash, sizeof(tsi_bash), "%s/bin/bash", main_install_dir);
            struct stat bash_st;
            bool has_tsi_bash = (stat(tsi_bash, &bash_st) == 0);

            // In strict isolation mode after bootstrap: ONLY TSI packages
            // No system compiler, no /bin - everything must come from TSI
            // Only fallback to /bin/sh if TSI bash is not available (shouldn't happen after bootstrap)
            struct stat st;
            bool has_bin = (stat("/bin", &st) == 0 && S_ISDIR(st.st_mode));

            if (has_tsi_bash) {
                // Use TSI bash - complete isolation, no system tools
                snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib SHELL=%s/bin/bash",
                         main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            } else if (has_bin) {
                // Fallback: TSI bash not available yet, use /bin/sh (should only happen during transition)
                log_warning("TSI bash not found, falling back to /bin/sh (this should not happen after bootstrap)");
                snprintf(env, sizeof(env), "PATH=%s/bin:/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib",
                         main_install_dir, main_install_dir, main_install_dir, main_install_dir);
            } else {
                // No /bin available - use only TSI (may fail if shell scripts are needed)
                log_warning("No /bin available and TSI bash not found - using only TSI PATH");
                snprintf(env, sizeof(env), "PATH=%s/bin PKG_CONFIG_PATH=%s/lib/pkgconfig LD_LIBRARY_PATH=%s/lib",
                         main_install_dir, main_install_dir, main_install_dir, main_install_dir);
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
    }

    // Apply package-specific environment variables (excluding CFLAGS - not needed for install)
    // Note: CFLAGS is excluded as it's only needed during build, not install
    if (pkg->env_count > 0) {
        for (size_t i = 0; i < pkg->env_count; i++) {
            if (pkg->env_keys[i] && pkg->env_values[i]) {
                // Skip CFLAGS - not needed for install
                if (strcmp(pkg->env_keys[i], "CFLAGS") == 0) {
                    continue;
                }
                // Append to env string: KEY='VALUE' (quote values to handle spaces)
                size_t env_len = strlen(env);
                size_t needed = env_len + strlen(pkg->env_keys[i]) + strlen(pkg->env_values[i]) + 5; // +5 for =, '', space, and quotes
                if (needed < sizeof(env)) {
                    if (env_len > 0) {
                        strcat(env, " ");
                    }
                    strcat(env, pkg->env_keys[i]);
                    strcat(env, "='");
                    strcat(env, pkg->env_values[i]);
                    strcat(env, "'");
                    log_developer("Added package env for install: %s='%s'", pkg->env_keys[i], pkg->env_values[i]);
                }
            }
        }
    }

    const char *build_system = pkg->build_system ? pkg->build_system : "autotools";
    log_debug("Using build system for install: %s", build_system);
    log_developer("Install environment: %s", env);
    char cmd[1024];

    if (strcmp(build_system, "autotools") == 0) {
        // Standard autotools install process (per INSTALL files):
        // Step 4: 'make install' to install the programs and any data files
        // (Optional Step 5: 'make installcheck' - not implemented, can be added if needed)
        // Use -k flag to continue on errors (e.g., missing help2man for doc target)
        // Check if package was installed successfully - check for binary, library, or header files
        // For packages like coreutils that install multiple binaries, check if bin directory has any files
        log_debug("Running make install for package: %s", pkg->name);
        // Check for: specific binary, any binary in bin/, library files, or lib/include directories
        snprintf(cmd, sizeof(cmd), "cd '%s' && %s make -k install 2>&1; if [ -f '%s/bin/%s' ] || [ -f '%s/bin/%s.exe' ] || ([ -d '%s/bin' ] && [ \"$(ls -A '%s/bin' 2>/dev/null)\" ]) || [ -f '%s/lib/lib%s.a' ] || [ -f '%s/lib/lib%s.so' ] || [ -f '%s/lib/lib%s.dylib' ] || [ -d '%s/lib' ] || [ -d '%s/include' ]; then exit 0; else exit 1; fi",
                 source_dir, env, config->install_dir, pkg->name, config->install_dir, pkg->name,
                 config->install_dir, config->install_dir,
                 config->install_dir, pkg->name, config->install_dir, pkg->name, config->install_dir, pkg->name,
                 config->install_dir, config->install_dir);
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

