#include "fetcher.h"
#include "package.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

SourceFetcher* fetcher_new(const char *source_dir) {
    SourceFetcher *fetcher = malloc(sizeof(SourceFetcher));
    if (!fetcher) return NULL;

    fetcher->source_dir = strdup(source_dir);

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(source_dir, &st) == -1) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", source_dir);
        system(cmd);
    }

    return fetcher;
}

void fetcher_free(SourceFetcher *fetcher) {
    if (!fetcher) return;
    free(fetcher->source_dir);
    free(fetcher);
}

bool fetcher_download_file(const char *url, const char *dest) {
    char cmd[1024];

    // Try wget first
    snprintf(cmd, sizeof(cmd), "wget -q -O '%s' '%s' 2>/dev/null", dest, url);
    if (system(cmd) == 0) {
        struct stat st;
        if (stat(dest, &st) == 0 && st.st_size > 0) {
            return true;
        }
    }

    // Try curl
    snprintf(cmd, sizeof(cmd), "curl -fsSL -o '%s' '%s' 2>/dev/null", dest, url);
    if (system(cmd) == 0) {
        struct stat st;
        if (stat(dest, &st) == 0 && st.st_size > 0) {
            return true;
        }
    }

    return false;
}

bool fetcher_extract_tarball(const char *archive, const char *dest) {
    char cmd[1024];

    // Try tar with different compression
    snprintf(cmd, sizeof(cmd), "tar -xzf '%s' -C '%s' 2>/dev/null", archive, dest);
    if (system(cmd) == 0) return true;

    snprintf(cmd, sizeof(cmd), "tar -xjf '%s' -C '%s' 2>/dev/null", archive, dest);
    if (system(cmd) == 0) return true;

    snprintf(cmd, sizeof(cmd), "tar -xf '%s' -C '%s' 2>/dev/null", archive, dest);
    if (system(cmd) == 0) return true;

    return false;
}

bool fetcher_clone_git(const char *url, const char *dest, const char *branch, const char *tag, const char *commit) {
    char cmd[1024];

    // Clone repository
    if (tag) {
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 --branch '%s' '%s' '%s' 2>/dev/null", tag, url, dest);
    } else if (branch) {
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 --branch '%s' '%s' '%s' 2>/dev/null", branch, url, dest);
    } else {
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 '%s' '%s' 2>/dev/null", url, dest);
    }

    if (system(cmd) != 0) {
        return false;
    }

    // Checkout specific commit if needed
    if (commit) {
        snprintf(cmd, sizeof(cmd), "cd '%s' && git checkout '%s' 2>/dev/null", dest, commit);
        system(cmd);
    }

    return true;
}

char* fetcher_fetch(SourceFetcher *fetcher, Package *pkg, bool force) {
    if (!fetcher || !pkg || !pkg->source_type) {
        return NULL;
    }

    char package_dir[512];
    snprintf(package_dir, sizeof(package_dir), "%s/%s", fetcher->source_dir, pkg->name);

    // Check if already exists
    struct stat st;
    if (stat(package_dir, &st) == 0 && !force) {
        return strdup(package_dir);
    }

    // Remove existing if force
    if (force && stat(package_dir, &st) == 0) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", package_dir);
        system(cmd);
    }

    if (strcmp(pkg->source_type, "git") == 0) {
        if (!pkg->source_url) {
            return NULL;
        }

        if (fetcher_clone_git(pkg->source_url, package_dir, pkg->source_branch, pkg->source_tag, pkg->source_commit)) {
            return strdup(package_dir);
        }
    } else if (strcmp(pkg->source_type, "tarball") == 0 || strcmp(pkg->source_type, "zip") == 0) {
        if (!pkg->source_url) {
            return NULL;
        }

        // Download archive
        char archive[1024];
        snprintf(archive, sizeof(archive), "%s/%s", fetcher->source_dir, strrchr(pkg->source_url, '/') ? strrchr(pkg->source_url, '/') + 1 : "archive");

        if (!fetcher_download_file(pkg->source_url, archive)) {
            return NULL;
        }

        // Create destination directory
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", package_dir);
        system(cmd);

        // Extract
        if (fetcher_extract_tarball(archive, package_dir)) {
            // Move contents if archive has single top-level directory
            DIR *dir = opendir(package_dir);
            if (dir) {
                struct dirent *entry;
                int count = 0;
                char single_dir[1024] = "";
                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_name[0] != '.') {
                        count++;
                        if (count == 1) {
                            snprintf(single_dir, sizeof(single_dir), "%s/%s", package_dir, entry->d_name);
                        }
                    }
                }
                closedir(dir);

                if (count == 1) {
                    // Move contents up one level
                    char temp[1024];
                    snprintf(temp, sizeof(temp), "%s/temp", package_dir);
                    rename(single_dir, temp);

                    dir = opendir(temp);
                    if (dir) {
                        while ((entry = readdir(dir)) != NULL) {
                            if (entry->d_name[0] != '.') {
                                char src[1024], dst[1024];
                                int src_len = snprintf(src, sizeof(src), "%s/%s", temp, entry->d_name);
                                int dst_len = snprintf(dst, sizeof(dst), "%s/%s", package_dir, entry->d_name);
                                if (src_len >= 0 && (size_t)src_len < sizeof(src) &&
                                    dst_len >= 0 && (size_t)dst_len < sizeof(dst)) {
                                    rename(src, dst);
                                }
                            }
                        }
                        closedir(dir);
                    }
                    rmdir(temp);
                }
            }

            return strdup(package_dir);
        }
    } else if (strcmp(pkg->source_type, "local") == 0) {
        if (!pkg->source_url) {
            return NULL;
        }

        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "cp -r '%s' '%s' 2>/dev/null", pkg->source_url, package_dir);
        if (system(cmd) == 0) {
            struct stat st;
            if (stat(package_dir, &st) == 0) {
                return strdup(package_dir);
            }
        }
    }

    return NULL;
}

