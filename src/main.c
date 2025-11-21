#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
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
    printf("  remove <package>                             Remove a package\n");
    printf("  list                                         List installed packages\n");
    printf("  info <package>                               Show package information\n");
    printf("  --help                                       Show this help\n");
    printf("  --version                                    Show version\n");
}

static int cmd_install(int argc, char **argv) {
    bool force = false;
    const char *package_name = NULL;
    const char *prefix = NULL;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--force") == 0) {
            force = true;
        } else if (strcmp(argv[i], "--prefix") == 0 && i + 1 < argc) {
            prefix = argv[++i];
        } else if (!package_name) {
            package_name = argv[i];
        }
    }

    if (!package_name) {
        fprintf(stderr, "Error: package name required\n");
        fprintf(stderr, "Usage: tsi install [--force] [--prefix PATH] <package>\n");
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

    // Check if already installed
    if (!force && database_is_installed(db, package_name)) {
        printf("Package %s is already installed. Use --force to reinstall.\n", package_name);
        resolver_free(resolver);
        repository_free(repo);
        database_free(db);
        return 0;
    }

    // Get installed packages list
    size_t installed_count = 0;
    char **installed = database_list_installed(db, &installed_count);

    // Resolve dependencies
    size_t deps_count = 0;
    char **deps = resolver_resolve(resolver, package_name, installed, installed_count, &deps_count);

    if (!deps) {
        fprintf(stderr, "Error: Failed to resolve dependencies\n");
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

    // Get build order
    size_t build_order_count = 0;
    char **build_order = resolver_get_build_order(resolver, deps, deps_count, &build_order_count);

    if (!build_order) {
        fprintf(stderr, "Error: Failed to determine build order\n");
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

        Package *dep_pkg = repository_get_package(repo, build_order[i]);
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
        snprintf(build_dir, sizeof(build_dir), "%s/%s", builder_config->build_dir, dep_pkg->name);
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
    Package *main_pkg = repository_get_package(repo, package_name);
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

    Package *pkg = repository_get_package(repo, package_name);

    if (!pkg) {
        fprintf(stderr, "Package not found: %s\n", package_name);
        repository_free(repo);
        return 1;
    }

    printf("Package: %s\n", pkg->name ? pkg->name : "unknown");
    printf("Version: %s\n", pkg->version ? pkg->version : "unknown");
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
    } else if (strcmp(argv[1], "remove") == 0 || strcmp(argv[1], "uninstall") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: package name required\n");
            return 1;
        }
        const char *home = getenv("HOME");
        if (!home) home = "/root";
        char db_dir[512];
        snprintf(db_dir, sizeof(db_dir), "%s/.tsi/db", home);
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
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
