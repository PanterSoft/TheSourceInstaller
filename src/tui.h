#ifndef TUI_H
#define TUI_H

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Check if stdout is a TTY (terminal)
static inline bool is_tty(void) {
    return isatty(fileno(stdout));
}

// Colors (ANSI escape codes) - Homebrew style (subtle)
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

// Icons - Homebrew style
#define ICON_SUCCESS "✓"
#define ICON_ERROR   "✗"
#define ICON_WARNING "⚠"
#define ICON_IN_PROGRESS "*"
#define ICON_ARROW   "→"

// Homebrew-style section header
static inline void print_section(const char *title) {
    printf("==> %s\n", title);
}

// Homebrew-style success message
static inline void print_success(const char *msg) {
    if (is_tty()) {
        printf("%s%s%s %s\n", COLOR_GREEN, ICON_SUCCESS, COLOR_RESET, msg);
    } else {
        printf("%s %s\n", ICON_SUCCESS, msg);
    }
}

// Homebrew-style error message
static inline void print_error(const char *msg) {
    if (is_tty()) {
        fprintf(stderr, "%s%s%s %s\n", COLOR_RED, ICON_ERROR, COLOR_RESET, msg);
    } else {
        fprintf(stderr, "%s %s\n", ICON_ERROR, msg);
    }
}

// Homebrew-style warning message
static inline void print_warning(const char *msg) {
    if (is_tty()) {
        printf("%s%s%s %s\n", COLOR_YELLOW, ICON_WARNING, COLOR_RESET, msg);
    } else {
        printf("%s %s\n", ICON_WARNING, msg);
    }
}

// Homebrew-style info message
static inline void print_info(const char *msg) {
    printf("%s\n", msg);
}

// Print package with version (Homebrew style: "package version")
static inline void print_package(const char *name, const char *version) {
    if (version) {
        printf("  %s %s\n", name, version);
    } else {
        printf("  %s\n", name);
    }
}

// Print package with version change (Homebrew style: "package old -> new")
static inline void print_package_update(const char *name, const char *old_version, const char *new_version) {
    if (old_version && new_version) {
        printf("  %s %s -> %s\n", name, old_version, new_version);
    } else if (new_version) {
        printf("  %s -> %s\n", name, new_version);
    } else {
        printf("  %s\n", name);
    }
}

// Print download progress (Homebrew style: "✓ Package (version): [Downloaded X MB/Y MB]")
static inline void print_download(const char *icon, const char *package, const char *version,
                                  const char *size_info) {
    (void)icon; // For future use
    if (version) {
        printf("%s %s (%s): %s\n", icon, package, version, size_info);
    } else {
        printf("%s %s: %s\n", icon, package, size_info);
    }
}

// Print in-progress operation (Homebrew style: "* Operation: detail")
static inline void print_progress(const char *operation, const char *detail) {
    if (is_tty()) {
        printf("%s%s%s %s", COLOR_BLUE, ICON_IN_PROGRESS, COLOR_RESET, operation);
        if (detail) {
            printf(": %s", detail);
        }
        printf("\n");
    } else {
        printf("%s %s", ICON_IN_PROGRESS, operation);
        if (detail) {
            printf(": %s", detail);
        }
        printf("\n");
    }
}

#ifdef __cplusplus
}
#endif

#endif // TUI_H

