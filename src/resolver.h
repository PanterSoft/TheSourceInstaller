#ifndef RESOLVER_H
#define RESOLVER_H

#include <stdbool.h>
#include <stddef.h>
#include "package.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Package **packages;
    size_t packages_count;
} Repository;

// Dependency resolver
typedef struct {
    Repository *repository;
    char **visited;  // For cycle detection during resolution
    size_t visited_count;
    size_t visited_capacity;
} DependencyResolver;

// Resolver functions
DependencyResolver* resolver_new(Repository *repo);
void resolver_free(DependencyResolver *resolver);
char** resolver_resolve(DependencyResolver *resolver, const char *package_name, char **installed, size_t installed_count, size_t *result_count);
char** resolver_get_build_order(DependencyResolver *resolver, char **packages, size_t packages_count, size_t *result_count);
bool resolver_has_circular_dependency(DependencyResolver *resolver, const char *package_name);

// Repository functions
Repository* repository_new(const char *repo_dir);
void repository_free(Repository *repo);
Package* repository_get_package(Repository *repo, const char *name);
Package* repository_get_package_version(Repository *repo, const char *name, const char *version);
char** repository_list_versions(Repository *repo, const char *name, size_t *count);
bool repository_add_package(Repository *repo, Package *pkg);
char** repository_list_packages(Repository *repo, size_t *count);

#ifdef __cplusplus
}
#endif

#endif // RESOLVER_H

