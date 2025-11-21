#ifndef DATABASE_H
#define DATABASE_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *name;
    char *version;
    char *install_path;
    time_t installed_at;
    char **dependencies;
    size_t dependencies_count;
} InstalledPackage;

typedef struct {
    char *db_path;
    InstalledPackage *packages;
    size_t packages_count;
} Database;

// Database functions
Database* database_new(const char *db_dir);
void database_free(Database *db);
bool database_load(Database *db);
bool database_save(Database *db);
bool database_is_installed(const Database *db, const char *package_name);
bool database_add_package(Database *db, const char *name, const char *version, const char *install_path, const char **deps, size_t deps_count);
bool database_remove_package(Database *db, const char *package_name);
InstalledPackage* database_get_package(const Database *db, const char *package_name);
char** database_list_installed(const Database *db, size_t *count);

#ifdef __cplusplus
}
#endif

#endif // DATABASE_H

