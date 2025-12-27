#include "fetcher.h"
#include "package.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

SourceFetcher* fetcher_new(const char *source_dir) {
    log_developer("fetcher_new called with source_dir='%s'", source_dir);
    SourceFetcher *fetcher = malloc(sizeof(SourceFetcher));
    if (!fetcher) {
        log_error("Failed to allocate memory for SourceFetcher");
        return NULL;
    }

    fetcher->source_dir = strdup(source_dir);

    // Create directory if it doesn't exist
    struct stat st = {0};
    if (stat(source_dir, &st) == -1) {
        log_debug("Source directory does not exist, creating: %s", source_dir);
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", source_dir);
        system(cmd);
    } else {
        log_developer("Source directory already exists: %s", source_dir);
    }

    log_debug("SourceFetcher initialized with source_dir: %s", source_dir);
    return fetcher;
}

void fetcher_free(SourceFetcher *fetcher) {
    if (!fetcher) return;
    free(fetcher->source_dir);
    free(fetcher);
}

bool fetcher_download_file(const char *url, const char *dest) {
    log_debug("Downloading file: %s -> %s", url, dest);
    char cmd[1024];

    // Try wget first
    snprintf(cmd, sizeof(cmd), "wget -q -O '%s' '%s' 2>/dev/null", dest, url);
    if (system(cmd) == 0) {
        struct stat st;
        if (stat(dest, &st) == 0 && st.st_size > 0) {
            log_info("File downloaded successfully using wget: %s (%ld bytes)", dest, (long)st.st_size);
            return true;
        }
    }

    // Try curl
    log_developer("wget failed, trying curl");
    snprintf(cmd, sizeof(cmd), "curl -fsSL -o '%s' '%s' 2>/dev/null", dest, url);
    if (system(cmd) == 0) {
        struct stat st;
        if (stat(dest, &st) == 0 && st.st_size > 0) {
            log_info("File downloaded successfully using curl: %s (%ld bytes)", dest, (long)st.st_size);
            return true;
        }
    }

    log_error("Failed to download file: %s -> %s", url, dest);
    return false;
}

bool fetcher_extract_tarball(const char *archive, const char *dest) {
    // Verify archive exists and is not empty
    struct stat st;
    if (stat(archive, &st) != 0) {
        log_error("Archive file does not exist: %s", archive);
        return false;
    }
    if (st.st_size == 0) {
        log_error("Archive file is empty: %s", archive);
        return false;
    }
    log_debug("Archive file exists: %s (%ld bytes)", archive, (long)st.st_size);

    char cmd[1024];
    int result;

    // Try tar with different compression formats
    // Note: We capture stderr to a temp file so we can show it if all attempts fail
    char error_file[512];
    snprintf(error_file, sizeof(error_file), "%s/tar_error.log", dest);

    // Try xz compression first (common for .tar.xz files)
    snprintf(cmd, sizeof(cmd), "tar -xJf '%s' -C '%s' 2>'%s'", archive, dest, error_file);
    result = system(cmd);
    if (result == 0) {
        // Verify extraction succeeded by checking if files were extracted
        DIR *dir = opendir(dest);
        if (dir) {
            int file_count = 0;
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] != '.') {
                    file_count++;
                    break; // At least one file exists
                }
            }
            closedir(dir);
            if (file_count > 0) {
                log_debug("Extraction successful (xz format)");
                unlink(error_file); // Remove error log on success
                return true;
            }
        }
    }

    // Try gzip compression
    snprintf(cmd, sizeof(cmd), "tar -xzf '%s' -C '%s' 2>'%s'", archive, dest, error_file);
    result = system(cmd);
    if (result == 0) {
        DIR *dir = opendir(dest);
        if (dir) {
            int file_count = 0;
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] != '.') {
                    file_count++;
                    break;
                }
            }
            closedir(dir);
            if (file_count > 0) {
                log_debug("Extraction successful (gzip format)");
                unlink(error_file);
                return true;
            }
        }
    }

    // Try bzip2 compression
    snprintf(cmd, sizeof(cmd), "tar -xjf '%s' -C '%s' 2>'%s'", archive, dest, error_file);
    result = system(cmd);
    if (result == 0) {
        DIR *dir = opendir(dest);
        if (dir) {
            int file_count = 0;
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] != '.') {
                    file_count++;
                    break;
                }
            }
            closedir(dir);
            if (file_count > 0) {
                log_debug("Extraction successful (bzip2 format)");
                unlink(error_file);
                return true;
            }
        }
    }

    // Try uncompressed
    snprintf(cmd, sizeof(cmd), "tar -xf '%s' -C '%s' 2>'%s'", archive, dest, error_file);
    result = system(cmd);
    if (result == 0) {
        DIR *dir = opendir(dest);
        if (dir) {
            int file_count = 0;
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] != '.') {
                    file_count++;
                    break;
                }
            }
            closedir(dir);
            if (file_count > 0) {
                log_debug("Extraction successful (uncompressed format)");
                unlink(error_file);
                return true;
            }
        }
    }

    // All extraction attempts failed - show error message
    log_error("Failed to extract archive: %s", archive);
    FILE *fp = fopen(error_file, "r");
    if (fp) {
        char line[256];
        log_error("Extraction error details:");
        while (fgets(line, sizeof(line), fp)) {
            // Remove trailing newline
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') {
                line[len-1] = '\0';
            }
            if (strlen(line) > 0) {
                log_error("  %s", line);
            }
        }
        fclose(fp);
        unlink(error_file);
    }

    // Verify the archive is actually a valid tar file
    log_error("Archive may be corrupted or in an unsupported format");
    log_error("Please verify the download completed successfully");

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
        log_error("fetcher_fetch called with invalid parameters");
        return NULL;
    }

    log_debug("Fetching package: %s@%s (source_type=%s, force=%s)",
              pkg->name, pkg->version ? pkg->version : "latest",
              pkg->source_type, force ? "true" : "false");

    // Use version-specific directory if version is specified
    char package_dir[512];
    if (pkg->version && strcmp(pkg->version, "latest") != 0) {
        snprintf(package_dir, sizeof(package_dir), "%s/%s-%s", fetcher->source_dir, pkg->name, pkg->version);
    } else {
        snprintf(package_dir, sizeof(package_dir), "%s/%s", fetcher->source_dir, pkg->name);
    }
    log_developer("Package directory: %s", package_dir);

    // Check if already exists
    struct stat st;
    if (stat(package_dir, &st) == 0 && !force) {
        log_debug("Package source already exists (skipping fetch): %s", package_dir);
        return strdup(package_dir);
    }

    // Remove existing if force
    if (force && stat(package_dir, &st) == 0) {
        log_debug("Force mode: removing existing source directory: %s", package_dir);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", package_dir);
        system(cmd);
    }

    if (strcmp(pkg->source_type, "git") == 0) {
        if (!pkg->source_url) {
            log_error("Git source type specified but source_url is NULL for package: %s", pkg->name);
            return NULL;
        }

        log_info("Cloning git repository: %s -> %s", pkg->source_url, package_dir);
        log_developer("Git parameters: branch=%s, tag=%s, commit=%s",
                      pkg->source_branch ? pkg->source_branch : "NULL",
                      pkg->source_tag ? pkg->source_tag : "NULL",
                      pkg->source_commit ? pkg->source_commit : "NULL");
        if (fetcher_clone_git(pkg->source_url, package_dir, pkg->source_branch, pkg->source_tag, pkg->source_commit)) {
            log_info("Successfully cloned git repository: %s", package_dir);
            return strdup(package_dir);
        }
        log_error("Failed to clone git repository: %s", pkg->source_url);
    } else if (strcmp(pkg->source_type, "tarball") == 0 || strcmp(pkg->source_type, "zip") == 0) {
        if (!pkg->source_url) {
            log_error("Tarball/zip source type specified but source_url is NULL for package: %s", pkg->name);
            return NULL;
        }

        // Download archive
        char archive[1024];
        snprintf(archive, sizeof(archive), "%s/%s", fetcher->source_dir, strrchr(pkg->source_url, '/') ? strrchr(pkg->source_url, '/') + 1 : "archive");
        log_info("Downloading %s archive: %s -> %s", pkg->source_type, pkg->source_url, archive);

        if (!fetcher_download_file(pkg->source_url, archive)) {
            log_error("Failed to download archive for package: %s", pkg->name);
            return NULL;
        }

        // Create destination directory
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s'", package_dir);
        system(cmd);

        // Extract
        log_debug("Extracting archive: %s -> %s", archive, package_dir);
        if (fetcher_extract_tarball(archive, package_dir)) {
            log_info("Successfully extracted archive: %s", package_dir);
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
        } else {
            log_error("Failed to extract archive: %s", archive);
            // Clean up empty directory created for extraction
            char cmd[1024];
            snprintf(cmd, sizeof(cmd), "rmdir '%s' 2>/dev/null", package_dir);
            system(cmd);
            return NULL;
        }
    } else if (strcmp(pkg->source_type, "local") == 0) {
        if (!pkg->source_url) {
            log_error("Local source type specified but source_url is NULL for package: %s", pkg->name);
            return NULL;
        }

        log_info("Copying local source: %s -> %s", pkg->source_url, package_dir);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "cp -r '%s' '%s' 2>/dev/null", pkg->source_url, package_dir);
        if (system(cmd) == 0) {
            struct stat st;
            if (stat(package_dir, &st) == 0) {
                log_info("Successfully copied local source: %s", package_dir);
                return strdup(package_dir);
            }
        }
        log_error("Failed to copy local source: %s -> %s", pkg->source_url, package_dir);
    } else {
        log_error("Unknown source type: %s for package: %s", pkg->source_type, pkg->name);
    }

    log_error("Failed to fetch package: %s@%s", pkg->name, pkg->version ? pkg->version : "latest");
    return NULL;
}

