#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include "package.h"
#include "database.h"
#include "resolver.h"
#include "fetcher.h"
#include "builder.h"
#include "tui.h"

// Output callback for build/install progress
static void output_callback(const char *line, void *userdata) {
    OutputBuffer *buf = (OutputBuffer *)userdata;
    if (buf) {
        output_buffer_add(buf, line);
        output_buffer_display(buf);
    }
}

static void print_usage(const char *prog_name) {
    printf("TSI - TheSourceInstaller\n");
    printf("Usage: %s <command> [options]\n\n", prog_name);
    printf("Commands:\n");
    printf("  install [--force] [--prefix PATH] <package>  Install a package\n");
    printf("  remove <package>                             Remove an installed package\n");
    printf("  list                                         List installed packages\n");
    printf("  info <package>                               Show package information\n");
    printf("  versions <package>                           List all available versions\n");
    printf("  update [--repo URL] [--local PATH]           Update package repository and TSI\n");
    printf("  uninstall [--prefix PATH]                    Uninstall TSI and all data\n");
    printf("  --help                                       Show this help\n");
    printf("  --version                                    Show version\n");
}

static bool run_command_with_window(const char *overview, const char *detail, const char *cmd) {
    if (!cmd || !*cmd) {
        return false;
    }

    bool interactive = is_tty();

    if (overview && *overview) {
        print_step_overview(overview, detail);
    }

    OutputBuffer buffer;
    output_buffer_init(&buffer);
    output_capture_start(cmd);
    output_buffer_add(&buffer, cmd);
    if (interactive) {
        output_buffer_display(&buffer);
    } else {
        printf("%s\n", cmd);
    }

    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        output_capture_end(&buffer);
        return false;
    }

    char line[OUTPUT_LINE_LENGTH];
    while (fgets(line, sizeof(line), pipe)) {
        output_buffer_add(&buffer, line);
        if (interactive) {
            output_buffer_display(&buffer);
        } else {
            fputs(line, stdout);
        }
    }

    int status = pclose(pipe);
    output_capture_end(&buffer);
    return status == 0;
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

    const char *home = getenv("HOME");
    if (!home) home = "/root";

    char tsi_prefix[1024];
    int len;
    if (prefix) {
        strncpy(tsi_prefix, prefix, sizeof(tsi_prefix) - 1);
        tsi_prefix[sizeof(tsi_prefix) - 1] = '\0';
    } else {
        len = snprintf(tsi_prefix, sizeof(tsi_prefix), "%s/.tsi", home);
        if (len < 0 || (size_t)len >= sizeof(tsi_prefix)) {
            fprintf(stderr, "Error: Path too long\n");
            return 1;
        }
    }

    char db_dir[1024];
    len = snprintf(db_dir, sizeof(db_dir), "%s/db", tsi_prefix);
    if (len < 0 || (size_t)len >= sizeof(db_dir)) {
        fprintf(stderr, "Error: Path too long\n");
        return 1;
    }

    char repo_dir[1024];
    len = snprintf(repo_dir, sizeof(repo_dir), "%s/repos", tsi_prefix);
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
                        // Second try: ~/.tsi/scripts/discover-versions.py (if installed)
                        const char *home = getenv("HOME");
                        if (!home) home = "/root";

                        // Try installed location first
                        len = snprintf(script_path, sizeof(script_path), "%s/.tsi/scripts/discover-versions.py", home);
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
    if (package_version) {
        print_section("Upgrading");
        printf("  %s %s -> %s\n", package_name, "?", package_version);
    } else {
        // Don't print section header here - will be printed by print_building_compact
    }

    // Check if already installed (check specific version if specified)
    if (!force) {
        InstalledPackage *installed_pkg = database_get_package(db, package_name);
        if (installed_pkg) {
            // If version specified, check if that specific version is installed
            if (package_version && installed_pkg->version && strcmp(installed_pkg->version, package_version) == 0) {
                if (package_version) {
                    char warn_msg[256];
                    snprintf(warn_msg, sizeof(warn_msg), "%s@%s is already installed", package_name, package_version);
                    print_warning(warn_msg);
                } else {
                    char warn_msg[256];
                    snprintf(warn_msg, sizeof(warn_msg), "%s is already installed", package_name);
                    print_warning(warn_msg);
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
                print_warning(warn_msg);
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
    size_t deps_count = 0;
    char **deps = resolver_resolve(resolver, package_name, installed, installed_count, &deps_count);

    if (!deps) {
        // Package exists but dependency resolution failed
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
        print_section("Resolving dependencies");

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
    size_t build_order_count = 0;
    char **build_order = resolver_get_build_order(resolver, deps, deps_count, &build_order_count);

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
                    print_warning(warn_msg);
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
        print_section("Build order");
        for (size_t i = 0; i < build_order_count; i++) {
            printf("  %zu. %s\n", i + 1, build_order[i]);
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

    // Install dependencies first
    for (size_t i = 0; i < build_order_count; i++) {
        if (strcmp(build_order[i], package_name) == 0) {
            continue; // Install main package last
        }

        print_progress("Installing dependency", build_order[i]);

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
            print_warning(warn_msg);
            continue;
        }

        // Fetch source
        char *dep_source_dir = fetcher_fetch(fetcher, dep_pkg, force);
        if (!dep_source_dir) {
            fprintf(stderr, "Error: Failed to fetch source for %s\n", build_order[i]);
            continue;
        }

        // Set package-specific install directory
        builder_config_set_package_dir(builder_config, dep_pkg->name, dep_pkg->version);

        // Build
        print_building_compact(dep_pkg->name, dep_pkg->version);
        char build_dir[1024];
        if (dep_pkg->version && strcmp(dep_pkg->version, "latest") != 0) {
            snprintf(build_dir, sizeof(build_dir), "%s/%s-%s", builder_config->build_dir, dep_pkg->name, dep_pkg->version);
        } else {
            snprintf(build_dir, sizeof(build_dir), "%s/%s", builder_config->build_dir, dep_pkg->name);
        }

        // Setup output buffer for showing last 5 lines
        OutputBuffer output_buf;
        output_buffer_init(&output_buf);
        char build_window_title[256];
        if (dep_pkg->version && dep_pkg->version[0]) {
            snprintf(build_window_title, sizeof(build_window_title), "%s Building %s %s", ICON_BUILD, dep_pkg->name, dep_pkg->version);
        } else {
            snprintf(build_window_title, sizeof(build_window_title), "%s Building %s", ICON_BUILD, dep_pkg->name);
        }
        output_capture_start(build_window_title);

        if (!builder_build_with_output(builder_config, dep_pkg, dep_source_dir, build_dir, output_callback, &output_buf)) {
            output_capture_end(&output_buf);
            print_error("Failed to build dependency");
            fprintf(stderr, "  %s\n", build_order[i]);
            has_failures = true;
            failed_deps = realloc(failed_deps, sizeof(char*) * (failed_deps_count + 1));
            if (failed_deps) {
                failed_deps[failed_deps_count++] = strdup(build_order[i]);
            }
            free(dep_source_dir);
            continue;
        }
        output_capture_end(&output_buf);

        // Install
        print_installing_compact(dep_pkg->name, dep_pkg->version);
        output_buffer_init(&output_buf);
        char install_window_title[256];
        if (dep_pkg->version && dep_pkg->version[0]) {
            snprintf(install_window_title, sizeof(install_window_title), "%s Installing %s %s", ICON_INSTALL, dep_pkg->name, dep_pkg->version);
        } else {
            snprintf(install_window_title, sizeof(install_window_title), "%s Installing %s", ICON_INSTALL, dep_pkg->name);
        }
        output_capture_start(install_window_title);

        if (!builder_install_with_output(builder_config, dep_pkg, dep_source_dir, build_dir, output_callback, &output_buf)) {
            output_capture_end(&output_buf);
            print_error("Failed to install dependency");
            fprintf(stderr, "  %s\n", build_order[i]);
            has_failures = true;
            failed_deps = realloc(failed_deps, sizeof(char*) * (failed_deps_count + 1));
            if (failed_deps) {
                failed_deps[failed_deps_count++] = strdup(build_order[i]);
            }
            free(dep_source_dir);
            continue;
        }
        output_capture_end(&output_buf);

        // Show completion
        char done_msg[256];
        if (dep_pkg->version) {
            snprintf(done_msg, sizeof(done_msg), "%s Installed %s %s", ICON_SUCCESS, dep_pkg->name, dep_pkg->version);
        } else {
            snprintf(done_msg, sizeof(done_msg), "%s Installed %s", ICON_SUCCESS, dep_pkg->name);
        }
        print_status_done(done_msg);

        // Create symlinks to main install directory
        builder_create_symlinks(builder_config, dep_pkg->name, dep_pkg->version);

        // Record in database with package-specific path
        database_add_package(db, dep_pkg->name, dep_pkg->version, builder_config->install_dir, (const char **)dep_pkg->dependencies, dep_pkg->dependencies_count);
        free(dep_source_dir);
    }

    // Install main package
    print_progress("Installing", package_name);
    Package *main_pkg = package_version ? repository_get_package_version(repo, package_name, package_version) : repository_get_package(repo, package_name);
    if (main_pkg) {
        // Set package-specific install directory
        builder_config_set_package_dir(builder_config, main_pkg->name, main_pkg->version);

        char *main_source_dir = fetcher_fetch(fetcher, main_pkg, force);
        if (main_source_dir) {
            char build_dir[1024];
            snprintf(build_dir, sizeof(build_dir), "%s/%s", builder_config->build_dir, main_pkg->name);

            print_building_compact(main_pkg->name, main_pkg->version);
            if (is_tty()) {
                printf("\n");  // New line for output area
            }

            // Setup output buffer for showing last 5 lines
            OutputBuffer output_buf;
            output_buffer_init(&output_buf);
            char main_build_title[256];
            if (main_pkg->version && main_pkg->version[0]) {
                snprintf(main_build_title, sizeof(main_build_title), "%s Building %s %s", ICON_BUILD, main_pkg->name, main_pkg->version);
            } else {
                snprintf(main_build_title, sizeof(main_build_title), "%s Building %s", ICON_BUILD, main_pkg->name);
            }
            output_capture_start(main_build_title);

            if (builder_build_with_output(builder_config, main_pkg, main_source_dir, build_dir, output_callback, &output_buf)) {
                output_capture_end(&output_buf);
                print_installing_compact(main_pkg->name, main_pkg->version);
                output_buffer_init(&output_buf);
                char main_install_title[256];
                if (main_pkg->version && main_pkg->version[0]) {
                    snprintf(main_install_title, sizeof(main_install_title), "%s Installing %s %s", ICON_INSTALL, main_pkg->name, main_pkg->version);
                } else {
                    snprintf(main_install_title, sizeof(main_install_title), "%s Installing %s", ICON_INSTALL, main_pkg->name);
                }
                output_capture_start(main_install_title);

                if (builder_install_with_output(builder_config, main_pkg, main_source_dir, build_dir, output_callback, &output_buf)) {
                    output_capture_end(&output_buf);
                    // Create symlinks to main install directory
                    builder_create_symlinks(builder_config, main_pkg->name, main_pkg->version);

                    // Record in database with package-specific path
                    database_add_package(db, main_pkg->name, main_pkg->version, builder_config->install_dir, (const char **)main_pkg->dependencies, main_pkg->dependencies_count);

                    // Show completion
                    char done_msg[256];
                    if (main_pkg->version) {
                        snprintf(done_msg, sizeof(done_msg), "%s Installed %s %s", ICON_SUCCESS, main_pkg->name, main_pkg->version);
                    } else {
                        snprintf(done_msg, sizeof(done_msg), "%s Installed %s", ICON_SUCCESS, main_pkg->name);
                    }
                    print_status_done(done_msg);

                    // Show summary
                    print_summary(builder_config->install_dir, 0, NULL);

                    // Show caveats (if any)
                    if (main_pkg->description) {
                        print_caveats_start();
                        print_caveat(main_pkg->description);
                    }
                } else {
                    print_error("Failed to install package");
                    if (package_version) {
                        fprintf(stderr, "  %s@%s\n", package_name, package_version);
                    } else {
                        fprintf(stderr, "  %s\n", package_name);
                    }
                    has_failures = true;
                }
            } else {
                print_error("Failed to build package");
                if (package_version) {
                    fprintf(stderr, "  %s@%s\n", package_name, package_version);
                } else {
                    fprintf(stderr, "  %s\n", package_name);
                }
                has_failures = true;
            }
            free(main_source_dir);
        } else {
            print_error("Failed to fetch source");
            if (package_version) {
                fprintf(stderr, "  %s@%s\n", package_name, package_version);
            } else {
                fprintf(stderr, "  %s\n", package_name);
            }
            has_failures = true;
        }
    } else {
        print_error("Package not found");
        fprintf(stderr, "  %s\n", package_name);
        has_failures = true;
    }

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
        print_error("Installation completed with errors");
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
    const char *home = getenv("HOME");
    if (!home) home = "/root";

    char db_dir[1024];
    snprintf(db_dir, sizeof(db_dir), "%s/.tsi/db", home);

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
    const char *home = getenv("HOME");
    if (!home) home = "/root";

    char repo_dir[1024];
    snprintf(repo_dir, sizeof(repo_dir), "%s/.tsi/repos", home);

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

    print_section("Available versions");
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
    const char *home = getenv("HOME");
    if (!home) home = "/root";

    char repo_dir[1024];
    snprintf(repo_dir, sizeof(repo_dir), "%s/.tsi/repos", home);

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

    print_section("Package Information");
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
    char db_dir[1024];
    snprintf(db_dir, sizeof(db_dir), "%s/.tsi/db", home);
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

    const char *home = getenv("HOME");
    if (!home) home = "/root";

    char tsi_prefix[1024];
    int len;
    if (prefix) {
        strncpy(tsi_prefix, prefix, sizeof(tsi_prefix) - 1);
        tsi_prefix[sizeof(tsi_prefix) - 1] = '\0';
    } else {
        len = snprintf(tsi_prefix, sizeof(tsi_prefix), "%s/.tsi", home);
        if (len < 0 || (size_t)len >= sizeof(tsi_prefix)) {
            fprintf(stderr, "Error: Path too long\n");
            return 1;
        }
    }

    char repo_dir[1024];
    len = snprintf(repo_dir, sizeof(repo_dir), "%s/repos", tsi_prefix);
    if (len < 0 || (size_t)len >= sizeof(repo_dir)) {
        fprintf(stderr, "Error: Path too long\n");
        return 1;
    }

    // Create repo directory if it doesn't exist
    char cmd[2048];
    int cmd_len = snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", repo_dir);
    if (cmd_len >= 0 && (size_t)cmd_len < sizeof(cmd)) {
        system(cmd);
    }

    print_section("Updating package repository");
    printf("  Repository directory: %s\n", repo_dir);

    bool success = false;

    // Update from local path
    if (local_path) {
        print_section("Updating from local path");
        printf("  %s\n", local_path);
        char copy_cmd[2048];
        int copy_cmd_len = snprintf(copy_cmd, sizeof(copy_cmd), "cp '%s'/*.json '%s/' 2>/dev/null", local_path, repo_dir);
        if (copy_cmd_len >= 0 && (size_t)copy_cmd_len < sizeof(copy_cmd)) {
            if (run_command_with_window("Copying packages", local_path, copy_cmd)) {
                print_success("Packages copied from local path");
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
            git_cmd_len = snprintf(git_cmd, sizeof(git_cmd), "cd '%s' && git pull 2>/dev/null", temp_dir);
        } else {
            // Clone repository
            git_cmd_len = snprintf(git_cmd, sizeof(git_cmd), "git clone --depth 1 '%s' '%s' 2>/dev/null", repo_url, temp_dir);
        }
        if (git_cmd_len < 0 || (size_t)git_cmd_len >= sizeof(git_cmd)) {
            fprintf(stderr, "Error: Command too long\n");
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
                print_success("Packages updated from repository");
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
            git_cmd_len = snprintf(git_cmd, sizeof(git_cmd), "git clone --depth 1 '%s' '%s' 2>/dev/null", default_repo, temp_dir);
        }
        if (git_cmd_len < 0 || (size_t)git_cmd_len >= sizeof(git_cmd)) {
            fprintf(stderr, "Error: Command too long\n");
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
                print_success("Packages updated from default repository");
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

    // Check for TSI self-update
    printf("\nChecking for TSI updates...\n");

    // Check common TSI source locations (in order of preference)
    char tsi_source_paths[][1024] = {
        {0}, // $INSTALL_DIR/tsi (if set)
        {0}, // $HOME/tsi-install/tsi (default bootstrap location)
        {0}  // Development mode: find git repo from executable path
    };

    int path_count = 0;

    // Check $INSTALL_DIR/tsi (from environment)
    const char *install_dir = getenv("INSTALL_DIR");
    if (install_dir && install_dir[0] != '\0') {
        snprintf(tsi_source_paths[path_count], sizeof(tsi_source_paths[path_count]), "%s/tsi", install_dir);
        path_count++;
    }

    // Check $HOME/tsi-install/tsi (default bootstrap location)
    snprintf(tsi_source_paths[path_count], sizeof(tsi_source_paths[path_count]), "%s/tsi-install/tsi", home);
    path_count++;

    // Check if we're running from a git repo (development mode)
    char current_exe[1024] = {0};
    ssize_t exe_len = -1;
#ifdef __linux__
    exe_len = readlink("/proc/self/exe", current_exe, sizeof(current_exe) - 1);
    if (exe_len > 0) {
        current_exe[exe_len] = '\0';
    }
#elif defined(__APPLE__)
    uint32_t size = sizeof(current_exe);
    if (_NSGetExecutablePath(current_exe, &size) == 0) {
        exe_len = strlen(current_exe);
    }
#endif
    if (exe_len > 0) {
        // Try to find git repo in parent directories
        char check_path[1024];
        strncpy(check_path, current_exe, sizeof(check_path) - 1);
        check_path[sizeof(check_path) - 1] = '\0';
        for (int i = 0; i < 5; i++) {
            char *last_slash = strrchr(check_path, '/');
            if (last_slash) {
                *last_slash = '\0';
                char git_path[1024];
                snprintf(git_path, sizeof(git_path), "%s/.git", check_path);
                struct stat st;
                if (stat(git_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                    // Verify it has src/Makefile (it's the TSI repo)
                    char makefile_path[1024];
                    snprintf(makefile_path, sizeof(makefile_path), "%s/src/Makefile", check_path);
                    if (stat(makefile_path, &st) == 0) {
                        strncpy(tsi_source_paths[path_count], check_path, sizeof(tsi_source_paths[path_count]) - 1);
                        path_count++;
                        break;
                    }
                }
            } else {
                break;
            }
        }
    }

    // Find TSI source directory
    char *tsi_source_dir = NULL;
    for (int i = 0; i < path_count; i++) {
        if (tsi_source_paths[i][0] == '\0') continue;

        char src_makefile[1024];
        snprintf(src_makefile, sizeof(src_makefile), "%s/src/Makefile", tsi_source_paths[i]);
        struct stat st;
        if (stat(src_makefile, &st) == 0) {
            tsi_source_dir = tsi_source_paths[i];
            break;
        }
    }

    if (!tsi_source_dir) {
        printf("TSI source not found. Skipping self-update check.\n");
        printf("(TSI was likely installed from a tarball or binary)\n");
        return 0;
    }

    printf("Found TSI source at: %s\n", tsi_source_dir);

    // Check if it's a git repository
    char git_dir[1024];
    snprintf(git_dir, sizeof(git_dir), "%s/.git", tsi_source_dir);
    struct stat st;
    bool is_git_repo = (stat(git_dir, &st) == 0 && S_ISDIR(st.st_mode));

    if (!is_git_repo) {
        printf("TSI source is not a git repository. Skipping update check.\n");
        return 0;
    }

    // Check for updates
    char fetch_cmd[2048];
    snprintf(fetch_cmd, sizeof(fetch_cmd), "cd '%s' && git fetch origin main 2>&1", tsi_source_dir);
    FILE *fetch_pipe = popen(fetch_cmd, "r");
    if (!fetch_pipe) {
        print_warning("Could not check for TSI updates (git fetch failed)");
        return 0;
    }

    // Read output (discard it, we just need the exit code)
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fetch_pipe) != NULL) {
        // Discard output
    }
    int fetch_status = pclose(fetch_pipe);

    if (fetch_status != 0) {
        print_warning("Could not check for TSI updates (git not available or network error)");
        return 0;
    }

    // Compare local and remote commits
    char local_commit_cmd[2048];
    snprintf(local_commit_cmd, sizeof(local_commit_cmd), "cd '%s' && git rev-parse HEAD 2>/dev/null", tsi_source_dir);
    FILE *local_pipe = popen(local_commit_cmd, "r");
    char local_commit[64] = {0};
    if (local_pipe) {
        if (fgets(local_commit, sizeof(local_commit), local_pipe)) {
            // Remove newline
            size_t len = strlen(local_commit);
            if (len > 0 && local_commit[len - 1] == '\n') {
                local_commit[len - 1] = '\0';
            }
        }
        pclose(local_pipe);
    }

    char remote_commit_cmd[2048];
    snprintf(remote_commit_cmd, sizeof(remote_commit_cmd), "cd '%s' && git rev-parse origin/main 2>/dev/null", tsi_source_dir);
    FILE *remote_pipe = popen(remote_commit_cmd, "r");
    char remote_commit[64] = {0};
    if (remote_pipe) {
        if (fgets(remote_commit, sizeof(remote_commit), remote_pipe)) {
            // Remove newline
            size_t len = strlen(remote_commit);
            if (len > 0 && remote_commit[len - 1] == '\n') {
                remote_commit[len - 1] = '\0';
            }
        }
        pclose(remote_pipe);
    }

    if (local_commit[0] == '\0' || remote_commit[0] == '\0') {
        printf("Could not determine TSI version. Skipping update check.\n");
        return 0;
    }

    if (strcmp(local_commit, remote_commit) == 0) {
        print_success("TSI is up to date");
        return 0;
    }

    // Updates available!
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("TSI update available!\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Current version: %s\n", local_commit);
    printf("Latest version:  %s\n", remote_commit);
    printf("\n");
    printf("Would you like to update TSI now? (y/N): ");
    fflush(stdout);

    char response[16];
    if (fgets(response, sizeof(response), stdin) == NULL) {
        printf("\nUpdate cancelled.\n");
        return 0;
    }

    // Remove newline
    size_t response_len = strlen(response);
    if (response_len > 0 && response[response_len - 1] == '\n') {
        response[response_len - 1] = '\0';
    }

    if (response[0] != 'y' && response[0] != 'Y') {
        printf("Update cancelled.\n");
        return 0;
    }

    printf("\nUpdating TSI...\n");

    // Pull latest changes
    char pull_cmd[2048];
    snprintf(pull_cmd, sizeof(pull_cmd), "cd '%s' && git pull origin main 2>&1", tsi_source_dir);
    if (!run_command_with_window("Pulling latest changes", tsi_source_dir, pull_cmd)) {
        fprintf(stderr, "Error: Failed to pull TSI updates\n");
        return 1;
    }

    // Build TSI
    char build_cmd[2048];
    snprintf(build_cmd, sizeof(build_cmd), "cd '%s/src' && make clean && make 2>&1", tsi_source_dir);
    if (!run_command_with_window("Building TSI", tsi_source_dir, build_cmd)) {
        fprintf(stderr, "Error: Failed to build TSI\n");
        return 1;
    }

    // Check if binary was created
    char binary_path[1024];
    snprintf(binary_path, sizeof(binary_path), "%s/src/bin/tsi", tsi_source_dir);
    if (stat(binary_path, &st) != 0) {
        fprintf(stderr, "Error: TSI binary not found after build\n");
        return 1;
    }

    // Install TSI
    char install_cmd[2048];
    snprintf(install_cmd, sizeof(install_cmd), "mkdir -p '%s/bin' && cp '%s' '%s/bin/tsi' && chmod +x '%s/bin/tsi'",
             tsi_prefix, binary_path, tsi_prefix, tsi_prefix);
    if (!run_command_with_window("Installing TSI", tsi_prefix, install_cmd)) {
        fprintf(stderr, "Error: Failed to install TSI\n");
        return 1;
    }

    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("✓ TSI updated successfully!\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("New version: %s\n", remote_commit);
    printf("TSI binary: %s/bin/tsi\n", tsi_prefix);
    printf("\n");

    return 0;
}

static int cmd_uninstall(int argc, char **argv) {
    const char *prefix = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--prefix") == 0 && i + 1 < argc) {
            prefix = argv[++i];
        }
    }

    const char *home = getenv("HOME");
    if (!home) home = "/root";

    char tsi_prefix[1024];
    int len;
    if (prefix) {
        strncpy(tsi_prefix, prefix, sizeof(tsi_prefix) - 1);
        tsi_prefix[sizeof(tsi_prefix) - 1] = '\0';
    } else {
        len = snprintf(tsi_prefix, sizeof(tsi_prefix), "%s/.tsi", home);
        if (len < 0 || (size_t)len >= sizeof(tsi_prefix)) {
            fprintf(stderr, "Error: Path too long\n");
            return 1;
        }
    }

    // Display warning and get confirmation FIRST, before any processing
    print_section("Uninstalling TSI");
    printf("  Installation path: %s\n", tsi_prefix);
    printf("\n");
    printf("  This will PERMANENTLY remove:\n");
    if (is_tty()) {
        printf("  %s%s%s TSI binary\n", COLOR_RED, ICON_ERROR, COLOR_RESET);
        printf("  %s%s%s Completion scripts\n", COLOR_RED, ICON_ERROR, COLOR_RESET);
        printf("  %s%s%s Installed packages and binaries\n", COLOR_RED, ICON_ERROR, COLOR_RESET);
        printf("  %s%s%s ALL TSI data (database, sources, builds, repository, etc.)\n", COLOR_RED, ICON_ERROR, COLOR_RESET);
    } else {
        printf("  ✗ TSI binary\n");
        printf("  ✗ Completion scripts\n");
        printf("  ✗ Installed packages and binaries\n");
        printf("  ✗ ALL TSI data (database, sources, builds, repository, etc.)\n");
    }
    printf("\n");
    print_warning("This action CANNOT be undone!");
    printf("\n");
    printf("Are you sure you want to continue? (yes/no): ");
    fflush(stdout);

    // Read user confirmation
    char response[16];
    if (fgets(response, sizeof(response), stdin) == NULL) {
        fprintf(stderr, "\nError: Failed to read input\n");
        return 1;
    }

    // Remove trailing newline
    size_t response_len = strlen(response);
    if (response_len > 0 && response[response_len - 1] == '\n') {
        response[response_len - 1] = '\0';
    }

    // Check if user confirmed
    if (strcmp(response, "yes") != 0 && strcmp(response, "y") != 0) {
        printf("\nUninstall cancelled.\n");
        return 0;
    }

    printf("\n");

    // Remove all TSI data (everything)
    printf("Removing all TSI data...\n");

    // Remove all TSI directories
    char cmd[2048];
    int cmd_len = snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tsi_prefix);
    if (cmd_len >= 0 && (size_t)cmd_len < sizeof(cmd) && system(cmd) == 0) {
        printf("✓ Removed all TSI data: %s\n", tsi_prefix);
    } else {
        fprintf(stderr, "Error: Failed to remove TSI data\n");
        return 1;
    }

    printf("\n");
    print_success("TSI uninstalled successfully");
    printf("\n");
    printf("\nNote: You may want to remove TSI from your PATH:\n");
    printf("  Remove 'export PATH=\"%s/bin:\\$PATH\"' from your shell profile\n", tsi_prefix);
    printf("  (~/.bashrc, ~/.zshrc, etc.)\n");

    return 0;
}

int main(int argc, char **argv) {
    tui_style_reload_from_env();

    int write_idx = 1;
    for (int read_idx = 1; read_idx < argc; read_idx++) {
        char *arg = argv[read_idx];
        if (strcmp(arg, "--style") == 0) {
            if (read_idx + 1 >= argc) {
                print_error("Missing style name after --style");
                return 1;
            }
            const char *style_name = argv[++read_idx];
            if (!tui_style_apply(style_name)) {
                char warn_msg[128];
                snprintf(warn_msg, sizeof(warn_msg),
                         "Unknown style '%s'. Using %s",
                         style_name, tui_style_active_name());
                print_warning(warn_msg);
            }
            continue;
        }
        if (strncmp(arg, "--style=", 8) == 0) {
            const char *style_name = arg + 8;
            if (!tui_style_apply(style_name)) {
                char warn_msg[128];
                snprintf(warn_msg, sizeof(warn_msg),
                         "Unknown style '%s'. Using %s",
                         style_name, tui_style_active_name());
                print_warning(warn_msg);
            }
            continue;
        }
        argv[write_idx++] = argv[read_idx];
    }
    argc = write_idx;
    argv[argc] = NULL;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("TSI 0.2.0 (C implementation)\n");
        return 0;
    }

    if (strcmp(argv[1], "install") == 0) {
        return cmd_install(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "uninstall") == 0) {
        // TSI uninstall (check before package remove)
        return cmd_uninstall(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "remove") == 0) {
        // Package removal
        if (argc < 3) {
            fprintf(stderr, "Error: package name required\n");
            return 1;
        }
        const char *home = getenv("HOME");
        if (!home) home = "/root";
        char db_dir[1024];
        int len = snprintf(db_dir, sizeof(db_dir), "%s/.tsi/db", home);
        if (len < 0 || (size_t)len >= sizeof(db_dir)) {
            fprintf(stderr, "Error: Path too long\n");
            return 1;
        }
        Database *db = database_new(db_dir);
        if (database_remove_package(db, argv[2])) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Removed %s", argv[2]);
            print_success(msg);
            database_free(db);
            return 0;
        } else {
            char warn_msg[256];
            snprintf(warn_msg, sizeof(warn_msg), "Package %s is not installed", argv[2]);
            print_warning(warn_msg);
            database_free(db);
            return 1;
        }
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
