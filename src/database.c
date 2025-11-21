#include "database.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

Database* database_new(const char *db_dir) {
    Database *db = calloc(1, sizeof(Database));
    if (!db) return NULL;

    db->db_path = malloc(strlen(db_dir) + 20);
    snprintf(db->db_path, strlen(db_dir) + 20, "%s/installed.json", db_dir);

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(db_dir, &st) == -1) {
        // Create directory (simple mkdir, no recursive)
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", db_dir);
        system(cmd);
    }

    database_load(db);
    return db;
}

void database_free(Database *db) {
    if (!db) return;

    free(db->db_path);

    for (size_t i = 0; i < db->packages_count; i++) {
        free(db->packages[i].name);
        free(db->packages[i].version);
        free(db->packages[i].install_path);
        for (size_t j = 0; j < db->packages[i].dependencies_count; j++) {
            free(db->packages[i].dependencies[j]);
        }
        free(db->packages[i].dependencies);
    }
    free(db->packages);
    free(db);
}

bool database_load(Database *db) {
    FILE *f = fopen(db->db_path, "r");
    if (!f) {
        db->packages_count = 0;
        db->packages = NULL;
        return true; // No database yet is OK
    }

    // Simple JSON parsing for installed packages
    // For now, just check if file exists and is valid JSON
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size == 0) {
        fclose(f);
        db->packages_count = 0;
        db->packages = NULL;
        return true;
    }

    // TODO: Implement proper JSON parsing
    // For now, just mark as loaded
    fclose(f);
    return true;
}

bool database_save(Database *db) {
    FILE *f = fopen(db->db_path, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"installed\": [\n");

    for (size_t i = 0; i < db->packages_count; i++) {
        InstalledPackage *pkg = &db->packages[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", pkg->name);
        fprintf(f, "      \"version\": \"%s\",\n", pkg->version);
        fprintf(f, "      \"install_path\": \"%s\",\n", pkg->install_path);
        fprintf(f, "      \"installed_at\": %ld,\n", (long)pkg->installed_at);
        fprintf(f, "      \"dependencies\": [");
        for (size_t j = 0; j < pkg->dependencies_count; j++) {
            fprintf(f, "\"%s\"", pkg->dependencies[j]);
            if (j < pkg->dependencies_count - 1) fprintf(f, ", ");
        }
        fprintf(f, "]\n");
        fprintf(f, "    }");
        if (i < db->packages_count - 1) fprintf(f, ",");
        fprintf(f, "\n");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}

bool database_is_installed(const Database *db, const char *package_name) {
    for (size_t i = 0; i < db->packages_count; i++) {
        if (strcmp(db->packages[i].name, package_name) == 0) {
            return true;
        }
    }
    return false;
}

bool database_add_package(Database *db, const char *name, const char *version, const char *install_path, const char **deps, size_t deps_count) {
    // Check if already exists
    if (database_is_installed(db, name)) {
        return false;
    }

    db->packages = realloc(db->packages, sizeof(InstalledPackage) * (db->packages_count + 1));
    InstalledPackage *pkg = &db->packages[db->packages_count];

    pkg->name = strdup(name);
    pkg->version = strdup(version ? version : "unknown");
    pkg->install_path = strdup(install_path ? install_path : "");
    pkg->installed_at = time(NULL);
    pkg->dependencies_count = deps_count;
    pkg->dependencies = NULL;

    if (deps_count > 0) {
        pkg->dependencies = malloc(sizeof(char*) * deps_count);
        for (size_t i = 0; i < deps_count; i++) {
            pkg->dependencies[i] = strdup(deps[i]);
        }
    }

    db->packages_count++;
    return database_save(db);
}

bool database_remove_package(Database *db, const char *package_name) {
    for (size_t i = 0; i < db->packages_count; i++) {
        if (strcmp(db->packages[i].name, package_name) == 0) {
            // Free package data
            free(db->packages[i].name);
            free(db->packages[i].version);
            free(db->packages[i].install_path);
            for (size_t j = 0; j < db->packages[i].dependencies_count; j++) {
                free(db->packages[i].dependencies[j]);
            }
            free(db->packages[i].dependencies);

            // Move remaining packages
            if (i < db->packages_count - 1) {
                memmove(&db->packages[i], &db->packages[i + 1],
                       sizeof(InstalledPackage) * (db->packages_count - i - 1));
            }

            db->packages_count--;
            db->packages = realloc(db->packages, sizeof(InstalledPackage) * db->packages_count);
            return database_save(db);
        }
    }
    return false;
}

InstalledPackage* database_get_package(const Database *db, const char *package_name) {
    for (size_t i = 0; i < db->packages_count; i++) {
        if (strcmp(db->packages[i].name, package_name) == 0) {
            return &db->packages[i];
        }
    }
    return NULL;
}

char** database_list_installed(const Database *db, size_t *count) {
    *count = db->packages_count;
    if (db->packages_count == 0) return NULL;

    char **list = malloc(sizeof(char*) * db->packages_count);
    for (size_t i = 0; i < db->packages_count; i++) {
        list[i] = strdup(db->packages[i].name);
    }
    return list;
}

