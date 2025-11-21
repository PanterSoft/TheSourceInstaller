#include "resolver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

// Simple topological sort for build order
static int find_package_index(char **packages, size_t count, const char *name) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(packages[i], name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

DependencyResolver* resolver_new(Repository *repo) {
    DependencyResolver *resolver = malloc(sizeof(DependencyResolver));
    if (!resolver) return NULL;
    resolver->repository = repo;
    return resolver;
}

void resolver_free(DependencyResolver *resolver) {
    free(resolver);
}

char** resolver_resolve(DependencyResolver *resolver, const char *package_name, char **installed, size_t installed_count, size_t *result_count) {
    *result_count = 0;
    char **result = NULL;
    size_t capacity = 0;

    // Simple recursive resolution (no cycle detection for now)
    // Check if already installed
    for (size_t i = 0; i < installed_count; i++) {
        if (installed[i] && strcmp(installed[i], package_name) == 0) {
            return NULL; // Already installed
        }
    }

    // Get package
    Package *pkg = repository_get_package(resolver->repository, package_name);
    if (!pkg) {
        return NULL;
    }

    // Resolve dependencies first
    for (size_t i = 0; i < pkg->dependencies_count; i++) {
        if (!pkg->dependencies[i]) continue; // Skip NULL dependencies
        // Check if already in result
        bool found = false;
        if (result) {
            for (size_t j = 0; j < *result_count; j++) {
                if (result[j] && strcmp(result[j], pkg->dependencies[i]) == 0) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            // Recursively resolve
            size_t deps_count = 0;
            char **deps = resolver_resolve(resolver, pkg->dependencies[i], installed, installed_count, &deps_count);

            // Add dependencies to result (if resolution succeeded)
            if (deps && deps_count > 0) {
                for (size_t j = 0; j < deps_count; j++) {
                    if (deps[j]) { // Only add non-NULL dependencies
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
                    if (result) {
                        for (size_t k = 0; k < *result_count; k++) {
                            if (result[k]) free(result[k]);
                        }
                        free(result);
                    }
                    *result_count = 0;
                    return NULL;
                }
                // Dependency exists but was already installed (and not using force) - skip it
            }
        }
    }

    // Add build dependencies
    for (size_t i = 0; i < pkg->build_dependencies_count; i++) {
        if (!pkg->build_dependencies[i]) continue; // Skip NULL dependencies
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

    return result;
}

char** resolver_get_build_order(DependencyResolver *resolver, char **packages, size_t packages_count, size_t *result_count) {
    // Simple topological sort
    *result_count = 0;
    if (packages_count == 0) {
        return NULL;
    }

    // Special case: single package with no dependencies
    if (packages_count == 1) {
        char **result = malloc(sizeof(char*));
        result[0] = strdup(packages[0]);
        *result_count = 1;
        return result;
    }

    char **result = malloc(sizeof(char*) * packages_count);
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
        Package *pkg = repository_get_package(resolver->repository, packages[i]);
        if (pkg) {
            for (size_t j = 0; j < pkg->dependencies_count; j++) {
                int dep_idx = find_package_index(packages, packages_count, pkg->dependencies[j]);
                if (dep_idx >= 0) {
                    in_degree[i]++;
                }
            }
            for (size_t j = 0; j < pkg->build_dependencies_count; j++) {
                int dep_idx = find_package_index(packages, packages_count, pkg->build_dependencies[j]);
                if (dep_idx >= 0) {
                    in_degree[i]++;
                }
            }
        } else {
            // Package not found in repository - this is an error
            // But we'll continue and handle it later
        }
    }

    // Topological sort
    while (*result_count < packages_count) {
        bool found = false;
        for (size_t i = 0; i < packages_count; i++) {
            if (!added[i] && in_degree[i] == 0) {
                result[*result_count++] = strdup(packages[i]);
                added[i] = true;
                found = true;

                // Decrease in-degree of dependents
                Package *pkg = repository_get_package(resolver->repository, packages[i]);
                if (pkg) {
                    for (size_t j = 0; j < packages_count; j++) {
                        if (!added[j]) {
                            Package *other = repository_get_package(resolver->repository, packages[j]);
                            if (other) {
                                if (package_has_dependency(other, packages[i])) {
                                    in_degree[j]--;
                                }
                            }
                        }
                    }
                }
                break;
            }
        }
        if (!found) {
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
        // Failed to add all packages - free result and return NULL
        for (size_t i = 0; i < *result_count; i++) {
            free(result[i]);
        }
        free(result);
        free(added);
        free(in_degree);
        return NULL;
    }

    free(added);
    free(in_degree);
    return result;
}

bool resolver_has_circular_dependency(DependencyResolver *resolver, const char *package_name) {
    // TODO: Implement cycle detection
    (void)resolver;
    (void)package_name;
    return false;
}

Repository* repository_new(const char *repo_dir) {
    Repository *repo = malloc(sizeof(Repository));
    if (!repo) return NULL;

    repo->packages = NULL;
    repo->packages_count = 0;

    // Load packages from directory
    DIR *dir = opendir(repo_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') continue;

            char path[512];
            snprintf(path, sizeof(path), "%s/%s", repo_dir, entry->d_name);

            struct stat st;
            if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                // Try to load as package
                Package *pkg = package_new();
                if (package_load_from_file(pkg, path)) {
                    repo->packages = realloc(repo->packages, sizeof(Package*) * (repo->packages_count + 1));
                    repo->packages[repo->packages_count++] = pkg;
                } else {
                    package_free(pkg);
                }
            }
        }
        closedir(dir);
    }

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
    for (size_t i = 0; i < repo->packages_count; i++) {
        if (strcmp(repo->packages[i]->name, name) == 0) {
            return repo->packages[i];
        }
    }
    return NULL;
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

