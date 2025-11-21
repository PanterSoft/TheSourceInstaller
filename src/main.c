#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "package.h"
#include "database.h"
#include "resolver.h"
#include "fetcher.h"
#include "builder.h"

static void print_usage(const char *prog_name) {
    printf("TSI - TheSourceInstaller\n");
    printf("Usage: %s <command> [options]\n\n", prog_name);
    printf("Commands:\n");
    printf("  install [--force] [--prefix PATH] <package>  Install a package\n");
    printf("  remove <package>                             Remove an installed package\n");
    printf("  list                                         List installed packages\n");
    printf("  info <package>                               Show package information\n");
    printf("  update [--repo URL] [--local PATH]           Update package repository\n");
    printf("  uninstall [--all] [--prefix PATH]            Uninstall TSI\n");
    printf("  --help                                       Show this help\n");
    printf("  --version                                    Show version\n");
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
        return 1;
    }

    printf("Installing package: %s\n", package_name);

    // Check if already installed (check specific version if specified)
    if (!force) {
        InstalledPackage *installed_pkg = database_get_package(db, package_name);
        if (installed_pkg) {
            // If version specified, check if that specific version is installed
            if (package_version && installed_pkg->version && strcmp(installed_pkg->version, package_version) == 0) {
                printf("Package %s@%s is already installed:\n", package_name, package_version);
                printf("  Install path: %s\n", installed_pkg->install_path ? installed_pkg->install_path : "unknown");
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
                printf("Package %s is already installed:\n", package_name);
                printf("  Version: %s\n", installed_pkg->version ? installed_pkg->version : "unknown");
                printf("  Install path: %s\n", installed_pkg->install_path ? installed_pkg->install_path : "unknown");
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
        // Check if package exists in repository
        Package *pkg = package_version ? repository_get_package_version(repo, package_name, package_version) : repository_get_package(repo, package_name);
        if (!pkg) {
            if (package_version) {
                fprintf(stderr, "Error: Package '%s@%s' not found in repository\n", package_name, package_version);
            } else {
                fprintf(stderr, "Error: Package '%s' not found in repository\n", package_name);
            }
        } else {
            fprintf(stderr, "Error: Failed to resolve dependencies for '%s'\n", package_name);
        }
        if (installed) {
            for (size_t i = 0; i < installed_count; i++) free(installed[i]);
            free(installed);
        }
        resolver_free(resolver);
        repository_free(repo);
        database_free(db);
        return 1;
    }

    printf("Resolved %zu dependencies\n", deps_count);

    // Debug: print resolved packages
    if (deps_count > 0) {
        printf("Packages to build: ");
        for (size_t i = 0; i < deps_count; i++) {
            printf("%s ", deps[i]);
        }
        printf("\n");
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
                    fprintf(stderr, "  Warning: Package '%s' not found in repository\n", deps[i]);
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

    printf("Build order:\n");
    for (size_t i = 0; i < build_order_count; i++) {
        printf("  %zu. %s\n", i + 1, build_order[i]);
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

    // Install dependencies first
    for (size_t i = 0; i < build_order_count; i++) {
        if (strcmp(build_order[i], package_name) == 0) {
            continue; // Install main package last
        }

        printf("Installing dependency: %s\n", build_order[i]);

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
            printf("Warning: Dependency package not found: %s\n", build_order[i]);
            continue;
        }

        // Fetch source
        char *dep_source_dir = fetcher_fetch(fetcher, dep_pkg, force);
        if (!dep_source_dir) {
            printf("Error: Failed to fetch source for %s\n", build_order[i]);
            continue;
        }

        // Build
        char build_dir[1024];
        if (dep_pkg->version && strcmp(dep_pkg->version, "latest") != 0) {
            snprintf(build_dir, sizeof(build_dir), "%s/%s-%s", builder_config->build_dir, dep_pkg->name, dep_pkg->version);
        } else {
            snprintf(build_dir, sizeof(build_dir), "%s/%s", builder_config->build_dir, dep_pkg->name);
        }
        if (!builder_build(builder_config, dep_pkg, dep_source_dir, build_dir)) {
            printf("Error: Failed to build %s\n", build_order[i]);
            free(dep_source_dir);
            continue;
        }

        // Install
        if (!builder_install(builder_config, dep_pkg, dep_source_dir, build_dir)) {
            printf("Error: Failed to install %s\n", build_order[i]);
            free(dep_source_dir);
            continue;
        }

        // Record in database
        database_add_package(db, dep_pkg->name, dep_pkg->version, builder_config->install_dir, (const char **)dep_pkg->dependencies, dep_pkg->dependencies_count);
        free(dep_source_dir);
    }

    // Install main package
    printf("Installing %s...\n", package_name);
    Package *main_pkg = package_version ? repository_get_package_version(repo, package_name, package_version) : repository_get_package(repo, package_name);
    if (main_pkg) {
        char *main_source_dir = fetcher_fetch(fetcher, main_pkg, force);
        if (main_source_dir) {
            char build_dir[1024];
            snprintf(build_dir, sizeof(build_dir), "%s/%s", builder_config->build_dir, main_pkg->name);

            if (builder_build(builder_config, main_pkg, main_source_dir, build_dir)) {
                if (builder_install(builder_config, main_pkg, main_source_dir, build_dir)) {
                    database_add_package(db, main_pkg->name, main_pkg->version, builder_config->install_dir, (const char **)main_pkg->dependencies, main_pkg->dependencies_count);
                    printf("Successfully installed %s\n", package_name);
                } else {
                    printf("Error: Failed to install %s\n", package_name);
                }
            } else {
                printf("Error: Failed to build %s\n", package_name);
            }
            free(main_source_dir);
        } else {
            printf("Error: Failed to fetch source for %s\n", package_name);
        }
    } else {
        fprintf(stderr, "Error: Package not found: %s\n", package_name);
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

    Package *pkg = version ? repository_get_package_version(repo, actual_name, version) : repository_get_package(repo, actual_name);

    if (!pkg) {
        if (version) {
            fprintf(stderr, "Package not found: %s@%s\n", actual_name, version);
        } else {
            fprintf(stderr, "Package not found: %s\n", actual_name);
        }
        if (at_pos) {
            free(allocated_name);
            free((char*)version);
        }
        repository_free(repo);
        return 1;
    }

    printf("Package: %s\n", pkg->name ? pkg->name : "unknown");
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

    repository_free(repo);
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

    printf("Updating package repository...\n");
    printf("Repository directory: %s\n", repo_dir);

    bool success = false;

    // Update from local path
    if (local_path) {
        printf("Updating from local path: %s\n", local_path);
        char copy_cmd[2048];
        int copy_cmd_len = snprintf(copy_cmd, sizeof(copy_cmd), "cp '%s'/*.json '%s/' 2>/dev/null", local_path, repo_dir);
        if (copy_cmd_len >= 0 && (size_t)copy_cmd_len < sizeof(copy_cmd) && system(copy_cmd) == 0) {
            printf("✓ Packages copied from local path\n");
            success = true;
        } else {
            fprintf(stderr, "Error: Failed to copy packages from local path\n");
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

        if (system(git_cmd) == 0) {
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
            if (system(copy_cmd) == 0) {
                printf("✓ Packages updated from repository\n");
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

        if (system(git_cmd) == 0) {
            // Copy package files
            char packages_dir[1024];
            int packages_dir_len = snprintf(packages_dir, sizeof(packages_dir), "%s/packages", temp_dir);
            if (packages_dir_len < 0 || (size_t)packages_dir_len >= sizeof(packages_dir)) {
                fprintf(stderr, "Error: Path too long\n");
                return 1;
            }

            char copy_cmd[2048];
            int copy_cmd_len = snprintf(copy_cmd, sizeof(copy_cmd), "cp '%s'/*.json '%s/' 2>/dev/null", packages_dir, repo_dir);
            if (copy_cmd_len >= 0 && (size_t)copy_cmd_len < sizeof(copy_cmd) && system(copy_cmd) == 0) {
                printf("✓ Packages updated from default repository\n");
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

static int cmd_uninstall(int argc, char **argv) {
    bool remove_all = false;
    const char *prefix = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--all") == 0) {
            remove_all = true;
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

    // Display warning and get confirmation
    printf("⚠️  WARNING: This will uninstall TSI!\n");
    printf("\n");
    printf("Uninstalling TSI from: %s\n", tsi_prefix);
    if (remove_all) {
        printf("This will remove:\n");
        printf("  - TSI binary\n");
        printf("  - Completion scripts\n");
        printf("  - Installed packages and binaries\n");
        printf("  - ALL TSI data (database, sources, builds, etc.)\n");
    } else {
        printf("This will remove:\n");
        printf("  - TSI binary\n");
        printf("  - Completion scripts\n");
        printf("  - Installed packages and binaries\n");
        printf("  - TSI data (database, sources, builds, etc.) will be preserved\n");
    }
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
        printf("Uninstall cancelled.\n");
        return 0;
    }

    printf("\n");

    // Remove installed packages/binaries
    char install_dir[1024];
    len = snprintf(install_dir, sizeof(install_dir), "%s/install", tsi_prefix);
    if (len >= 0 && (size_t)len < sizeof(install_dir)) {
        // Check if install directory exists
        struct stat st;
        if (stat(install_dir, &st) == 0) {
            // Load database to get package count for display
            char db_dir[1024];
            len = snprintf(db_dir, sizeof(db_dir), "%s/db", tsi_prefix);
            size_t package_count = 0;
            if (len >= 0 && (size_t)len < sizeof(db_dir)) {
                Database *db = database_new(db_dir);
                if (db) {
                    package_count = db->packages_count;
                    database_free(db);
                }
            }

            if (package_count > 0) {
                printf("Removing installed packages (%zu package(s))...\n", package_count);
            } else {
                printf("Removing installed packages and binaries...\n");
            }

            // Remove the entire install directory which contains all binaries, libraries, etc.
            char cmd[2048];
            int cmd_len = snprintf(cmd, sizeof(cmd), "rm -rf '%s'", install_dir);
            if (cmd_len >= 0 && (size_t)cmd_len < sizeof(cmd)) {
                if (system(cmd) == 0) {
                    printf("✓ Removed installed packages and binaries from: %s\n", install_dir);
                } else {
                    printf("⚠ Warning: Failed to remove install directory\n");
                }
            }
        }
    }

    // Remove binary
    char bin_path[1024];
    len = snprintf(bin_path, sizeof(bin_path), "%s/bin/tsi", tsi_prefix);
    if (len >= 0 && (size_t)len < sizeof(bin_path)) {
        if (unlink(bin_path) == 0) {
            printf("✓ Removed binary: %s\n", bin_path);
        } else {
            printf("⚠ Binary not found: %s\n", bin_path);
        }
    }

    // Remove completion scripts
    char completions_dir[1024];
    len = snprintf(completions_dir, sizeof(completions_dir), "%s/share/completions", tsi_prefix);
    if (len >= 0 && (size_t)len < sizeof(completions_dir)) {
        char cmd[2048];
        int cmd_len = snprintf(cmd, sizeof(cmd), "rm -rf '%s'", completions_dir);
        if (cmd_len >= 0 && (size_t)cmd_len < sizeof(cmd)) {
            if (system(cmd) == 0) {
                printf("✓ Removed completion scripts\n");
            }
        }
    }

    // Remove share directory if empty
    char share_dir[1024];
    len = snprintf(share_dir, sizeof(share_dir), "%s/share", tsi_prefix);
    if (len >= 0 && (size_t)len < sizeof(share_dir)) {
        char cmd[2048];
        int cmd_len = snprintf(cmd, sizeof(cmd), "rmdir '%s' 2>/dev/null", share_dir);
        if (cmd_len >= 0 && (size_t)cmd_len < sizeof(cmd)) {
            system(cmd);
        }
    }

    // Remove bin directory if empty
    char bin_dir[1024];
    len = snprintf(bin_dir, sizeof(bin_dir), "%s/bin", tsi_prefix);
    if (len >= 0 && (size_t)len < sizeof(bin_dir)) {
        char cmd[2048];
        int cmd_len = snprintf(cmd, sizeof(cmd), "rmdir '%s' 2>/dev/null", bin_dir);
        if (cmd_len >= 0 && (size_t)cmd_len < sizeof(cmd)) {
            system(cmd);
        }
    }

    if (remove_all) {
        printf("\nRemoving all TSI data...\n");

        // Remove all TSI directories
        char cmd[2048];
        int cmd_len = snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tsi_prefix);
        if (cmd_len >= 0 && (size_t)cmd_len < sizeof(cmd) && system(cmd) == 0) {
            printf("✓ Removed all TSI data: %s\n", tsi_prefix);
        } else {
            fprintf(stderr, "Error: Failed to remove TSI data\n");
            return 1;
        }
    } else {
        printf("\nTSI binary and completion scripts removed.\n");
        printf("TSI data (packages, database, etc.) preserved at: %s\n", tsi_prefix);
        printf("Use 'tsi uninstall --all' to remove all data.\n");
    }

    printf("\n✓ TSI uninstalled successfully!\n");
    printf("\nNote: You may want to remove TSI from your PATH:\n");
    printf("  Remove 'export PATH=\"%s/bin:\\$PATH\"' from your shell profile\n", tsi_prefix);
    printf("  (~/.bashrc, ~/.zshrc, etc.)\n");

    return 0;
}

int main(int argc, char **argv) {
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
            printf("Removed %s\n", argv[2]);
            database_free(db);
            return 0;
        } else {
            printf("Package %s is not installed\n", argv[2]);
            database_free(db);
            return 1;
        }
    } else if (strcmp(argv[1], "list") == 0) {
        return cmd_list(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "info") == 0) {
        return cmd_info(argc - 1, argv + 1);
    } else if (strcmp(argv[1], "update") == 0) {
        return cmd_update(argc - 1, argv + 1);
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
