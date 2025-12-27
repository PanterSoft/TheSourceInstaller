#include "fetcher.h"
#include "package.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// Helper function to get TSI installation prefix
// Similar to get_tsi_prefix() in main.c but accessible from fetcher
static const char *get_tsi_prefix_for_fetcher(void) {
    static char prefix[1024] = {0};
    static bool initialized = false;

    if (initialized) {
        return prefix[0] ? prefix : NULL;
    }
    initialized = true;

    // Try to get from environment variable first
    const char *env_prefix = getenv("TSI_PREFIX");
    if (env_prefix && env_prefix[0]) {
        strncpy(prefix, env_prefix, sizeof(prefix) - 1);
        prefix[sizeof(prefix) - 1] = '\0';
        return prefix;
    }

    // Try to detect from binary location
    char exe_path[1024] = {0};
    ssize_t len = 0;

#ifdef __APPLE__
    uint32_t size = sizeof(exe_path);
    if (_NSGetExecutablePath(exe_path, &size) == 0) {
        len = strlen(exe_path);
    }
#else
    len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
#endif

    if (len > 0 && len < (ssize_t)sizeof(exe_path)) {
        exe_path[len] = '\0';
        char *bin_pos = strstr(exe_path, "/bin/tsi");
        if (bin_pos) {
            size_t prefix_len = bin_pos - exe_path;
            if (prefix_len > 0 && prefix_len < sizeof(prefix)) {
                strncpy(prefix, exe_path, prefix_len);
                prefix[prefix_len] = '\0';
                return prefix;
            }
        }
    }

    // Fallback to common locations
    const char *home = getenv("HOME");
    struct stat st;
    if (home) {
        snprintf(prefix, sizeof(prefix), "%s/.tsi", home);
        if (stat(prefix, &st) == 0) {
            return prefix;
        }
    }

    // Try /opt/tsi
    if (stat("/opt/tsi", &st) == 0) {
        strncpy(prefix, "/opt/tsi", sizeof(prefix) - 1);
        return prefix;
    }

    return NULL;
}

// Helper function to find a tool, preferring TSI-installed version
static char *find_tool(const char *tool_name) {
    static char tool_path[1024];

    // First, try TSI-installed version
    const char *tsi_prefix = get_tsi_prefix_for_fetcher();
    if (tsi_prefix) {
        snprintf(tool_path, sizeof(tool_path), "%s/bin/%s", tsi_prefix, tool_name);
        struct stat st;
        if (stat(tool_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
            log_debug("Using TSI-installed %s: %s", tool_name, tool_path);
            return tool_path;
        }
    }

    // Fall back to system tool (will be found via PATH)
    return (char *)tool_name;
}

// Check if a tool is available (either TSI-installed or in system PATH)
static bool tool_available(const char *tool_name) {
    // First check TSI-installed version
    const char *tsi_prefix = get_tsi_prefix_for_fetcher();
    if (tsi_prefix) {
        char tool_path[1024];
        snprintf(tool_path, sizeof(tool_path), "%s/bin/%s", tsi_prefix, tool_name);
        struct stat st;
        if (stat(tool_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
            return true;
        }
    }

    // Check system PATH using command -v
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", tool_name);
    return system(cmd) == 0;
}

// Check if wget is BusyBox version (doesn't support --progress option)
// Uses cached result to avoid repeated checks
static bool is_busybox_wget(const char *wget_path) {
    static bool cached_result = false;
    static bool has_cache = false;
    static char cached_path[1024] = {0};

    if (!wget_path) return false;

    // Check cache first
    if (has_cache && strcmp(cached_path, wget_path) == 0) {
        return cached_result;
    }

    // Check version output for "BusyBox" string (fastest method)
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s --version 2>&1 | head -1 | grep -q 'BusyBox'", wget_path);
    bool is_busybox = (system(cmd) == 0);

    // If version check didn't work, try help output
    if (!is_busybox) {
        snprintf(cmd, sizeof(cmd), "%s --help 2>&1 | head -1 | grep -q 'BusyBox'", wget_path);
        is_busybox = (system(cmd) == 0);
    }

    // Cache the result
    strncpy(cached_path, wget_path, sizeof(cached_path) - 1);
    cached_path[sizeof(cached_path) - 1] = '\0';
    cached_result = is_busybox;
    has_cache = true;

    if (is_busybox) {
        log_debug("Detected BusyBox wget: %s", wget_path);
    }

    return is_busybox;
}

// Download tool preference enum
typedef enum {
    DOWNLOAD_TOOL_NONE = 0,
    DOWNLOAD_TOOL_WGET,
    DOWNLOAD_TOOL_CURL
} DownloadTool;

// Detect which download tool is available, preferring TSI-installed versions
static DownloadTool detect_download_tool(void) {
    // Prefer wget if available (better progress bar support)
    if (tool_available("wget")) {
        log_debug("Detected wget as download tool");
        return DOWNLOAD_TOOL_WGET;
    }

    // Fall back to curl
    if (tool_available("curl")) {
        log_debug("Detected curl as download tool");
        return DOWNLOAD_TOOL_CURL;
    }

    log_debug("No download tool available (wget or curl)");
    return DOWNLOAD_TOOL_NONE;
}

// Archive format types
typedef enum {
    ARCHIVE_FORMAT_UNKNOWN = 0,
    ARCHIVE_FORMAT_XZ,      // .tar.xz
    ARCHIVE_FORMAT_GZIP,    // .tar.gz, .tgz
    ARCHIVE_FORMAT_BZIP2,   // .tar.bz2, .tbz2
    ARCHIVE_FORMAT_TAR      // .tar (uncompressed)
} ArchiveFormat;

// Detect archive format by file extension and magic bytes
static ArchiveFormat detect_archive_format(const char *archive) {
    if (!archive) return ARCHIVE_FORMAT_UNKNOWN;

    // First, try to detect by file extension (fastest method)
    const char *ext = strrchr(archive, '.');
    if (ext) {
        // Check for .tar.xz, .tar.gz, .tar.bz2 first (multi-extension)
        if (strstr(archive, ".tar.xz") || strstr(archive, ".txz")) {
            return ARCHIVE_FORMAT_XZ;
        }
        if (strstr(archive, ".tar.gz") || strcmp(ext, ".tgz") == 0) {
            return ARCHIVE_FORMAT_GZIP;
        }
        if (strstr(archive, ".tar.bz2") || strcmp(ext, ".tbz2") == 0 || strcmp(ext, ".tbz") == 0) {
            return ARCHIVE_FORMAT_BZIP2;
        }
        if (strcmp(ext, ".tar") == 0) {
            return ARCHIVE_FORMAT_TAR;
        }
        // Check single extensions
        if (strcmp(ext, ".xz") == 0) {
            return ARCHIVE_FORMAT_XZ;
        }
        if (strcmp(ext, ".gz") == 0) {
            return ARCHIVE_FORMAT_GZIP;
        }
        if (strcmp(ext, ".bz2") == 0 || strcmp(ext, ".bz") == 0) {
            return ARCHIVE_FORMAT_BZIP2;
        }
    }

    // If extension detection failed, check magic bytes (file signatures)
    FILE *fp = fopen(archive, "rb");
    if (!fp) {
        log_debug("Cannot open archive for magic byte detection: %s", archive);
        return ARCHIVE_FORMAT_UNKNOWN;
    }

    unsigned char magic[6];
    size_t read = fread(magic, 1, sizeof(magic), fp);
    fclose(fp);

    if (read < 2) {
        return ARCHIVE_FORMAT_UNKNOWN;
    }

    // Check magic bytes
    // xz: 0xfd 0x37 0x7a 0x58 0x5a 0x00
    if (read >= 6 && magic[0] == 0xfd && magic[1] == 0x37 &&
        magic[2] == 0x7a && magic[3] == 0x58 && magic[4] == 0x5a && magic[5] == 0x00) {
        log_debug("Detected xz format by magic bytes");
        return ARCHIVE_FORMAT_XZ;
    }

    // gzip: 0x1f 0x8b
    if (magic[0] == 0x1f && magic[1] == 0x8b) {
        log_debug("Detected gzip format by magic bytes");
        return ARCHIVE_FORMAT_GZIP;
    }

    // bzip2: "BZ" (0x42 0x5a)
    if (read >= 2 && magic[0] == 0x42 && magic[1] == 0x5a) {
        log_debug("Detected bzip2 format by magic bytes");
        return ARCHIVE_FORMAT_BZIP2;
    }

    // tar: Check for "ustar" at offset 257 (standard tar header)
    // For simplicity, if it's not compressed and has a .tar extension or no known compression magic, assume tar
    if (read >= 2) {
        // Try to read at offset 257 for ustar magic
        fp = fopen(archive, "rb");
        if (fp) {
            fseek(fp, 257, SEEK_SET);
            char ustar[6] = {0};
            if (fread(ustar, 1, 5, fp) == 5 && strncmp(ustar, "ustar", 5) == 0) {
                fclose(fp);
                log_debug("Detected tar format by ustar magic");
                return ARCHIVE_FORMAT_TAR;
            }
            fclose(fp);
        }
    }

    log_debug("Could not detect archive format for: %s", archive);
    return ARCHIVE_FORMAT_UNKNOWN;
}

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

    // Detect which download tool is available
    DownloadTool tool = detect_download_tool();
    if (tool == DOWNLOAD_TOOL_NONE) {
        log_error("No download tool available (wget or curl required)");
        return false;
    }

    // Check if stdout is a TTY to determine if we should show progress
    bool show_progress = isatty(STDOUT_FILENO);

    char cmd[1024];
    char *tool_path;
    const char *tool_name;

    // Select tool and build command
    if (tool == DOWNLOAD_TOOL_WGET) {
        tool_path = find_tool("wget");
        tool_name = "wget";

        // Check if this is BusyBox wget (doesn't support --progress)
        bool busybox_wget = is_busybox_wget(tool_path);

        if (show_progress && !busybox_wget) {
            // GNU wget with progress bar
            snprintf(cmd, sizeof(cmd), "%s --progress=bar:force -O '%s' '%s' 2>&1", tool_path, dest, url);
        } else if (show_progress && busybox_wget) {
            // BusyBox wget - use verbose mode instead (shows progress)
            snprintf(cmd, sizeof(cmd), "%s -O '%s' '%s' 2>&1", tool_path, dest, url);
        } else {
            // wget quiet mode
            snprintf(cmd, sizeof(cmd), "%s -q -O '%s' '%s' 2>/dev/null", tool_path, dest, url);
        }
    } else { // DOWNLOAD_TOOL_CURL
        tool_path = find_tool("curl");
        tool_name = "curl";
        if (show_progress) {
            // curl with progress bar (# shows progress)
            snprintf(cmd, sizeof(cmd), "%s -# -fSL -o '%s' '%s' 2>&1", tool_path, dest, url);
        } else {
            // curl quiet mode
            snprintf(cmd, sizeof(cmd), "%s -fsSL -o '%s' '%s' 2>/dev/null", tool_path, dest, url);
        }
    }

    log_debug("Using %s to download: %s", tool_name, url);

    // Execute download
    int result = system(cmd);
    if (result == 0) {
        struct stat st;
        if (stat(dest, &st) == 0 && st.st_size > 0) {
            log_info("File downloaded successfully using %s: %s (%ld bytes)", tool_name, dest, (long)st.st_size);
            return true;
        } else {
            log_error("Download completed but file is empty or missing: %s", dest);
            return false;
        }
    } else {
        log_error("Download failed using %s (exit code: %d)", tool_name, result);
        return false;
    }
}

// Helper function to verify extraction succeeded
static bool verify_extraction(const char *dest) {
    DIR *dir = opendir(dest);
    if (!dir) return false;

    int file_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            file_count++;
            break; // At least one file exists
        }
    }
    closedir(dir);
    return file_count > 0;
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

    // Detect archive format
    ArchiveFormat format = detect_archive_format(archive);

    char cmd[1024];
    int result;
    char error_file[512];
    snprintf(error_file, sizeof(error_file), "%s/tar_error.log", dest);
    const char *format_name = "unknown";

    // Find TSI-installed tools, preferring them over system tools
    char *tar_path = find_tool("tar");
    char *gzip_path = find_tool("gzip");
    char *xz_path = find_tool("xz");

    // Extract using the detected format
    switch (format) {
        case ARCHIVE_FORMAT_XZ:
            format_name = "xz";
            log_info("Detected xz compression format, extracting with tar -xJf");
            // For xz, we need to use tar with -J flag, or decompress with xz first then tar
            // Try tar -xJf first (GNU tar supports this)
            snprintf(cmd, sizeof(cmd), "%s -xJf '%s' -C '%s' 2>'%s'", tar_path, archive, dest, error_file);
            result = system(cmd);
            if (result != 0) {
                // Fallback: decompress with xz first, then extract with tar
                log_debug("tar -xJf failed, trying xz decompression + tar extraction");
                char temp_tar[1024];
                snprintf(temp_tar, sizeof(temp_tar), "%s/temp.tar", dest);
                char xz_cmd[1024];
                snprintf(xz_cmd, sizeof(xz_cmd), "%s -dc '%s' > '%s' 2>'%s'", xz_path, archive, temp_tar, error_file);
                if (system(xz_cmd) == 0) {
                    snprintf(cmd, sizeof(cmd), "%s -xf '%s' -C '%s' 2>>'%s'", tar_path, temp_tar, dest, error_file);
                    result = system(cmd);
                    unlink(temp_tar); // Clean up temp file
                }
            }
            break;

        case ARCHIVE_FORMAT_GZIP:
            format_name = "gzip";
            log_info("Detected gzip compression format, extracting with tar -xzf");
            // Try tar -xzf first
            snprintf(cmd, sizeof(cmd), "%s -xzf '%s' -C '%s' 2>'%s'", tar_path, archive, dest, error_file);
            result = system(cmd);
            if (result != 0) {
                // Fallback: decompress with gzip first, then extract with tar
                log_debug("tar -xzf failed, trying gzip decompression + tar extraction");
                char temp_tar[1024];
                snprintf(temp_tar, sizeof(temp_tar), "%s/temp.tar", dest);
                char gzip_cmd[1024];
                snprintf(gzip_cmd, sizeof(gzip_cmd), "%s -dc '%s' > '%s' 2>'%s'", gzip_path, archive, temp_tar, error_file);
                if (system(gzip_cmd) == 0) {
                    snprintf(cmd, sizeof(cmd), "%s -xf '%s' -C '%s' 2>>'%s'", tar_path, temp_tar, dest, error_file);
                    result = system(cmd);
                    unlink(temp_tar); // Clean up temp file
                }
            }
            break;

        case ARCHIVE_FORMAT_BZIP2:
            format_name = "bzip2";
            log_info("Detected bzip2 compression format, extracting with tar -xjf");
            snprintf(cmd, sizeof(cmd), "%s -xjf '%s' -C '%s' 2>'%s'", tar_path, archive, dest, error_file);
            result = system(cmd);
            break;

        case ARCHIVE_FORMAT_TAR:
            format_name = "tar (uncompressed)";
            log_info("Detected uncompressed tar format, extracting with tar -xf");
            snprintf(cmd, sizeof(cmd), "%s -xf '%s' -C '%s' 2>'%s'", tar_path, archive, dest, error_file);
            result = system(cmd);
            break;

        case ARCHIVE_FORMAT_UNKNOWN:
        default:
            log_warning("Could not detect archive format, trying all formats in order");
            format_name = "unknown";
            // Fall through to try-all logic below
            result = -1;
            break;
    }

    // If format was detected and extraction succeeded, verify it
    if (format != ARCHIVE_FORMAT_UNKNOWN && result == 0) {
        if (verify_extraction(dest)) {
            log_info("Extraction successful (%s format)", format_name);
            unlink(error_file);
            return true;
        } else {
            log_warning("Extraction command succeeded but no files were extracted, trying other formats");
        }
    }

    // If detection failed or extraction failed, try all formats as fallback
    if (format == ARCHIVE_FORMAT_UNKNOWN || result != 0) {
        log_debug("Trying xz format as fallback");
        snprintf(cmd, sizeof(cmd), "%s -xJf '%s' -C '%s' 2>'%s'", tar_path, archive, dest, error_file);
        result = system(cmd);
        if (result != 0) {
            // Try xz decompression + tar
            char temp_tar[1024];
            snprintf(temp_tar, sizeof(temp_tar), "%s/temp.tar", dest);
            char xz_cmd[1024];
            snprintf(xz_cmd, sizeof(xz_cmd), "%s -dc '%s' > '%s' 2>'%s'", xz_path, archive, temp_tar, error_file);
            if (system(xz_cmd) == 0) {
                snprintf(cmd, sizeof(cmd), "%s -xf '%s' -C '%s' 2>>'%s'", tar_path, temp_tar, dest, error_file);
                result = system(cmd);
                unlink(temp_tar);
            }
        }
        if (result == 0 && verify_extraction(dest)) {
            log_info("Extraction successful (xz format, fallback)");
            unlink(error_file);
            return true;
        }

        log_debug("Trying gzip format as fallback");
        snprintf(cmd, sizeof(cmd), "%s -xzf '%s' -C '%s' 2>'%s'", tar_path, archive, dest, error_file);
        result = system(cmd);
        if (result != 0) {
            // Try gzip decompression + tar
            char temp_tar[1024];
            snprintf(temp_tar, sizeof(temp_tar), "%s/temp.tar", dest);
            char gzip_cmd[1024];
            snprintf(gzip_cmd, sizeof(gzip_cmd), "%s -dc '%s' > '%s' 2>'%s'", gzip_path, archive, temp_tar, error_file);
            if (system(gzip_cmd) == 0) {
                snprintf(cmd, sizeof(cmd), "%s -xf '%s' -C '%s' 2>>'%s'", tar_path, temp_tar, dest, error_file);
                result = system(cmd);
                unlink(temp_tar);
            }
        }
        if (result == 0 && verify_extraction(dest)) {
            log_info("Extraction successful (gzip format, fallback)");
            unlink(error_file);
            return true;
        }

        log_debug("Trying bzip2 format as fallback");
        snprintf(cmd, sizeof(cmd), "%s -xjf '%s' -C '%s' 2>'%s'", tar_path, archive, dest, error_file);
        result = system(cmd);
        if (result == 0 && verify_extraction(dest)) {
            log_info("Extraction successful (bzip2 format, fallback)");
            unlink(error_file);
            return true;
        }

        log_debug("Trying uncompressed tar format as fallback");
        snprintf(cmd, sizeof(cmd), "%s -xf '%s' -C '%s' 2>'%s'", tar_path, archive, dest, error_file);
        result = system(cmd);
        if (result == 0 && verify_extraction(dest)) {
            log_info("Extraction successful (uncompressed tar format, fallback)");
            unlink(error_file);
            return true;
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

