#include "resolver.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

// Helper function to parse package@version string
static void parse_package_version(const char *dep_spec, char **name_out, char **version_out) {
    char *at_pos = strchr((char*)dep_spec, '@');
    if (at_pos) {
        size_t name_len = at_pos - dep_spec;
        *name_out = malloc(name_len + 1);
        if (*name_out) {
            strncpy(*name_out, dep_spec, name_len);
            (*name_out)[name_len] = '\0';
        }
        *version_out = strdup(at_pos + 1);
    } else {
        *name_out = strdup(dep_spec);
        *version_out = NULL;
    }
}

// Simple topological sort for build order
static int find_package_index(char **packages, size_t count, const char *name) {
    // Extract package name if it's in package@version format
    char *pkg_name = NULL;
    char *pkg_version = NULL;
    parse_package_version(name, &pkg_name, &pkg_version);

    const char *search_name = pkg_name ? pkg_name : name;

    for (size_t i = 0; i < count; i++) {
        if (!packages[i]) continue;

        // Extract name from packages[i] if it's in package@version format
        char *cmp_name = NULL;
        char *cmp_version = NULL;
        parse_package_version(packages[i], &cmp_name, &cmp_version);

        const char *cmp_search = cmp_name ? cmp_name : packages[i];
        bool matches = (strcmp(cmp_search, search_name) == 0);

        // If version was specified, check it matches
        if (matches && pkg_version && cmp_version) {
            matches = (strcmp(pkg_version, cmp_version) == 0);
        } else if (matches && pkg_version && !cmp_version) {
            matches = false; // Required version not found
        }

        if (cmp_name) free(cmp_name);
        if (cmp_version) free(cmp_version);

        if (matches) {
            if (pkg_name) free(pkg_name);
            if (pkg_version) free(pkg_version);
            return (int)i;
        }
    }

    if (pkg_name) free(pkg_name);
    if (pkg_version) free(pkg_version);
    return -1;
}

DependencyResolver* resolver_new(Repository *repo) {
    log_developer("resolver_new called");
    DependencyResolver *resolver = malloc(sizeof(DependencyResolver));
    if (!resolver) {
        log_error("Failed to allocate memory for DependencyResolver");
        return NULL;
    }
    resolver->repository = repo;
    resolver->visited = NULL;
    resolver->visited_count = 0;
    resolver->visited_capacity = 0;
    log_debug("DependencyResolver initialized");
    return resolver;
}

void resolver_free(DependencyResolver *resolver) {
    if (resolver && resolver->visited) {
        for (size_t i = 0; i < resolver->visited_count; i++) {
            if (resolver->visited[i]) free(resolver->visited[i]);
        }
        free(resolver->visited);
    }
    free(resolver);
}

// Helper to check if a package name (possibly with @version) matches an installed package
static bool is_package_installed(const char *package_spec, char **installed, size_t installed_count) {
    if (!package_spec || !installed) return false;

    // Extract package name from package_spec (may be "package@version")
    char *spec_name = NULL;
    char *spec_version = NULL;
    parse_package_version(package_spec, &spec_name, &spec_version);
    const char *search_name = spec_name ? spec_name : package_spec;

    for (size_t i = 0; i < installed_count; i++) {
        if (!installed[i]) continue;

        // Extract name from installed[i] (may be "package@version")
        char *inst_name = NULL;
        char *inst_version = NULL;
        parse_package_version(installed[i], &inst_name, &inst_version);
        const char *cmp_name = inst_name ? inst_name : installed[i];

        // Match by name (version is optional)
        bool name_matches = (strcmp(cmp_name, search_name) == 0);

        if (inst_name) free(inst_name);
        if (inst_version) free(inst_version);

        if (name_matches) {
            if (spec_name) free(spec_name);
            if (spec_version) free(spec_version);
            return true;
        }
    }

    if (spec_name) free(spec_name);
    if (spec_version) free(spec_version);
    return false;
}

char** resolver_resolve(DependencyResolver *resolver, const char *package_name, char **installed, size_t installed_count, size_t *result_count) {
    log_developer("resolver_resolve called for package: %s (installed_count=%zu)", package_name, installed_count);
    *result_count = 0;
    char **result = NULL;
    size_t capacity = 0;

    if (!resolver) {
        log_error("resolver_resolve called with NULL resolver");
        return NULL;
    }

    // Initialize visited list if needed
    if (resolver->visited == NULL) {
        resolver->visited_capacity = 16;
        resolver->visited = malloc(sizeof(char*) * resolver->visited_capacity);
        resolver->visited_count = 0;
    }

    // Check for circular dependency
    for (size_t i = 0; i < resolver->visited_count; i++) {
        if (resolver->visited[i] && strcmp(resolver->visited[i], package_name) == 0) {
            log_error("Circular dependency detected: %s", package_name);
            *result_count = 0;
            return NULL;
        }
    }

    // Add to visited list
    if (resolver->visited_count >= resolver->visited_capacity) {
        resolver->visited_capacity *= 2;
        resolver->visited = realloc(resolver->visited, sizeof(char*) * resolver->visited_capacity);
    }
    resolver->visited[resolver->visited_count++] = strdup(package_name);

    // Check if already installed
    if (is_package_installed(package_name, installed, installed_count)) {
        log_debug("Package already installed, skipping resolution: %s", package_name);
        // Remove from visited before returning
        if (resolver->visited_count > 0) {
            free(resolver->visited[--resolver->visited_count]);
            resolver->visited[resolver->visited_count] = NULL;
        }
        return NULL; // Already installed
    }

    // Get package
    Package *pkg = repository_get_package(resolver->repository, package_name);
    if (!pkg) {
        log_error("Package not found in repository: %s", package_name);
        return NULL;
    }
    log_debug("Package found: %s@%s with %zu dependencies", pkg->name, pkg->version ? pkg->version : "latest", pkg->dependencies_count);

    // Resolve dependencies first
    for (size_t i = 0; i < pkg->dependencies_count; i++) {
        if (!pkg->dependencies[i]) continue; // Skip NULL dependencies

        // Parse dependency spec (may be package@version)
        char *dep_name = NULL;
        char *dep_version = NULL;
        parse_package_version(pkg->dependencies[i], &dep_name, &dep_version);
        const char *dep_spec = pkg->dependencies[i];

        // Skip self-dependency (package depending on itself)
        char *pkg_name_only = NULL;
        char *pkg_version_only = NULL;
        parse_package_version(package_name, &pkg_name_only, &pkg_version_only);
        if (pkg_name_only && dep_name && strcmp(pkg_name_only, dep_name) == 0) {
            // Package depends on itself - skip it
            free(pkg_name_only);
            free(pkg_version_only);
            if (dep_name) free(dep_name);
            if (dep_version) free(dep_version);
            continue;
        }
        if (pkg_name_only) free(pkg_name_only);
        if (pkg_version_only) free(pkg_version_only);

        // Check if already in result (compare by name, and version if specified)
        bool found = false;
        if (result) {
            for (size_t j = 0; j < *result_count; j++) {
                if (!result[j]) continue;

                char *res_name = NULL;
                char *res_version = NULL;
                parse_package_version(result[j], &res_name, &res_version);

                bool name_match = (strcmp(res_name ? res_name : result[j], dep_name ? dep_name : dep_spec) == 0);
                bool version_match = true;

                if (dep_version && res_version) {
                    version_match = (strcmp(dep_version, res_version) == 0);
                } else if (dep_version && !res_version) {
                    version_match = false; // Required version not in result
                }

                if (res_name) free(res_name);
                if (res_version) free(res_version);

                if (name_match && version_match) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            // Recursively resolve (pass the full dependency spec including version)
            log_developer("Recursively resolving dependency: %s", dep_spec);
            size_t deps_count = 0;
            char **deps = resolver_resolve(resolver, dep_spec, installed, installed_count, &deps_count);
            log_developer("Dependency resolution returned %zu packages for: %s", deps_count, dep_spec);

            // Clean up parsed dependency
            if (dep_name) free(dep_name);
            if (dep_version) free(dep_version);

            // Add dependencies to result (if resolution succeeded)
            if (deps && deps_count > 0) {
                for (size_t j = 0; j < deps_count; j++) {
                    if (deps[j]) { // Only add non-NULL dependencies
                        // Check if this dependency is already in result (by name)
                        bool already_added = false;
                        if (result) {
                            for (size_t k = 0; k < *result_count; k++) {
                                if (!result[k]) continue;

                                // Extract names for comparison
                                char *res_name = NULL;
                                char *res_version = NULL;
                                parse_package_version(result[k], &res_name, &res_version);

                                char *dep_name_check = NULL;
                                char *dep_version_check = NULL;
                                parse_package_version(deps[j], &dep_name_check, &dep_version_check);

                                const char *res_name_str = res_name ? res_name : result[k];
                                const char *dep_name_str = dep_name_check ? dep_name_check : deps[j];

                                // Match by name (version is optional)
                                if (strcmp(res_name_str, dep_name_str) == 0) {
                                    already_added = true;
                                }

                                if (res_name) free(res_name);
                                if (res_version) free(res_version);
                                if (dep_name_check) free(dep_name_check);
                                if (dep_version_check) free(dep_version_check);

                                if (already_added) break;
                            }
                        }

                        if (!already_added) {
                            if (*result_count >= capacity) {
                            capacity = capacity ? capacity * 2 : 8;
                            char **new_result = realloc(result, sizeof(char*) * capacity);
                            if (!new_result) {
                                // Allocation failed - clean up
                                for (size_t k = 0; k < *result_count; k++) {
                                    if (result && result[k]) free(result[k]);
                                }
                                if (result) free(result);
                                for (size_t k = 0; k < deps_count; k++) {
                                    if (deps[k]) free(deps[k]);
                                }
                                free(deps);
                                *result_count = 0;
                                return NULL;
                            }
                            result = new_result;
                        }
                        result[*result_count] = strdup(deps[j]);
                        if (!result[*result_count]) {
                            // strdup failed - clean up
                            for (size_t k = 0; k < *result_count; k++) {
                                if (result[k]) free(result[k]);
                            }
                            free(result);
                            for (size_t k = 0; k < deps_count; k++) {
                                if (deps[k]) free(deps[k]);
                            }
                            free(deps);
                            *result_count = 0;
                            return NULL;
                            }
                            (*result_count)++;
                        }
                    }
                    if (deps[j]) free(deps[j]);
                }
                free(deps);
            } else if (deps) {
                // Empty result - free it
                free(deps);
            } else {
                // deps is NULL - check if dependency exists in repository
                Package *dep_pkg = repository_get_package(resolver->repository, pkg->dependencies[i]);
                if (!dep_pkg) {
                    // Dependency not found in repository - this is an error
                    log_error("Dependency not found in repository: %s (required by %s)", pkg->dependencies[i], package_name);
                    if (result) {
                        for (size_t k = 0; k < *result_count; k++) {
                            if (result[k]) free(result[k]);
                        }
                        free(result);
                    }
                    *result_count = 0;
                    return NULL;
                }
                log_debug("Dependency already installed (skipping): %s", pkg->dependencies[i]);
                // Dependency exists but was already installed (and not using force) - skip it
            }
        }
    }

    // Add build dependencies
    if (pkg->build_dependencies_count > 0) {
        log_debug("Resolving %zu build dependencies for package: %s", pkg->build_dependencies_count, package_name);
    }
    for (size_t i = 0; i < pkg->build_dependencies_count; i++) {
        if (!pkg->build_dependencies[i]) continue; // Skip NULL dependencies

        // Skip self-dependency (package depending on itself)
        char *pkg_name_only = NULL;
        char *pkg_version_only = NULL;
        parse_package_version(package_name, &pkg_name_only, &pkg_version_only);
        char *build_dep_name = NULL;
        char *build_dep_version = NULL;
        parse_package_version(pkg->build_dependencies[i], &build_dep_name, &build_dep_version);
        if (pkg_name_only && build_dep_name && strcmp(pkg_name_only, build_dep_name) == 0) {
            // Package depends on itself - skip it
            free(pkg_name_only);
            free(pkg_version_only);
            if (build_dep_name) free(build_dep_name);
            if (build_dep_version) free(build_dep_version);
            continue;
        }
        if (pkg_name_only) free(pkg_name_only);
        if (pkg_version_only) free(pkg_version_only);
        if (build_dep_name) free(build_dep_name);
        if (build_dep_version) free(build_dep_version);

        bool found = false;
        if (result) {
            for (size_t j = 0; j < *result_count; j++) {
                if (result[j] && strcmp(result[j], pkg->build_dependencies[i]) == 0) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            size_t deps_count = 0;
            char **deps = resolver_resolve(resolver, pkg->build_dependencies[i], installed, installed_count, &deps_count);

            // Add dependencies to result (if resolution succeeded)
            if (deps && deps_count > 0) {
                for (size_t j = 0; j < deps_count; j++) {
                    if (deps[j]) { // Only add non-NULL dependencies
                        // Check if this dependency is already in result (by name)
                        bool already_added = false;
                        if (result) {
                            for (size_t k = 0; k < *result_count; k++) {
                                if (!result[k]) continue;

                                // Extract names for comparison
                                char *res_name = NULL;
                                char *res_version = NULL;
                                parse_package_version(result[k], &res_name, &res_version);

                                char *dep_name_check = NULL;
                                char *dep_version_check = NULL;
                                parse_package_version(deps[j], &dep_name_check, &dep_version_check);

                                const char *res_name_str = res_name ? res_name : result[k];
                                const char *dep_name_str = dep_name_check ? dep_name_check : deps[j];

                                // Match by name (version is optional)
                                if (strcmp(res_name_str, dep_name_str) == 0) {
                                    already_added = true;
                                }

                                if (res_name) free(res_name);
                                if (res_version) free(res_version);
                                if (dep_name_check) free(dep_name_check);
                                if (dep_version_check) free(dep_version_check);

                                if (already_added) break;
                            }
                        }

                        if (!already_added) {
                            if (*result_count >= capacity) {
                                capacity = capacity ? capacity * 2 : 8;
                                char **new_result = realloc(result, sizeof(char*) * capacity);
                                if (!new_result) {
                                    // Allocation failed - clean up
                                    for (size_t k = 0; k < *result_count; k++) {
                                        if (result && result[k]) free(result[k]);
                                    }
                                    if (result) free(result);
                                    for (size_t k = 0; k < deps_count; k++) {
                                        if (deps[k]) free(deps[k]);
                                    }
                                    free(deps);
                                    *result_count = 0;
                                    return NULL;
                                }
                                result = new_result;
                            }
                            result[*result_count] = strdup(deps[j]);
                            if (!result[*result_count]) {
                                // strdup failed - clean up
                                for (size_t k = 0; k < *result_count; k++) {
                                    if (result[k]) free(result[k]);
                                }
                                free(result);
                                for (size_t k = 0; k < deps_count; k++) {
                                    if (deps[k]) free(deps[k]);
                                }
                                free(deps);
                                *result_count = 0;
                                return NULL;
                            }
                            (*result_count)++;
                        }
                    }
                    if (deps[j]) free(deps[j]);
                }
                free(deps);
            } else if (deps) {
                // Empty result - free it
                free(deps);
            } else {
                // deps is NULL - check if build dependency exists in repository
                Package *dep_pkg = repository_get_package(resolver->repository, pkg->build_dependencies[i]);
                if (!dep_pkg) {
                    // Build dependency not found in repository - this is an error
                    if (result) {
                        for (size_t k = 0; k < *result_count; k++) {
                            if (result[k]) free(result[k]);
                        }
                        free(result);
                    }
                    *result_count = 0;
                    return NULL;
                }
                // Build dependency exists but was already installed (and not using force) - skip it
            }
        }
    }

    // Add this package (always add the package itself)
    if (*result_count >= capacity) {
        size_t new_capacity = capacity ? capacity * 2 : 1;
        char **new_result;
        if (result) {
            new_result = realloc(result, sizeof(char*) * new_capacity);
        } else {
            new_result = malloc(sizeof(char*) * new_capacity);
        }
        if (!new_result) {
            // Clean up existing result if any
            if (result) {
                for (size_t i = 0; i < *result_count; i++) {
                    if (result[i]) free(result[i]);
                }
                free(result);
            }
            *result_count = 0;
            return NULL;
        }
        result = new_result;
        capacity = new_capacity;
    }
    result[*result_count] = strdup(package_name);
    if (!result[*result_count]) {
        // strdup failed - clean up
        for (size_t i = 0; i < *result_count; i++) {
            if (result[i]) free(result[i]);
        }
        free(result);
        *result_count = 0;
        return NULL;
    }
    (*result_count)++;

    // Remove from visited list before returning
    for (size_t i = 0; i < resolver->visited_count; i++) {
        if (resolver->visited[i] && strcmp(resolver->visited[i], package_name) == 0) {
            free(resolver->visited[i]);
            // Shift remaining entries
            for (size_t j = i; j < resolver->visited_count - 1; j++) {
                resolver->visited[j] = resolver->visited[j + 1];
            }
            resolver->visited_count--;
            resolver->visited[resolver->visited_count] = NULL;
            break;
        }
    }

    // Reset visited list if we're back at the top level (visited_count == 0)
    if (resolver->visited_count == 0 && resolver->visited) {
        free(resolver->visited);
        resolver->visited = NULL;
        resolver->visited_capacity = 0;
    }

    log_debug("Dependency resolution completed for %s: %zu total packages", package_name, *result_count);
    return result;
}

char** resolver_get_build_order(DependencyResolver *resolver, char **packages, size_t packages_count, size_t *result_count) {
    log_debug("Calculating build order for %zu packages", packages_count);
    // Simple topological sort
    *result_count = 0;
    if (packages_count == 0) {
        log_warning("No packages provided for build order calculation");
        return NULL;
    }

    // Special case: single package with no dependencies
    if (packages_count == 1) {
        char **result = malloc(sizeof(char*));
        result[0] = strdup(packages[0]);
        *result_count = 1;
        return result;
    }

    char **result = calloc(packages_count, sizeof(char*));  // Initialize to NULL
    if (!result) return NULL;

    bool *added = calloc(packages_count, sizeof(bool));
    if (!added) {
        free(result);
        return NULL;
    }

    // Build dependency graph
    int *in_degree = calloc(packages_count, sizeof(int));
    if (!in_degree) {
        free(result);
        free(added);
        return NULL;
    }

    for (size_t i = 0; i < packages_count; i++) {
        // Parse package@version if present
        char *pkg_name = NULL;
        char *pkg_version = NULL;
        parse_package_version(packages[i], &pkg_name, &pkg_version);
        const char *actual_name = pkg_name ? pkg_name : packages[i];

        Package *pkg = pkg_version ? repository_get_package_version(resolver->repository, actual_name, pkg_version) : repository_get_package(resolver->repository, actual_name);
        if (pkg) {
            for (size_t j = 0; j < pkg->dependencies_count; j++) {
                if (!pkg->dependencies[j]) continue;
                int dep_idx = find_package_index(packages, packages_count, pkg->dependencies[j]);
                if (dep_idx >= 0) {
                    in_degree[i]++;
                }
            }
            for (size_t j = 0; j < pkg->build_dependencies_count; j++) {
                if (!pkg->build_dependencies[j]) continue;
                int dep_idx = find_package_index(packages, packages_count, pkg->build_dependencies[j]);
                if (dep_idx >= 0) {
                    in_degree[i]++;
                }
            }
        } else {
            // Package not found in repository - this is an error
            // But we'll continue and handle it later
        }

        if (pkg_name) free(pkg_name);
        if (pkg_version) free(pkg_version);
    }

    // Topological sort
    log_developer("Starting topological sort: packages_count=%zu", packages_count);
    for (size_t i = 0; i < packages_count; i++) {
        log_developer("  Initial in_degree[%zu] for '%s': %d", i, packages[i], in_degree[i]);
    }
    while (*result_count < packages_count) {
        log_developer("Topological sort iteration: *result_count=%zu, packages_count=%zu", *result_count, packages_count);
        bool found = false;
        for (size_t i = 0; i < packages_count; i++) {
            if (!added[i] && in_degree[i] == 0) {
                log_developer("Adding package %zu: '%s' (in_degree=0)", *result_count, packages[i]);
                char *dup_str = strdup(packages[i]);
                if (!dup_str) {
                    log_error("strdup failed for '%s'", packages[i]);
                    continue;
                }
                result[*result_count] = dup_str;
                log_developer("  Assigned result[%zu] = '%s' (pointer: %p)", *result_count, result[*result_count], (void*)result[*result_count]);
                (*result_count)++;
                log_developer("  Incremented *result_count to %zu", *result_count);
                added[i] = true;
                found = true;

                // Decrease in-degree of dependents
                // Parse package name from packages[i] (may be "package@version")
                char *added_pkg_name = NULL;
                char *added_pkg_version = NULL;
                parse_package_version(packages[i], &added_pkg_name, &added_pkg_version);
                const char *added_name = added_pkg_name ? added_pkg_name : packages[i];

                Package *pkg = repository_get_package(resolver->repository, added_name);
                if (pkg) {
                    for (size_t j = 0; j < packages_count; j++) {
                        if (!added[j]) {
                            // Parse package name from packages[j] (may be "package@version")
                            char *other_pkg_name = NULL;
                            char *other_pkg_version = NULL;
                            parse_package_version(packages[j], &other_pkg_name, &other_pkg_version);
                            const char *other_name = other_pkg_name ? other_pkg_name : packages[j];

                            Package *other = repository_get_package(resolver->repository, other_name);
                            if (other) {
                                if (package_has_dependency(other, added_name)) {
                                    log_developer("  Package '%s' depends on '%s', decreasing in_degree[%zu] from %d to %d",
                                                  other_name, added_name, j, in_degree[j], in_degree[j] - 1);
                                    in_degree[j]--;
                                }
                            }
                            if (other_pkg_name) free(other_pkg_name);
                            if (other_pkg_version) free(other_pkg_version);
                        }
                    }
                }
                if (added_pkg_name) free(added_pkg_name);
                if (added_pkg_version) free(added_pkg_version);
                break;  // Break inner for loop, continue while loop
            }
        }
        if (!found) {
            log_warning("No package found with in_degree=0, but *result_count=%zu < packages_count=%zu", *result_count, packages_count);
            // Circular dependency or error - check if we added all packages
            if (*result_count < packages_count) {
                // Failed to add all packages - free result and return NULL
                for (size_t i = 0; i < *result_count; i++) {
                    free(result[i]);
                }
                free(result);
                free(added);
                free(in_degree);
                return NULL;
            }
            break;
        }
    }

    // Verify all packages were added
    if (*result_count < packages_count) {
        log_error("Build order calculation incomplete (added %zu of %zu packages)", *result_count, packages_count);
        // Failed to add all packages - free result and return NULL
        for (size_t i = 0; i < *result_count; i++) {
            free(result[i]);
        }
        free(result);
        free(added);
        free(in_degree);
        *result_count = 0;
        return NULL;
    }

    log_debug("Build order calculated successfully: %zu packages", *result_count);
    log_developer("About to return from resolver_get_build_order: result=%p, *result_count=%zu", (void*)result, *result_count);
    free(added);
    free(in_degree);
    log_developer("After freeing memory: result=%p, *result_count=%zu", (void*)result, *result_count);
    return result;
}

bool resolver_has_circular_dependency(DependencyResolver *resolver, const char *package_name) {
    // TODO: Implement cycle detection
    (void)resolver;
    (void)package_name;
    return false;
}

Repository* repository_new(const char *repo_dir) {
    log_developer("repository_new called with repo_dir='%s'", repo_dir);
    Repository *repo = malloc(sizeof(Repository));
    if (!repo) {
        log_error("Failed to allocate memory for Repository");
        return NULL;
    }

    repo->packages = NULL;
    repo->packages_count = 0;

    // Load packages from directory
    DIR *dir = opendir(repo_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;

            // Only process .json files
            size_t name_len = strlen(entry->d_name);
            if (name_len < 5 || strcmp(entry->d_name + name_len - 5, ".json") != 0) {
                continue;
            }

            char path[512];
            snprintf(path, sizeof(path), "%s/%s", repo_dir, entry->d_name);

            struct stat st;
            if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                // Load file content
                FILE *f = fopen(path, "r");
                if (!f) continue;

                fseek(f, 0, SEEK_END);
                long file_size = ftell(f);
                fseek(f, 0, SEEK_SET);

                if (file_size <= 0 || file_size > 1024 * 1024) { // Max 1MB
                    fclose(f);
                    continue;
                }

                char *json = malloc(file_size + 1);
                if (!json) {
                    fclose(f);
                    continue;
                }

                size_t read = fread(json, 1, file_size, f);
                fclose(f);
                json[read] = '\0';

                // Check if this is a multi-version file (has "versions" array)
                // Look for "versions" key followed by ':' and then '['
                const char *versions_key = strstr(json, "\"versions\"");
                bool is_multi_version = false;
                if (versions_key) {
                    // Check if it's followed by ':' (making it a key) and then '[' (making it an array)
                    const char *colon = strchr(versions_key, ':');
                    if (colon) {
                        colon++; // Skip ':'
                        while (isspace(*colon)) colon++; // Skip whitespace
                        if (*colon == '[') {
                            // This is a "versions" array - multi-version format
                            // Multi-version format: {"name": "...", "versions": [...]}
                            // Parse each version and create a Package for each
                            const char *versions_start = colon; // Already points to '['
                            versions_start++; // Skip '['

                        // Find the package name first (shared across all versions)
                        char *package_name = NULL;
                        const char *name_pos = strstr(json, "\"name\"");
                        if (name_pos) {
                            name_pos = strchr(name_pos, ':');
                            if (name_pos) {
                                name_pos++;
                                while (isspace(*name_pos)) name_pos++;
                                if (*name_pos == '"') {
                                    name_pos++;
                                    const char *name_end = strchr(name_pos, '"');
                                    if (name_end) {
                                        size_t name_len = name_end - name_pos;
                                        package_name = malloc(name_len + 1);
                                        if (package_name) {
                                            strncpy(package_name, name_pos, name_len);
                                            package_name[name_len] = '\0';
                                        }
                                    }
                                }
                            }
                        }

                        // Parse each version object in the array
                        const char *p = versions_start;
                        int brace_depth = 0;
                        int bracket_depth = 1; // Start at 1 since we're already inside the versions array '['
                        const char *obj_start = NULL;

                        while (*p) {
                            if (*p == '[') {
                                bracket_depth++;
                            } else if (*p == ']') {
                                bracket_depth--;
                                // Only stop if we've closed the versions array (bracket_depth == 0)
                                if (bracket_depth == 0) {
                                    break;
                                }
                            } else if (*p == '{') {
                                if (brace_depth == 0 && bracket_depth == 1) {
                                    // Top-level object in versions array
                                    obj_start = p;
                                }
                                brace_depth++;
                            } else if (*p == '}') {
                                brace_depth--;
                                if (brace_depth == 0 && bracket_depth == 1 && obj_start) {
                                    // Extract this version object
                                    size_t obj_len = p - obj_start + 1;
                                    char *version_json = malloc(obj_len + 1);
                                    if (version_json) {
                                        strncpy(version_json, obj_start, obj_len);
                                        version_json[obj_len] = '\0';

                                        // Create package from this version
                                        // First, add the name to the version JSON temporarily
                                        char *version_json_with_name = NULL;
                                        if (package_name) {
                                            // Insert name at the beginning: {"name":"...", ...existing json...}
                                            size_t name_json_len = strlen(package_name) + 20; // "{\"name\":\"\","
                                            size_t version_json_len = strlen(version_json);
                                            version_json_with_name = malloc(name_json_len + version_json_len + 1);
                                            if (version_json_with_name) {
                                                snprintf(version_json_with_name, name_json_len + version_json_len + 1,
                                                        "{\"name\":\"%s\",%s", package_name, version_json + 1); // Skip first {
                                            }
                                        }

                                        Package *pkg = package_new();
                                        const char *json_to_parse = version_json_with_name ? version_json_with_name : version_json;
                                        if (package_load_from_json(pkg, json_to_parse)) {
                                            repo->packages = realloc(repo->packages, sizeof(Package*) * (repo->packages_count + 1));
                                            repo->packages[repo->packages_count++] = pkg;
                                        } else {
                                            package_free(pkg);
                                        }

                                        if (version_json_with_name) {
                                            free(version_json_with_name);
                                        }
                                        free(version_json);
                                    }
                                    obj_start = NULL;
                                }
                            }
                            p++;
                        }

                            if (package_name) free(package_name);
                        }
                    }
                }

                if (!is_multi_version) {
                    // Single version format: {"name": "...", "version": "...", ...}
                    Package *pkg = package_new();
                    if (package_load_from_file(pkg, path)) {
                        repo->packages = realloc(repo->packages, sizeof(Package*) * (repo->packages_count + 1));
                        repo->packages[repo->packages_count++] = pkg;
                    } else {
                        package_free(pkg);
                    }
                }

                free(json);
            }
        }
        closedir(dir);
    }

    log_debug("Repository loaded: %zu packages from %s", repo->packages_count, repo_dir);
    return repo;
}

void repository_free(Repository *repo) {
    if (!repo) return;
    for (size_t i = 0; i < repo->packages_count; i++) {
        package_free(repo->packages[i]);
    }
    free(repo->packages);
    free(repo);
}

Package* repository_get_package(Repository *repo, const char *name) {
    log_developer("repository_get_package called for: %s", name);
    // Return the latest version (highest version string, or first found if no version specified)
    Package *latest = NULL;
    for (size_t i = 0; i < repo->packages_count; i++) {
        if (strcmp(repo->packages[i]->name, name) == 0) {
            if (!latest) {
                latest = repo->packages[i];
            } else if (repo->packages[i]->version && latest->version) {
                // Simple version comparison (lexicographic, works for semantic versions)
                if (strcmp(repo->packages[i]->version, latest->version) > 0) {
                    latest = repo->packages[i];
                }
            }
        }
    }
    if (latest) {
        log_debug("Package found: %s@%s", latest->name, latest->version ? latest->version : "latest");
    } else {
        log_warning("Package not found in repository: %s", name);
    }
    return latest;
}

Package* repository_get_package_version(Repository *repo, const char *name, const char *version) {
    log_developer("repository_get_package_version called for: %s@%s", name, version ? version : "latest");
    if (!version || strcmp(version, "latest") == 0) {
        return repository_get_package(repo, name);
    }

    for (size_t i = 0; i < repo->packages_count; i++) {
        if (strcmp(repo->packages[i]->name, name) == 0) {
            if (repo->packages[i]->version && strcmp(repo->packages[i]->version, version) == 0) {
                log_debug("Package version found: %s@%s", name, version);
                return repo->packages[i];
            }
        }
    }
    log_warning("Package version not found in repository: %s@%s", name, version);
    return NULL;
}

char** repository_list_versions(Repository *repo, const char *name, size_t *count) {
    *count = 0;
    char **versions = NULL;

    for (size_t i = 0; i < repo->packages_count; i++) {
        if (strcmp(repo->packages[i]->name, name) == 0) {
            const char *version = repo->packages[i]->version ? repo->packages[i]->version : "latest";
            versions = realloc(versions, sizeof(char*) * (*count + 1));
            versions[*count] = strdup(version);
            (*count)++;
        }
    }

    return versions;
}

bool repository_add_package(Repository *repo, Package *pkg) {
    if (repository_get_package(repo, pkg->name)) {
        return false; // Already exists
    }

    repo->packages = realloc(repo->packages, sizeof(Package*) * (repo->packages_count + 1));
    repo->packages[repo->packages_count++] = pkg;
    return true;
}

char** repository_list_packages(Repository *repo, size_t *count) {
    *count = repo->packages_count;
    if (repo->packages_count == 0) return NULL;

    char **list = malloc(sizeof(char*) * repo->packages_count);
    for (size_t i = 0; i < repo->packages_count; i++) {
        list[i] = strdup(repo->packages[i]->name);
    }
    return list;
}

