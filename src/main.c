#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "package.h"
#include "database.h"
#include "resolver.h"
#include "fetcher.h"
#include "builder.h"
#include "log.h"
#include "config.h"

// Output callback for build/install progress
static void output_callback(const char *line, void *userdata) {
    (void)userdata;
    if (line) {
        // Filter out shell error messages that are just noise
        if (strstr(line, "sh: -c:") != NULL &&
            (strstr(line, "unexpected EOF") != NULL ||
             strstr(line, "syntax error") != NULL ||
             strstr(line, "unexpected end of file") != NULL)) {
            return;
        }
        // Filter out lines that are just paths ending with quotes (noise from install process)
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\'' &&
            (strchr(line, '/') != NULL || strstr(line, "man") != NULL)) {
            return;
        }
        printf("%s\n", line);
        fflush(stdout);
    }
}

static const char *get_home_dir(void) {
    const char *home = getenv("HOME");
    return home ? home : "/root";
}

// Get TSI installation prefix by detecting where the binary is located
// Returns the prefix (e.g., /opt/tsi or ~/.tsi) or NULL if detection fails
static const char *get_tsi_prefix(void) {
    static char prefix[1024] = {0};
    static bool initialized = false;

    if (initialized) {
        return prefix[0] ? prefix : NULL;
    }
    initialized = true;

    // Try to get the full path to the executable
    char exe_path[1024] = {0};
    ssize_t len = 0;

#ifdef __APPLE__
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        len = strlen(exe_path);
    }
#else
    // Linux: try /proc/self/exe first (most reliable)
    len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
#endif

    if (len > 0 && len < (ssize_t)sizeof(exe_path)) {
        exe_path[len] = '\0';
        // Find the last /bin/tsi pattern and extract prefix
        char *bin_pos = strstr(exe_path, "/bin/tsi");
        if (bin_pos) {
            size_t prefix_len = bin_pos - exe_path;
            if (prefix_len > 0 && prefix_len < sizeof(prefix)) {
                strncpy(prefix, exe_path, prefix_len);
                prefix[prefix_len] = '\0';
                return prefix;
            }
        }
    }

    return NULL;
}

// Get TSI prefix with fallback to default location
// This is the main function to use - it auto-detects or falls back to $HOME/.tsi
static void get_tsi_prefix_with_fallback(char *tsi_prefix, size_t size, const char *user_prefix) {
    if (user_prefix) {
        // User explicitly provided prefix
        strncpy(tsi_prefix, user_prefix, size - 1);
        tsi_prefix[size - 1] = '\0';
        return;
    }

    // Try to auto-detect from binary location
    const char *detected = get_tsi_prefix();
    if (detected) {
        strncpy(tsi_prefix, detected, size - 1);
        tsi_prefix[size - 1] = '\0';
        return;
    }

    // Fallback to default location
    const char *home = get_home_dir();
    snprintf(tsi_prefix, size, "%s/.tsi", home);
}

static void print_usage(const char *prog_name) {
    printf("TSI - TheSourceInstaller\n");
    printf("Usage: %s <command> [options]\n\n", prog_name);
    printf("Commands:\n");
    printf("  install [--force] [--prefix PATH] <package>  Install a package\n");
    printf("  remove <package> [package...]                Remove installed package(s)\n");
    printf("  list                                         List installed packages\n");
    printf("  info <package>                               Show package information\n");
    printf("  versions <package>                           List all available versions\n");
    printf("  update [--repo URL] [--local PATH]           Update package repository\n");
    printf("  --help                                       Show this help\n");
    printf("  --version                                    Show version\n");
    printf("\n");
}

// Helper to extract package name from "package@version" format
// Returns true if the package spec matches the given name
static bool package_name_matches(const char *package_spec, const char *name) {
    if (!package_spec || !name) return false;
    char *at_pos = strchr((char*)package_spec, '@');
    if (at_pos) {
        size_t name_len = at_pos - package_spec;
        return (name_len == strlen(name) && strncmp(package_spec, name, name_len) == 0);
    }
    return (strcmp(package_spec, name) == 0);
}

static bool run_command_with_window(const char *overview, const char *detail, const char *cmd) {
    if (!cmd || !*cmd) {
        return false;
    }

    if (overview && *overview) {
        printf("-> %s", overview);
        if (detail && *detail) {
            printf(" %s", detail);
        }
        printf("\n");
    }

        printf("%s\n", cmd);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        return false;
    }

    char line[256];
    int status = -1;
    bool pipe_error = false;

    while (fgets(line, sizeof(line), pipe)) {
            fputs(line, stdout);
        fflush(stdout);
    }

    if (ferror(pipe)) {
        pipe_error = true;
    }

    status = pclose(pipe);

    if (pipe_error) {
        return false;
    }

    return status == 0;
}

static int cmd_remove(int argc, char **argv) {
    const char *prefix = NULL;
    int package_count = 0;
    const char **packages = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--prefix") == 0 && i + 1 < argc) {
            prefix = argv[++i];
        } else if (argv[i][0] != '-') {
            // Not an option, treat as package name
            package_count++;
            packages = realloc((void*)packages, sizeof(const char*) * package_count);
            if (!packages) {
                fprintf(stderr, "Error: Memory allocation failed\n");
                return 1;
            }
            packages[package_count - 1] = argv[i];
        }
    }

    if (package_count == 0) {
        fprintf(stderr, "Error: at least one package name required\n");
        fprintf(stderr, "Usage: tsi remove [--prefix PATH] <package> [package...]\n");
        if (packages) free((void*)packages);
        return 1;
    }

    char tsi_prefix[1024];
    get_tsi_prefix_with_fallback(tsi_prefix, sizeof(tsi_prefix), prefix);

    char db_dir[1024];
    int len = snprintf(db_dir, sizeof(db_dir), "%s/db", tsi_prefix);
    if (len < 0 || (size_t)len >= sizeof(db_dir)) {
        fprintf(stderr, "Error: Path too long\n");
        if (packages) free((void*)packages);
        return 1;
    }

    Database *db = database_new(db_dir);
    if (!db) {
        fprintf(stderr, "Error: Failed to open database\n");
        if (packages) free((void*)packages);
        return 1;
    }

    int success_count = 0;
    int fail_count = 0;

    // Remove each package
    for (int i = 0; i < package_count; i++) {
        if (database_remove_package(db, packages[i])) {
            printf("Removed %s\n", packages[i]);
            success_count++;
        } else {
            fprintf(stderr, "Warning: Package %s is not installed\n", packages[i]);
            fail_count++;
        }
    }

    database_free(db);
    if (packages) free((void*)packages);

    // Return 0 if all succeeded, 1 if any failed
    return (fail_count > 0) ? 1 : 0;
}

static int cmd_install(int argc, char **argv) {
    bool force = false;
    const char *package_name = NULL;
    const char *package_version = NULL;
    const char *prefix = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--force") == 0) {
            force = true;
        } else if (strcmp(argv[i], "--prefix") == 0 && i + 1 < argc) {
            prefix = argv[++i];
        } else if (!package_name) {
            // Check for package@version syntax
            char *at_pos = strchr(argv[i], '@');
            if (at_pos) {
                size_t name_len = at_pos - argv[i];
                package_name = malloc(name_len + 1);
                if (!package_name) {
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
                strncpy((char*)package_name, argv[i], name_len);
                ((char*)package_name)[name_len] = '\0';
                package_version = strdup(at_pos + 1);
                if (!package_version) {
                    free((char*)package_name);
                    fprintf(stderr, "Error: Memory allocation failed\n");
                    return 1;
                }
            } else {
                package_name = argv[i];
            }
        }
    }

    if (!package_name) {
        fprintf(stderr, "Error: package name required\n");
        fprintf(stderr, "Usage: tsi install [--force] [--prefix PATH] <package>[@version]\n");
        if (package_version) {
            free((char*)package_version);
        }
        return 1;
    }

    char tsi_prefix[1024];
    get_tsi_prefix_with_fallback(tsi_prefix, sizeof(tsi_prefix), prefix);

    // Config is already loaded in main(), but reload if prefix changed
    if (prefix) {
        config_load(tsi_prefix);
    }

    char db_dir[1024];
    int len = snprintf(db_dir, sizeof(db_dir), "%s/db", tsi_prefix);
    if (len < 0 || (size_t)len >= sizeof(db_dir)) {
        fprintf(stderr, "Error: Path too long\n");
        return 1;
    }

    char repo_dir[1024];
    len = snprintf(repo_dir, sizeof(repo_dir), "%s/packages", tsi_prefix);
    if (len < 0 || (size_t)len >= sizeof(repo_dir)) {
        fprintf(stderr, "Error: Path too long\n");
        return 1;
    }

    Database *db = database_new(db_dir);
    if (!db) {
        fprintf(stderr, "Error: Failed to initialize database\n");
        return 1;
    }

    Repository *repo = repository_new(repo_dir);
    if (!repo) {
        fprintf(stderr, "Error: Failed to initialize repository\n");
        database_free(db);
        return 1;
    }

    DependencyResolver *resolver = resolver_new(repo);
    if (!resolver) {
        fprintf(stderr, "Error: Failed to initialize resolver\n");
        repository_free(repo);
        database_free(db);
        if (package_version) {
            free((char*)package_version);
            free((char*)package_name);
        }
        return 1;
    }

    // Check if repository is empty and suggest update
    if (repo->packages_count == 0) {
        fprintf(stderr, "Error: No packages found in repository.\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "The package repository is empty. Run 'tsi update' to download packages.\n");
        fprintf(stderr, "\n");
        resolver_free(resolver);
        repository_free(repo);
        database_free(db);
        if (package_version) {
            free((char*)package_version);
            free((char*)package_name);
        }
        return 1;
    }

    // First verify package exists before proceeding
    // Check if version string is incomplete (ends with dot, is empty, or doesn't match any exact version)
    bool incomplete_version = false;
    if (package_version) {
        size_t version_len = strlen(package_version);
        if (version_len == 0 || package_version[version_len - 1] == '.') {
            incomplete_version = true;
        } else {
            // Check if it's a prefix (doesn't match any exact version but might match some)
            Package *exact_match = repository_get_package_version(repo, package_name, package_version);
            if (!exact_match) {
                // Not an exact match - check if any versions start with this prefix
                size_t versions_count = 0;
                char **versions = repository_list_versions(repo, package_name, &versions_count);
                if (versions && versions_count > 0) {
                    bool has_prefix_match = false;
                    for (size_t i = 0; i < versions_count; i++) {
                        if (strncmp(versions[i], package_version, strlen(package_version)) == 0) {
                            has_prefix_match = true;
                            break;
                        }
                    }
                    if (has_prefix_match) {
                        incomplete_version = true;
                    }
                    // Free versions array
                    for (size_t i = 0; i < versions_count; i++) {
                        free(versions[i]);
                    }
                    free(versions);
                }
            }
        }
    }

    Package *pkg = NULL;
    if (!incomplete_version) {
        pkg = package_version ? repository_get_package_version(repo, package_name, package_version) : repository_get_package(repo, package_name);
    }

    if (!pkg || incomplete_version) {
        if (package_version) {
            if (incomplete_version) {
                fprintf(stderr, "Error: Incomplete version specification '%s@%s'\n", package_name, package_version);
            } else {
                fprintf(stderr, "Error: Package '%s@%s' not found in repository\n", package_name, package_version);
            }

            // Check if package exists (but version doesn't or is incomplete)
            Package *any_version = repository_get_package(repo, package_name);
            if (any_version) {
                // Try to automatically discover and add the version
                if (!incomplete_version && package_version) {
                    fprintf(stderr, "\nVersion '%s' not found. Attempting to discover it...\n", package_version);

                    // Build path to package file
                    char package_file[2048];
                    len = snprintf(package_file, sizeof(package_file), "%s/%s.json", repo_dir, package_name);
                    if (len >= 0 && (size_t)len < sizeof(package_file)) {
                        // Check if discover-versions.py script exists
                        char script_path[2048];
                        // Try to find script relative to repo (assuming we're in a TSI installation)
                        // First try: scripts/discover-versions.py (if running from repo root)
                        // Second try: TSI_PREFIX/scripts/discover-versions.py (if installed)
                        char tsi_prefix[1024];
                        get_tsi_prefix_with_fallback(tsi_prefix, sizeof(tsi_prefix), NULL);

                        // Try installed location first
                        len = snprintf(script_path, sizeof(script_path), "%s/scripts/discover-versions.py", tsi_prefix);
                        if (len >= 0 && (size_t)len < sizeof(script_path)) {
                            struct stat st;
                            if (stat(script_path, &st) != 0) {
                                // Try repo location
                                len = snprintf(script_path, sizeof(script_path), "scripts/discover-versions.py");
                            }
                        }

                        // Build command to check and add version
                        char cmd[4096];
                        len = snprintf(cmd, sizeof(cmd), "python3 \"%s\" \"%s\" --check-version \"%s\" --packages-dir \"%s\" 2>&1",
                                      script_path, package_name, package_version, repo_dir);
                        if (len >= 0 && (size_t)len < sizeof(cmd)) {
                            FILE *fp = popen(cmd, "r");
                            if (fp) {
                                char line[512];
                                bool version_found = false;
                                while (fgets(line, sizeof(line), fp)) {
                                    // Check for success message
                                    if (strstr(line, "found and added") || strstr(line, "✓")) {
                                        version_found = true;
                                    }
                                    // Show output to user
                                    fprintf(stderr, "%s", line);
                                }
                                int exit_code = pclose(fp);

                                if (version_found && exit_code == 0) {
                                    // Reload repository to get the new version
                                    repository_free(repo);
                                    repo = repository_new(repo_dir);
                                    if (repo) {
                                        resolver_free(resolver);
                                        resolver = resolver_new(repo);
                                        if (resolver) {
                                            // Try to get the package again
                                            pkg = repository_get_package_version(repo, package_name, package_version);
                                            if (pkg) {
                                                fprintf(stderr, "✓ Version discovered and added. Proceeding with installation...\n\n");
                                                // Continue with installation below
                                                goto install_package;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Package exists, show available versions
                size_t versions_count = 0;
                char **versions = repository_list_versions(repo, package_name, &versions_count);
                if (versions && versions_count > 0) {
                    // Remove duplicates from versions list
                    size_t unique_count = 0;
                    char **unique_versions = malloc(sizeof(char*) * versions_count);
                    for (size_t i = 0; i < versions_count; i++) {
                        bool is_duplicate = false;
                        for (size_t j = 0; j < unique_count; j++) {
                            if (strcmp(versions[i], unique_versions[j]) == 0) {
                                is_duplicate = true;
                                break;
                            }
                        }
                        if (!is_duplicate) {
                            unique_versions[unique_count++] = strdup(versions[i]);
                        }
                    }

                    if (incomplete_version) {
                        // Show versions that match the prefix first
                        bool found_match = false;
                        fprintf(stderr, "\nVersions matching '%s*':\n", package_version);
                        for (size_t i = 0; i < unique_count; i++) {
                            if (strncmp(unique_versions[i], package_version, strlen(package_version)) == 0) {
                                fprintf(stderr, "  - %s@%s\n", package_name, unique_versions[i]);
                                found_match = true;
                            }
                        }
                        if (!found_match) {
                            fprintf(stderr, "  (no versions match '%s*')\n", package_version);
                        }
                        fprintf(stderr, "\nAll available versions for '%s':\n", package_name);
                        for (size_t i = 0; i < unique_count; i++) {
                            fprintf(stderr, "  - %s@%s\n", package_name, unique_versions[i]);
                        }
                    } else {
                        fprintf(stderr, "\nAvailable versions for '%s':\n", package_name);
                        for (size_t i = 0; i < unique_count; i++) {
                            fprintf(stderr, "  - %s@%s\n", package_name, unique_versions[i]);
                        }
                    }

                    // Free both arrays
                    for (size_t i = 0; i < unique_count; i++) {
                        free(unique_versions[i]);
                    }
                    free(unique_versions);
                    for (size_t i = 0; i < versions_count; i++) {
                        free(versions[i]);
                    }
                    free(versions);
                }
            } else {
                // Package doesn't exist at all
                fprintf(stderr, "\nPackage '%s' not found in repository.\n", package_name);
                if (repo->packages_count == 0) {
                    fprintf(stderr, "\nThe package repository is empty. Run 'tsi update' to download packages.\n");
                } else {
                    fprintf(stderr, "Use 'tsi list' to see available packages.\n");
                }
            }
        } else {
            fprintf(stderr, "Error: Package '%s' not found in repository\n", package_name);
            if (repo->packages_count == 0) {
                fprintf(stderr, "\nThe package repository is empty. Run 'tsi update' to download packages.\n");
            } else {
                fprintf(stderr, "Use 'tsi list' to see available packages.\n");
            }
        }

        // Clean up and return
        if (package_version) {
            free((char*)package_version);
            free((char*)package_name);
        }
        resolver_free(resolver);
        repository_free(repo);
        database_free(db);
        return 1;
    }

install_package:
    // Section headers will be printed by individual operations (building, installing)

    // Check if already installed (check specific version if specified)
    if (!force) {
        InstalledPackage *installed_pkg = database_get_package(db, package_name);
        if (installed_pkg) {
            // If version specified, check if that specific version is installed
            if (package_version && installed_pkg->version && strcmp(installed_pkg->version, package_version) == 0) {
                if (package_version) {
                    char warn_msg[256];
                    snprintf(warn_msg, sizeof(warn_msg), "%s@%s is already installed", package_name, package_version);
                    fprintf(stderr, "Warning: %s\n", warn_msg);
                } else {
                    char warn_msg[256];
                    snprintf(warn_msg, sizeof(warn_msg), "%s is already installed", package_name);
                    fprintf(stderr, "Warning: %s\n", warn_msg);
                }
                if (installed_pkg->install_path) {
                    printf("  Install path: %s\n", installed_pkg->install_path);
                }
                if (installed_pkg->dependencies_count > 0) {
                    printf("  Dependencies: ");
                    for (size_t i = 0; i < installed_pkg->dependencies_count; i++) {
                        printf("%s", installed_pkg->dependencies[i]);
                        if (i < installed_pkg->dependencies_count - 1) printf(", ");
                    }
                    printf("\n");
                }
                printf("\nUse --force to reinstall.\n");
                resolver_free(resolver);
                repository_free(repo);
                database_free(db);
                if (package_version) {
                    free((char*)package_version);
                    free((char*)package_name);
                }
                return 0;
            } else if (!package_version) {
                // No version specified, but package is installed
                char warn_msg[256];
                snprintf(warn_msg, sizeof(warn_msg), "%s is already installed", package_name);
                fprintf(stderr, "Warning: %s\n", warn_msg);
                if (installed_pkg->version) {
                    printf("  Version: %s\n", installed_pkg->version);
                }
                if (installed_pkg->install_path) {
                    printf("  Install path: %s\n", installed_pkg->install_path);
                }
                if (installed_pkg->dependencies_count > 0) {
                    printf("  Dependencies: ");
                    for (size_t i = 0; i < installed_pkg->dependencies_count; i++) {
                        printf("%s", installed_pkg->dependencies[i]);
                        if (i < installed_pkg->dependencies_count - 1) printf(", ");
                    }
                    printf("\n");
                }
                printf("\nUse --force to reinstall, or specify version with %s@<version>\n", package_name);
                resolver_free(resolver);
                repository_free(repo);
                database_free(db);
                if (package_version) {
                    free((char*)package_version);
                    free((char*)package_name);
                }
                return 0;
            }
        }
    }

    // Get installed packages list (skip if force is enabled)
    size_t installed_count = 0;
    char **installed = NULL;
    if (!force) {
        installed = database_list_installed(db, &installed_count);
    }

    // Resolve dependencies
    log_debug("Resolving dependencies for package: %s@%s", package_name, package_version ? package_version : "latest");
    log_developer("Installed packages count: %zu", installed_count);
    size_t deps_count = 0;
    char **deps = resolver_resolve(resolver, package_name, installed, installed_count, &deps_count);
    log_debug("Dependency resolution returned %zu dependencies", deps_count);

    if (!deps) {
        // Package exists but dependency resolution failed
        log_error("Failed to resolve dependencies for package: %s@%s", package_name, package_version ? package_version : "latest");
        fprintf(stderr, "Error: Failed to resolve dependencies for '%s'\n", package_name);
        if (installed) {
            for (size_t i = 0; i < installed_count; i++) free(installed[i]);
            free(installed);
        }
        if (package_version) {
            free((char*)package_version);
            free((char*)package_name);
        }
        resolver_free(resolver);
        repository_free(repo);
        database_free(db);
        return 1;
    }

    if (deps_count > 0) {
        printf("==> Resolving dependencies");
        if (config_is_strict_isolation()) {
            printf(" (isolated)");
        }
        printf("\n");

        // Count actual dependencies (excluding the main package itself)
        size_t actual_deps_count = 0;
        for (size_t i = 0; i < deps_count; i++) {
            // Extract package name from deps[i] (may be package@version)
            char *dep_name = NULL;
            char *dep_version = NULL;
            char *at_pos = strchr(deps[i], '@');
            if (at_pos) {
                size_t name_len = at_pos - deps[i];
                dep_name = malloc(name_len + 1);
                if (dep_name) {
                    strncpy(dep_name, deps[i], name_len);
                    dep_name[name_len] = '\0';
                }
            } else {
                dep_name = strdup(deps[i]);
            }

            // Compare with main package name (extract name from package_name too)
            char *main_name = NULL;
            char *main_version = NULL;
            at_pos = strchr((char*)package_name, '@');
            if (at_pos) {
                size_t name_len = at_pos - (char*)package_name;
                main_name = malloc(name_len + 1);
                if (main_name) {
                    strncpy(main_name, package_name, name_len);
                    main_name[name_len] = '\0';
                }
            } else {
                main_name = strdup(package_name);
            }

            if (dep_name && main_name && strcmp(dep_name, main_name) != 0) {
                actual_deps_count++;
            }

            if (dep_name) free(dep_name);
            if (dep_version) free(dep_version);
            if (main_name) free(main_name);
            if (main_version) free(main_version);
        }

        if (actual_deps_count > 0) {
            printf("  Resolved %zu dependency%s: ", actual_deps_count, actual_deps_count == 1 ? "" : "s");
            bool first = true;
            for (size_t i = 0; i < deps_count; i++) {
                // Extract package name from deps[i]
                char *dep_name = NULL;
                char *at_pos = strchr(deps[i], '@');
                if (at_pos) {
                    size_t name_len = at_pos - deps[i];
                    dep_name = malloc(name_len + 1);
                    if (dep_name) {
                        strncpy(dep_name, deps[i], name_len);
                        dep_name[name_len] = '\0';
                    }
                } else {
                    dep_name = strdup(deps[i]);
                }

                // Extract main package name
                char *main_name = NULL;
                at_pos = strchr((char*)package_name, '@');
                if (at_pos) {
                    size_t name_len = at_pos - (char*)package_name;
                    main_name = malloc(name_len + 1);
                    if (main_name) {
                        strncpy(main_name, package_name, name_len);
                        main_name[name_len] = '\0';
                    }
                } else {
                    main_name = strdup(package_name);
                }

                // Only print if it's not the main package
                if (dep_name && main_name && strcmp(dep_name, main_name) != 0) {
                    if (!first) printf(", ");
                    printf("%s", deps[i]);
                    first = false;
                }

                if (dep_name) free(dep_name);
                if (main_name) free(main_name);
            }
            printf("\n\n");
        }
    }

    // Get build order
    log_developer("About to call resolver_get_build_order: deps=%p, deps_count=%zu", (void*)deps, deps_count);
    if (deps && deps_count > 0) {
        log_developer("Deps array contents:");
        for (size_t i = 0; i < deps_count; i++) {
            log_developer("  deps[%zu]='%s'", i, deps[i] ? deps[i] : "(NULL)");
        }
    }
    size_t build_order_count = 0;
    log_developer("Before resolver_get_build_order: deps=%p, deps_count=%zu, build_order_count=%zu", (void*)deps, deps_count, build_order_count);
    char **build_order = resolver_get_build_order(resolver, deps, deps_count, &build_order_count);
    log_developer("After resolver_get_build_order: build_order=%p, build_order_count=%zu (address of count: %p)", (void*)build_order, build_order_count, (void*)&build_order_count);

    // CRITICAL FIX: If build_order is not NULL but count is 0, use deps_count as fallback
    // This handles the case where the function successfully creates the array but the count gets lost
    if (build_order && build_order_count == 0 && deps_count > 0) {
        log_warning("build_order is not NULL but count is 0! Using deps_count=%zu as fallback", deps_count);
        build_order_count = deps_count;
    }

    if (!build_order) {
        fprintf(stderr, "Error: Failed to determine build order\n");
        if (deps_count > 0) {
            fprintf(stderr, "  Packages: ");
            for (size_t i = 0; i < deps_count; i++) {
                fprintf(stderr, "%s ", deps[i]);
            }
            fprintf(stderr, "\n");
            // Check if packages exist in repository
            for (size_t i = 0; i < deps_count; i++) {
                Package *pkg = repository_get_package(repo, deps[i]);
                if (!pkg) {
                    char warn_msg[256];
                    snprintf(warn_msg, sizeof(warn_msg), "Package '%s' not found in repository", deps[i]);
                    fprintf(stderr, "Warning: %s\n", warn_msg);
                }
            }
        }
        for (size_t i = 0; i < deps_count; i++) free(deps[i]);
        free(deps);
        if (installed) {
            for (size_t i = 0; i < installed_count; i++) free(installed[i]);
            free(installed);
        }
        resolver_free(resolver);
        repository_free(repo);
        database_free(db);
        return 1;
    }

    if (build_order_count > 0) {
        printf("==> Build order");
        if (config_is_strict_isolation()) {
            printf(" (isolated)");
        }
        printf("\n");
        log_debug("Build order calculated: %zu packages", build_order_count);
        for (size_t i = 0; i < build_order_count; i++) {
            if (build_order[i]) {
            printf("  %zu. %s\n", i + 1, build_order[i]);
                log_developer("  Build order[%zu] = '%s'", i, build_order[i]);
            } else {
                printf("  %zu. (null)\n", i + 1);
                log_warning("  Build order[%zu] is NULL!", i);
            }
        }
    }

    BuilderConfig *builder_config = builder_config_new(tsi_prefix);
    if (!builder_config) {
        fprintf(stderr, "Error: Failed to initialize builder config\n");
        for (size_t i = 0; i < deps_count; i++) free(deps[i]);
        free(deps);
        for (size_t i = 0; i < build_order_count; i++) free(build_order[i]);
        free(build_order);
        if (installed) {
            for (size_t i = 0; i < installed_count; i++) free(installed[i]);
            free(installed);
        }
        resolver_free(resolver);
        repository_free(repo);
        database_free(db);
        return 1;
    }

    char source_dir[1024];
    len = snprintf(source_dir, sizeof(source_dir), "%s/sources", tsi_prefix);
    if (len < 0 || (size_t)len >= sizeof(source_dir)) {
        fprintf(stderr, "Error: Path too long\n");
        builder_config_free(builder_config);
        for (size_t i = 0; i < deps_count; i++) free(deps[i]);
        free(deps);
        for (size_t i = 0; i < build_order_count; i++) free(build_order[i]);
        free(build_order);
        if (installed) {
            for (size_t i = 0; i < installed_count; i++) free(installed[i]);
            free(installed);
        }
        resolver_free(resolver);
        repository_free(repo);
        database_free(db);
        return 1;
    }
    SourceFetcher *fetcher = fetcher_new(source_dir);
    if (!fetcher) {
        fprintf(stderr, "Error: Failed to initialize fetcher\n");
        builder_config_free(builder_config);
        for (size_t i = 0; i < deps_count; i++) free(deps[i]);
        free(deps);
        for (size_t i = 0; i < build_order_count; i++) free(build_order[i]);
        free(build_order);
        if (installed) {
            for (size_t i = 0; i < installed_count; i++) free(installed[i]);
            free(installed);
        }
        resolver_free(resolver);
        repository_free(repo);
        database_free(db);
        return 1;
    }

    // Track failures
    bool has_failures = false;
    int failed_deps_count = 0;
    char **failed_deps = NULL;

    // Count dependencies (excluding main package)
    log_developer("About to count dependencies: build_order=%p, build_order_count=%zu, package_name='%s'",
                  (void*)build_order, build_order_count, package_name);
    if (!build_order) {
        log_error("build_order is NULL! This should not happen.");
    }
    size_t dependency_count = 0;
    log_developer("Counting dependencies from build_order (count=%zu, package_name='%s')", build_order_count, package_name);
    for (size_t i = 0; i < build_order_count; i++) {
        if (!build_order[i]) {
            log_warning("  build_order[%zu] is NULL, skipping", i);
            continue;
        }
        bool is_main = package_name_matches(build_order[i], package_name);
        log_developer("  build_order[%zu]='%s' matches main package: %s", i, build_order[i], is_main ? "yes" : "no");
        if (!is_main) {
            dependency_count++;
        }
    }
    log_developer("Total dependency_count: %zu", dependency_count);

    // Install dependencies first
    if (dependency_count > 0) {
        printf("==> Installing dependencies");
        if (config_is_strict_isolation()) {
            printf(" (isolated)");
        }
        printf("\n");
        log_info("Installing %zu dependencies before main package", dependency_count);
    } else {
        log_warning("No dependencies to install (dependency_count=0, build_order_count=%zu)", build_order_count);
    }

    size_t current_dep = 0;
    for (size_t i = 0; i < build_order_count; i++) {
        if (!build_order[i]) {
            log_warning("Skipping NULL build_order[%zu]", i);
            continue;
        }
        if (package_name_matches(build_order[i], package_name)) {
            continue; // Install main package last
        }

        current_dep++;
        if (dependency_count > 1) {
            char dep_msg[256];
            snprintf(dep_msg, sizeof(dep_msg), "Installing dependency %zu of %zu", current_dep, dependency_count);
            printf("Building dependency: %s\n", build_order[i]);
        } else {
            printf("Installing dependency: %s\n", build_order[i]);
        }

        // Parse package@version from build_order if present
        char *dep_name = NULL;
        char *dep_version = NULL;
        char *at_pos = strchr(build_order[i], '@');
        if (at_pos) {
            size_t name_len = at_pos - build_order[i];
            dep_name = malloc(name_len + 1);
            if (dep_name) {
                strncpy(dep_name, build_order[i], name_len);
                dep_name[name_len] = '\0';
            }
            dep_version = strdup(at_pos + 1);
        } else {
            dep_name = strdup(build_order[i]);
        }

        Package *dep_pkg = dep_version ? repository_get_package_version(repo, dep_name ? dep_name : build_order[i], dep_version) : repository_get_package(repo, dep_name ? dep_name : build_order[i]);

        if (dep_name) free(dep_name);
        if (dep_version) free(dep_version);
        if (!dep_pkg) {
            char warn_msg[256];
            snprintf(warn_msg, sizeof(warn_msg), "Dependency package not found: %s", build_order[i]);
            fprintf(stderr, "Error: %s\n", warn_msg);
            log_error("Dependency package not found: %s", build_order[i]);
            has_failures = true;
            failed_deps = realloc(failed_deps, sizeof(char*) * (failed_deps_count + 1));
            if (failed_deps) {
                failed_deps[failed_deps_count++] = strdup(build_order[i]);
            }
            // Abort on error - don't continue building
            log_error("Aborting installation due to missing dependency");
            goto cleanup;
        }

        // Fetch source
        log_debug("Fetching source for dependency: %s@%s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest");
        char *dep_source_dir = fetcher_fetch(fetcher, dep_pkg, force);
        if (!dep_source_dir) {
            fprintf(stderr, "Error: Failed to fetch source for %s\n", build_order[i]);
            log_error("Failed to fetch source for dependency: %s@%s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest");
            has_failures = true;
            failed_deps = realloc(failed_deps, sizeof(char*) * (failed_deps_count + 1));
            if (failed_deps) {
                failed_deps[failed_deps_count++] = strdup(build_order[i]);
            }
            // Abort on error - don't continue building
            log_error("Aborting installation due to fetch failure");
            goto cleanup;
        }
        log_developer("Source fetched for dependency: %s@%s -> %s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest", dep_source_dir);

        // Set package-specific install directory
        builder_config_set_package_dir(builder_config, dep_pkg->name, dep_pkg->version);

        // Build
        printf("==> Building %s", dep_pkg->name);
        if (dep_pkg->version && dep_pkg->version[0]) {
            printf(" %s", dep_pkg->version);
        }
        printf("\n");
        char build_dir[1024];
        if (dep_pkg->version && strcmp(dep_pkg->version, "latest") != 0) {
            snprintf(build_dir, sizeof(build_dir), "%s/%s-%s", builder_config->build_dir, dep_pkg->name, dep_pkg->version);
        } else {
            snprintf(build_dir, sizeof(build_dir), "%s/%s", builder_config->build_dir, dep_pkg->name);
        }

        log_debug("Building dependency: %s@%s in %s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest", build_dir);
        if (!builder_build_with_output(builder_config, dep_pkg, dep_source_dir, build_dir, output_callback, NULL)) {
            fprintf(stderr, "Error: Failed to build dependency\n");
            fprintf(stderr, "  %s\n", build_order[i]);
            log_error("Failed to build dependency: %s@%s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest");
            has_failures = true;
            failed_deps = realloc(failed_deps, sizeof(char*) * (failed_deps_count + 1));
            if (failed_deps) {
                failed_deps[failed_deps_count++] = strdup(build_order[i]);
            }
            free(dep_source_dir);
            // Abort on error - don't continue building
            log_error("Aborting installation due to build failure");
            goto cleanup;
        }
        log_info("Successfully built dependency: %s@%s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest");

        // Install
        printf("==> Installing %s", dep_pkg->name);
        if (dep_pkg->version && dep_pkg->version[0]) {
            printf(" %s", dep_pkg->version);
        }
        printf("\n");

        log_debug("Installing dependency: %s@%s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest");
        if (!builder_install_with_output(builder_config, dep_pkg, dep_source_dir, build_dir, output_callback, NULL)) {
            fprintf(stderr, "Error: Failed to install dependency\n");
            fprintf(stderr, "  %s\n", build_order[i]);
            log_error("Failed to install dependency: %s@%s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest");
            has_failures = true;
            failed_deps = realloc(failed_deps, sizeof(char*) * (failed_deps_count + 1));
            if (failed_deps) {
                failed_deps[failed_deps_count++] = strdup(build_order[i]);
            }
            free(dep_source_dir);
            // Abort on error - don't continue installing
            log_error("Aborting installation due to install failure");
            goto cleanup;
        }
        log_info("Successfully installed dependency: %s@%s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest");

        // Show completion
        char done_msg[256];
        if (dep_pkg->version) {
            snprintf(done_msg, sizeof(done_msg), "Installed %s %s", dep_pkg->name, dep_pkg->version);
        } else {
            snprintf(done_msg, sizeof(done_msg), "Installed %s", dep_pkg->name);
        }
        printf("%s\n", done_msg);

        // Create symlinks to main install directory
        log_developer("Creating symlinks for dependency: %s@%s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest");
        builder_create_symlinks(builder_config, dep_pkg->name, dep_pkg->version);

        // Record in database with package-specific path
        log_debug("Recording dependency in database: %s@%s -> %s", dep_pkg->name, dep_pkg->version ? dep_pkg->version : "latest", builder_config->install_dir);
        database_add_package(db, dep_pkg->name, dep_pkg->version, builder_config->install_dir, (const char **)dep_pkg->dependencies, dep_pkg->dependencies_count);
        free(dep_source_dir);
    }

    // Install main package
    if (dependency_count > 0) {
        printf("\n");  // Add spacing after dependencies
    }
    printf("==> Installing package");
    if (config_is_strict_isolation()) {
        printf(" (isolated)");
    }
    printf("\n");
    printf("Installing: %s\n", package_name);
    log_info("Installing main package: %s@%s", package_name, package_version ? package_version : "latest");
    Package *main_pkg = package_version ? repository_get_package_version(repo, package_name, package_version) : repository_get_package(repo, package_name);
    if (main_pkg) {
        // Set package-specific install directory
        builder_config_set_package_dir(builder_config, main_pkg->name, main_pkg->version);

        log_debug("Fetching source for main package: %s@%s", main_pkg->name, main_pkg->version ? main_pkg->version : "latest");
        char *main_source_dir = fetcher_fetch(fetcher, main_pkg, force);
        if (main_source_dir) {
            log_developer("Source fetched for main package: %s@%s -> %s", main_pkg->name, main_pkg->version ? main_pkg->version : "latest", main_source_dir);
            char build_dir[1024];
            snprintf(build_dir, sizeof(build_dir), "%s/%s", builder_config->build_dir, main_pkg->name);

            printf("==> Building %s", main_pkg->name);
            if (main_pkg->version && main_pkg->version[0]) {
                printf(" %s", main_pkg->version);
            }
            printf("\n");

            log_debug("Building main package: %s@%s in %s", main_pkg->name, main_pkg->version ? main_pkg->version : "latest", build_dir);
            if (builder_build_with_output(builder_config, main_pkg, main_source_dir, build_dir, output_callback, NULL)) {
                log_info("Successfully built main package: %s@%s", main_pkg->name, main_pkg->version ? main_pkg->version : "latest");
                printf("==> Installing %s", main_pkg->name);
                if (main_pkg->version && main_pkg->version[0]) {
                    printf(" %s", main_pkg->version);
                }
                printf("\n");

                log_debug("Installing main package: %s@%s", main_pkg->name, main_pkg->version ? main_pkg->version : "latest");
                if (builder_install_with_output(builder_config, main_pkg, main_source_dir, build_dir, output_callback, NULL)) {
                    log_info("Successfully installed main package: %s@%s", main_pkg->name, main_pkg->version ? main_pkg->version : "latest");
                    // Create symlinks to main install directory
                    log_developer("Creating symlinks for main package: %s@%s", main_pkg->name, main_pkg->version ? main_pkg->version : "latest");
                    builder_create_symlinks(builder_config, main_pkg->name, main_pkg->version);

                    // Record in database with package-specific path
                    log_debug("Recording main package in database: %s@%s -> %s", main_pkg->name, main_pkg->version ? main_pkg->version : "latest", builder_config->install_dir);
                    database_add_package(db, main_pkg->name, main_pkg->version, builder_config->install_dir, (const char **)main_pkg->dependencies, main_pkg->dependencies_count);

                    // Show completion
                    char done_msg[256];
                    if (main_pkg->version) {
                        snprintf(done_msg, sizeof(done_msg), "Installed %s %s", main_pkg->name, main_pkg->version);
                    } else {
                        snprintf(done_msg, sizeof(done_msg), "Installed %s", main_pkg->name);
                    }
                    printf("%s\n", done_msg);
                    log_info("Successfully installed package: %s@%s", main_pkg->name, main_pkg->version ? main_pkg->version : "latest");

                    // Show summary
                    printf("Installed to: %s\n", builder_config->install_dir);

                    // Show description (if any)
                    if (main_pkg->description) {
                        printf("Description: %s\n", main_pkg->description);
                    }
                } else {
                    fprintf(stderr, "Error: Failed to install package\n");
                    if (package_version) {
                        fprintf(stderr, "  %s@%s\n", package_name, package_version);
                        log_error("Failed to install package: %s@%s", package_name, package_version);
                    } else {
                        fprintf(stderr, "  %s\n", package_name);
                        log_error("Failed to install package: %s", package_name);
                    }
                    has_failures = true;
                }
            } else {
                fprintf(stderr, "Error: Failed to build package\n");
                if (package_version) {
                    fprintf(stderr, "  %s@%s\n", package_name, package_version);
                    log_error("Failed to build package: %s@%s", package_name, package_version);
                } else {
                    fprintf(stderr, "  %s\n", package_name);
                    log_error("Failed to build package: %s", package_name);
                }
                has_failures = true;
            }
            free(main_source_dir);
        } else {
            fprintf(stderr, "Error: Failed to fetch source\n");
            if (package_version) {
                fprintf(stderr, "  %s@%s\n", package_name, package_version);
                log_error("Failed to fetch source for package: %s@%s", package_name, package_version);
            } else {
                fprintf(stderr, "  %s\n", package_name);
                log_error("Failed to fetch source for package: %s", package_name);
            }
            has_failures = true;
        }
    } else {
        fprintf(stderr, "Error: Package not found\n");
        fprintf(stderr, "  %s\n", package_name);
        has_failures = true;
    }

cleanup:
    // Clean up failed dependencies list
    if (failed_deps) {
        for (int i = 0; i < failed_deps_count; i++) {
            free(failed_deps[i]);
        }
        free(failed_deps);
    }

    // Report summary and exit with error code if there were failures
    if (has_failures) {
        printf("\n");
        fprintf(stderr, "Error: Installation completed with errors\n");
        if (failed_deps_count > 0) {
            printf("Failed dependencies: %d\n", failed_deps_count);
        }
        builder_config_free(builder_config);
        fetcher_free(fetcher);
        for (size_t i = 0; i < deps_count; i++) free(deps[i]);
        free(deps);
        for (size_t i = 0; i < build_order_count; i++) free(build_order[i]);
        free(build_order);
        repository_free(repo);
        database_free(db);
        return 1;
    }

    builder_config_free(builder_config);
    fetcher_free(fetcher);

    // Free resources
    for (size_t i = 0; i < deps_count; i++) {
        free(deps[i]);
    }
    free(deps);

    for (size_t i = 0; i < build_order_count; i++) {
        free(build_order[i]);
    }
    free(build_order);

    // Cleanup logging
    log_cleanup();

    if (installed) {
        for (size_t i = 0; i < installed_count; i++) {
            free(installed[i]);
        }
        free(installed);
    }

    resolver_free(resolver);
    repository_free(repo);
    database_free(db);

    return 0;
}

static int cmd_list(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char tsi_prefix[1024];
    get_tsi_prefix_with_fallback(tsi_prefix, sizeof(tsi_prefix), NULL);

    char db_dir[1024];
    snprintf(db_dir, sizeof(db_dir), "%s/db", tsi_prefix);

    Database *db = database_new(db_dir);
    if (!db) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }

    size_t count = 0;
    char **packages = database_list_installed(db, &count);

    if (count == 0) {
        printf("No packages installed.\n");
    } else {
        printf("Installed packages:\n");
        for (size_t i = 0; i < count; i++) {
            InstalledPackage *pkg = database_get_package(db, packages[i]);
            if (pkg) {
                printf("  %s (%s)\n", pkg->name, pkg->version);
            }
            free(packages[i]);
        }
    }

    if (packages) free(packages);
    database_free(db);
    return 0;
}

static int cmd_versions(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: package name required\n");
        fprintf(stderr, "Usage: tsi versions <package>\n");
        return 1;
    }

    const char *package_name = argv[1];
    char tsi_prefix[1024];
    get_tsi_prefix_with_fallback(tsi_prefix, sizeof(tsi_prefix), NULL);

    char repo_dir[1024];
    snprintf(repo_dir, sizeof(repo_dir), "%s/packages", tsi_prefix);

    Repository *repo = repository_new(repo_dir);
    if (!repo) {
        fprintf(stderr, "Failed to initialize repository\n");
        return 1;
    }

    // Check if package exists
    Package *pkg = repository_get_package(repo, package_name);
    if (!pkg) {
        fprintf(stderr, "Package '%s' not found in repository.\n", package_name);
        fprintf(stderr, "Use 'tsi list' to see available packages.\n");
        repository_free(repo);
        return 1;
    }

    // List all available versions
    size_t versions_count = 0;
    char **versions = repository_list_versions(repo, package_name, &versions_count);

    if (!versions || versions_count == 0) {
        fprintf(stderr, "No versions found for package '%s'\n", package_name);
        repository_free(repo);
        return 1;
    }

    // Remove duplicates
    size_t unique_count = 0;
    char **unique_versions = malloc(sizeof(char*) * versions_count);
    if (!unique_versions) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        for (size_t i = 0; i < versions_count; i++) {
            free(versions[i]);
        }
        free(versions);
        repository_free(repo);
        return 1;
    }

    for (size_t i = 0; i < versions_count; i++) {
        bool is_duplicate = false;
        for (size_t j = 0; j < unique_count; j++) {
            if (strcmp(versions[i], unique_versions[j]) == 0) {
                is_duplicate = true;
                break;
            }
        }
        if (!is_duplicate) {
            unique_versions[unique_count++] = strdup(versions[i]);
        }
    }

    printf("==> Available versions\n");
    printf("  %s\n", package_name);
    for (size_t i = 0; i < unique_count; i++) {
        printf("  %s\n", unique_versions[i]);
        free(unique_versions[i]);
    }
    free(unique_versions);

    // Free versions array
    for (size_t i = 0; i < versions_count; i++) {
        free(versions[i]);
    }
    free(versions);

    repository_free(repo);
    return 0;
}

static int cmd_info(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: package name required\n");
        return 1;
    }

    const char *package_name = argv[1];
    char tsi_prefix[1024];
    get_tsi_prefix_with_fallback(tsi_prefix, sizeof(tsi_prefix), NULL);

    char repo_dir[1024];
    snprintf(repo_dir, sizeof(repo_dir), "%s/packages", tsi_prefix);

    Repository *repo = repository_new(repo_dir);
    if (!repo) {
        fprintf(stderr, "Failed to initialize repository\n");
        return 1;
    }

    // Check if version is specified in package_name
    const char *version = NULL;
    char *at_pos = strchr((char*)package_name, '@');
    char *actual_name = (char*)package_name;
    char *allocated_name = NULL;
    if (at_pos) {
        size_t name_len = at_pos - package_name;
        allocated_name = malloc(name_len + 1);
        if (!allocated_name) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            repository_free(repo);
            return 1;
        }
        strncpy(allocated_name, package_name, name_len);
        allocated_name[name_len] = '\0';
        actual_name = allocated_name;
        version = strdup(at_pos + 1);
        if (!version) {
            free(allocated_name);
            fprintf(stderr, "Error: Memory allocation failed\n");
            repository_free(repo);
            return 1;
        }
    }

    // Check if version string is incomplete (ends with dot, is empty, or doesn't match any exact version)
    bool incomplete_version = false;
    if (version) {
        size_t version_len = strlen(version);
        if (version_len == 0 || version[version_len - 1] == '.') {
            incomplete_version = true;
        } else {
            // Check if it's a prefix (doesn't match any exact version but might match some)
            Package *exact_match = repository_get_package_version(repo, actual_name, version);
            if (!exact_match) {
                // Not an exact match - check if any versions start with this prefix
                size_t versions_count = 0;
                char **versions = repository_list_versions(repo, actual_name, &versions_count);
                if (versions && versions_count > 0) {
                    bool has_prefix_match = false;
                    for (size_t i = 0; i < versions_count; i++) {
                        if (strncmp(versions[i], version, strlen(version)) == 0) {
                            has_prefix_match = true;
                            break;
                        }
                    }
                    if (has_prefix_match) {
                        incomplete_version = true;
                    }
                    // Free versions array
                    for (size_t i = 0; i < versions_count; i++) {
                        free(versions[i]);
                    }
                    free(versions);
                }
            }
        }
    }

    Package *pkg = NULL;
    if (!incomplete_version) {
        pkg = version ? repository_get_package_version(repo, actual_name, version) : repository_get_package(repo, actual_name);
    }

    if (!pkg || incomplete_version) {
        if (version) {
            if (incomplete_version) {
                fprintf(stderr, "Error: Incomplete version specification '%s@%s'\n", actual_name, version);
            } else {
                fprintf(stderr, "Package not found: %s@%s\n", actual_name, version);
            }

            // Check if package exists (but version doesn't or is incomplete)
            Package *any_version = repository_get_package(repo, actual_name);
            if (any_version) {
                // Package exists, show available versions
                size_t versions_count = 0;
                char **versions = repository_list_versions(repo, actual_name, &versions_count);
                if (versions && versions_count > 0) {
                    // Remove duplicates from versions list
                    size_t unique_count = 0;
                    char **unique_versions = malloc(sizeof(char*) * versions_count);
                    for (size_t i = 0; i < versions_count; i++) {
                        bool is_duplicate = false;
                        for (size_t j = 0; j < unique_count; j++) {
                            if (strcmp(versions[i], unique_versions[j]) == 0) {
                                is_duplicate = true;
                                break;
                            }
                        }
                        if (!is_duplicate) {
                            unique_versions[unique_count++] = strdup(versions[i]);
                        }
                    }

                    if (incomplete_version) {
                        // Show versions that match the prefix first
                        bool found_match = false;
                        fprintf(stderr, "\nVersions matching '%s*':\n", version);
                        for (size_t i = 0; i < unique_count; i++) {
                            if (strncmp(unique_versions[i], version, strlen(version)) == 0) {
                                fprintf(stderr, "  - %s@%s\n", actual_name, unique_versions[i]);
                                found_match = true;
                            }
                        }
                        if (!found_match) {
                            fprintf(stderr, "  (no versions match '%s*')\n", version);
                        }
                        fprintf(stderr, "\nAll available versions for '%s':\n", actual_name);
                        for (size_t i = 0; i < unique_count; i++) {
                            fprintf(stderr, "  - %s@%s\n", actual_name, unique_versions[i]);
                        }
                    } else {
                        fprintf(stderr, "\nAvailable versions for '%s':\n", actual_name);
                        for (size_t i = 0; i < unique_count; i++) {
                            fprintf(stderr, "  - %s@%s\n", actual_name, unique_versions[i]);
                        }
                    }

                    // Free both arrays
                    for (size_t i = 0; i < unique_count; i++) {
                        free(unique_versions[i]);
                    }
                    free(unique_versions);
                    for (size_t i = 0; i < versions_count; i++) {
                        free(versions[i]);
                    }
                    free(versions);
                }
            } else {
                fprintf(stderr, "Package '%s' not found in repository.\n", actual_name);
                fprintf(stderr, "Use 'tsi list' to see available packages.\n");
            }
        } else {
            fprintf(stderr, "Package not found: %s\n", actual_name);
            fprintf(stderr, "Use 'tsi list' to see available packages.\n");
        }
        if (at_pos) {
            free(allocated_name);
            free((char*)version);
        }
        repository_free(repo);
        return 1;
    }

    printf("==> Package Information\n");
    if (pkg->name) {
        if (pkg->version) {
            printf("  %s %s\n", pkg->name, pkg->version);
        } else {
            printf("  %s\n", pkg->name);
        }
    }
    printf("Version: %s\n", pkg->version ? pkg->version : "unknown");

    // List all available versions
    size_t versions_count = 0;
    char **versions = repository_list_versions(repo, pkg->name, &versions_count);
    if (versions && versions_count > 1) {
        printf("Available versions: ");
        for (size_t i = 0; i < versions_count; i++) {
            if (pkg->version && strcmp(versions[i], pkg->version) == 0) {
                printf("[%s]", versions[i]);
            } else {
                printf("%s", versions[i]);
            }
            if (i < versions_count - 1) printf(", ");
        }
        printf("\n");
        for (size_t i = 0; i < versions_count; i++) {
            free(versions[i]);
        }
        free(versions);
    }
    printf("Description: %s\n", pkg->description ? pkg->description : "");
    printf("Build System: %s\n", pkg->build_system ? pkg->build_system : "unknown");

    if (pkg->dependencies_count > 0) {
        printf("Dependencies: ");
        for (size_t i = 0; i < pkg->dependencies_count; i++) {
            printf("%s", pkg->dependencies[i]);
            if (i < pkg->dependencies_count - 1) printf(", ");
        }
        printf("\n");
    }

    // Check if package is installed
    // tsi_prefix already declared at the start of this function
    char db_dir[1024];
    snprintf(db_dir, sizeof(db_dir), "%s/db", tsi_prefix);
    Database *db = database_new(db_dir);
    if (db) {
        InstalledPackage *installed_pkg = database_get_package(db, pkg->name);
        if (installed_pkg) {
            // Check if the requested version matches the installed version
            bool version_matches = false;
            if (version && installed_pkg->version) {
                version_matches = (strcmp(installed_pkg->version, version) == 0);
            } else if (!version && installed_pkg->version) {
                version_matches = true; // No version specified, show any installed version
            }

            if (version_matches || !version) {
                printf("\nInstallation Status: Installed\n");
                printf("  Installed Version: %s\n", installed_pkg->version ? installed_pkg->version : "unknown");
                printf("  Install Path: %s\n", installed_pkg->install_path ? installed_pkg->install_path : "unknown");
            }
        }
        database_free(db);
    }

    repository_free(repo);
    if (at_pos) {
        free(allocated_name);
        free((char*)version);
    }
    return 0;
}

static int cmd_update(int argc, char **argv) {

    const char *repo_url = NULL;
    const char *local_path = NULL;
    const char *prefix = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--repo") == 0 && i + 1 < argc) {
            repo_url = argv[++i];
        } else if (strcmp(argv[i], "--local") == 0 && i + 1 < argc) {
            local_path = argv[++i];
        } else if (strcmp(argv[i], "--prefix") == 0 && i + 1 < argc) {
            prefix = argv[++i];
        }
    }

    char tsi_prefix[1024];
    get_tsi_prefix_with_fallback(tsi_prefix, sizeof(tsi_prefix), prefix);

    char repo_dir[1024];
    int len = snprintf(repo_dir, sizeof(repo_dir), "%s/packages", tsi_prefix);
    if (len < 0 || (size_t)len >= sizeof(repo_dir)) {
        fprintf(stderr, "Error: Path too long\n");
        return 1;
    }

    // Create repo directory if it doesn't exist
    // Use absolute path to mkdir to avoid PATH issues
    char cmd[2048];
    int cmd_len = snprintf(cmd, sizeof(cmd), "/bin/mkdir -p '%s' 2>/dev/null || /usr/bin/mkdir -p '%s' 2>/dev/null || true", repo_dir, repo_dir);
    if (cmd_len >= 0 && (size_t)cmd_len < sizeof(cmd)) {
        system(cmd);
    }

    printf("==> Updating package repository\n");
    printf("  Repository directory: %s\n", repo_dir);

    bool success = false;

    // Update from local path
    if (local_path) {
        printf("==> Updating from local path\n");
        printf("  %s\n", local_path);
        char copy_cmd[2048];
        int copy_cmd_len = snprintf(copy_cmd, sizeof(copy_cmd), "cp '%s'/*.json '%s/' 2>/dev/null", local_path, repo_dir);
        if (copy_cmd_len >= 0 && (size_t)copy_cmd_len < sizeof(copy_cmd)) {
            if (run_command_with_window("Copying packages", local_path, copy_cmd)) {
                printf("Packages copied from local path\n");
                success = true;
            } else {
                fprintf(stderr, "Error: Failed to copy packages from local path\n");
            }
        }
    }
    // Update from git repository
    else if (repo_url) {
        printf("Updating from repository: %s\n", repo_url);
        char temp_dir[1024];
        int temp_dir_len = snprintf(temp_dir, sizeof(temp_dir), "%s/tmp-repo-update", tsi_prefix);
        if (temp_dir_len < 0 || (size_t)temp_dir_len >= sizeof(temp_dir)) {
            fprintf(stderr, "Error: Path too long\n");
            return 1;
        }

        // Clone or update repository
        char git_cmd[2048];
        struct stat st;
        int git_cmd_len;
        if (stat(temp_dir, &st) == 0) {
            // Update existing clone
            git_cmd_len = snprintf(git_cmd, sizeof(git_cmd), "cd '%s' && git pull 2>&1", temp_dir);
        } else {
            // Clone repository
            git_cmd_len = snprintf(git_cmd, sizeof(git_cmd), "git clone --depth 1 '%s' '%s' 2>&1", repo_url, temp_dir);
        }
        if (git_cmd_len < 0 || (size_t)git_cmd_len >= sizeof(git_cmd)) {
            fprintf(stderr, "Error: Command too long\n");
            return 1;
        }

        // Check if git is available before trying to use it
        // Use a safer check that won't hang
        FILE *git_check = popen("command -v git 2>/dev/null", "r");
        bool git_available = false;
        if (git_check) {
            char result[256];
            if (fgets(result, sizeof(result), git_check)) {
                git_available = true;
            }
            pclose(git_check);
        }
        if (!git_available) {
            fprintf(stderr, "Error: git is not installed or not in PATH\n");
            fprintf(stderr, "Please install git to update the package repository\n");
            return 1;
        }

        if (run_command_with_window("Syncing repository", repo_url, git_cmd)) {
            // Copy package files
            char packages_dir[1024];
            int packages_dir_len = snprintf(packages_dir, sizeof(packages_dir), "%s/packages", temp_dir);
            if (packages_dir_len < 0 || (size_t)packages_dir_len >= sizeof(packages_dir)) {
                fprintf(stderr, "Error: Path too long\n");
                return 1;
            }

            // Check if packages directory exists, otherwise try root
            if (stat(packages_dir, &st) != 0) {
                strncpy(packages_dir, temp_dir, sizeof(packages_dir) - 1);
                packages_dir[sizeof(packages_dir) - 1] = '\0';
            }

            char copy_cmd[2048];
            int copy_cmd_len = snprintf(copy_cmd, sizeof(copy_cmd), "cp '%s'/*.json '%s/' 2>/dev/null", packages_dir, repo_dir);
            if (copy_cmd_len < 0 || (size_t)copy_cmd_len >= sizeof(copy_cmd)) {
                fprintf(stderr, "Error: Command too long\n");
                return 1;
            }
            if (run_command_with_window("Copying packages", packages_dir, copy_cmd)) {
                printf("Packages updated from repository\n");
                success = true;
            } else {
                fprintf(stderr, "Error: Failed to copy packages from repository\n");
            }
        } else {
            fprintf(stderr, "Error: Failed to clone/update repository\n");
        }
    }
    // Default: Update from TSI source repository
    else {
        const char *default_repo = "https://github.com/PanterSoft/tsi.git";
        printf("Updating from default repository: %s\n", default_repo);

        char temp_dir[1024];
        int temp_dir_len = snprintf(temp_dir, sizeof(temp_dir), "%s/tmp-repo-update", tsi_prefix);
        if (temp_dir_len < 0 || (size_t)temp_dir_len >= sizeof(temp_dir)) {
            fprintf(stderr, "Error: Path too long\n");
            return 1;
        }

        // Clone or update repository
        char git_cmd[2048];
        struct stat st;
        int git_cmd_len;
        if (stat(temp_dir, &st) == 0) {
            // Update existing clone
            git_cmd_len = snprintf(git_cmd, sizeof(git_cmd), "cd '%s' && git pull 2>/dev/null", temp_dir);
        } else {
            // Clone repository
            git_cmd_len = snprintf(git_cmd, sizeof(git_cmd), "git clone --depth 1 '%s' '%s' 2>&1", default_repo, temp_dir);
        }
        if (git_cmd_len < 0 || (size_t)git_cmd_len >= sizeof(git_cmd)) {
            fprintf(stderr, "Error: Command too long\n");
            return 1;
        }

        // Check if git is available before trying to use it
        // Use a safer check that won't hang
        FILE *git_check = popen("command -v git 2>/dev/null", "r");
        bool git_available = false;
        if (git_check) {
            char result[256];
            if (fgets(result, sizeof(result), git_check)) {
                git_available = true;
            }
            pclose(git_check);
        }
        if (!git_available) {
            fprintf(stderr, "Error: git is not installed or not in PATH\n");
            fprintf(stderr, "Please install git to update the package repository\n");
            return 1;
        }

        if (run_command_with_window("Syncing repository", default_repo, git_cmd)) {
            // Copy package files
            char packages_dir[1024];
            int packages_dir_len = snprintf(packages_dir, sizeof(packages_dir), "%s/packages", temp_dir);
            if (packages_dir_len < 0 || (size_t)packages_dir_len >= sizeof(packages_dir)) {
                fprintf(stderr, "Error: Path too long\n");
                return 1;
            }

            char copy_cmd[2048];
            int copy_cmd_len = snprintf(copy_cmd, sizeof(copy_cmd), "cp '%s'/*.json '%s/' 2>/dev/null", packages_dir, repo_dir);
            if (copy_cmd_len >= 0 && (size_t)copy_cmd_len < sizeof(copy_cmd) &&
                run_command_with_window("Copying packages", packages_dir, copy_cmd)) {
                printf("Packages updated from default repository\n");
                success = true;
            } else {
                fprintf(stderr, "Error: Failed to copy packages from repository\n");
            }
        } else {
            fprintf(stderr, "Error: Failed to clone/update repository\n");
            fprintf(stderr, "Hint: Make sure git is installed and you have internet access\n");
        }
    }

    if (success) {
        // Count updated packages
        DIR *dir = opendir(repo_dir);
        int count = 0;
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] != '.' && strstr(entry->d_name, ".json")) {
                    count++;
                }
            }
            closedir(dir);
        }
        printf("\nRepository updated successfully!\n");
        printf("Total packages available: %d\n", count);
        printf("\nUse 'tsi info <package>' to see package details\n");
    } else {
        fprintf(stderr, "\nFailed to update repository\n");
        return 1;
    }

        return 0;
    }


int main(int argc, char **argv) {
    // Check for --version early (before logging initialization to avoid hangs)
    if (argc >= 2 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        printf("TSI 0.2.0 (C implementation)\n");
        fflush(stdout);
        fflush(stderr);
        return 0;
    }

    // Initialize logging (file logging disabled by default to prevent hangs)
    const char *enable_console_log = getenv("TSI_LOG_TO_CONSOLE");
        log_set_file(false);
        log_set_console(enable_console_log && strcmp(enable_console_log, "1") == 0);
    if (log_get_level() == LOG_LEVEL_NONE) {
        log_set_level(LOG_LEVEL_DEVELOPER);
    }

    // ============================================================================
    // CONFIGURATION LOADING - Central to TSI's operation
    // ============================================================================
    // Configuration is a fundamental part of TSI. The config file (tsi.cfg) controls
    // core behavior like strict isolation mode. Config is loaded early here so it's
    // available to all commands and subsystems throughout TSI's execution.
    // ============================================================================
    char tsi_prefix[1024];
    get_tsi_prefix_with_fallback(tsi_prefix, sizeof(tsi_prefix), NULL);
    config_load(tsi_prefix);

    if (argc < 2) {
        print_usage(argv[0]);
        log_cleanup();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        log_cleanup();
        return 0;
    }

    if (strcmp(argv[1], "install") == 0) {
        return cmd_install(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "remove") == 0) {
        return cmd_remove(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "list") == 0) {
        return cmd_list(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "info") == 0) {
        return cmd_info(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "versions") == 0) {
        return cmd_versions(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "update") == 0) {
        return cmd_update(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
