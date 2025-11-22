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

// Colors (ANSI escape codes)
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

// Icons/Emojis
#define ICON_SUCCESS "✓"
#define ICON_ERROR   "✗"
#define ICON_WARNING "⚠"
#define ICON_INFO    "ℹ"
#define ICON_ARROW   "→"
#define ICON_PACKAGE "📦"
#define ICON_DOWNLOAD "⬇"
#define ICON_BUILD   "🔨"
#define ICON_INSTALL "📥"

// Pretty print functions
static inline void print_success(const char *msg) {
    if (is_tty()) {
        printf("%s%s%s %s%s%s\n", COLOR_GREEN, ICON_SUCCESS, COLOR_RESET, COLOR_BOLD, msg, COLOR_RESET);
    } else {
        printf("✓ %s\n", msg);
    }
}

static inline void print_error(const char *msg) {
    if (is_tty()) {
        fprintf(stderr, "%s%s%s %s%s%s\n", COLOR_RED, ICON_ERROR, COLOR_RESET, COLOR_BOLD, msg, COLOR_RESET);
    } else {
        fprintf(stderr, "✗ %s\n", msg);
    }
}

static inline void print_warning(const char *msg) {
    if (is_tty()) {
        printf("%s%s%s %s%s%s\n", COLOR_YELLOW, ICON_WARNING, COLOR_RESET, COLOR_BOLD, msg, COLOR_RESET);
    } else {
        printf("⚠ %s\n", msg);
    }
}

static inline void print_info(const char *msg) {
    if (is_tty()) {
        printf("%s%s%s %s\n", COLOR_CYAN, ICON_INFO, COLOR_RESET, msg);
    } else {
        printf("ℹ %s\n", msg);
    }
}

static inline void print_section(const char *title) {
    if (is_tty()) {
        printf("\n%s%s%s\n", COLOR_BOLD, title, COLOR_RESET);
    } else {
        printf("\n%s\n", title);
    }
}

static inline void print_header(const char *title) {
    if (is_tty()) {
        printf("\n%s═══════════════════════════════════════════════════════════%s\n", COLOR_BOLD, COLOR_RESET);
        printf("%s%s%s\n", COLOR_BOLD, title, COLOR_RESET);
        printf("%s═══════════════════════════════════════════════════════════%s\n\n", COLOR_BOLD, COLOR_RESET);
    } else {
        printf("\n═══════════════════════════════════════════════════════════\n");
        printf("%s\n", title);
        printf("═══════════════════════════════════════════════════════════\n\n");
    }
}

static inline void print_package_name(const char *name, const char *version) {
    if (is_tty()) {
        printf("%s%s%s", COLOR_BOLD, name, COLOR_RESET);
        if (version) {
            printf("%s@%s%s%s", COLOR_CYAN, COLOR_BOLD, version, COLOR_RESET);
        }
    } else {
        printf("%s", name);
        if (version) {
            printf("@%s", version);
        }
    }
}

static inline void print_step(const char *icon, const char *step, const char *detail) {
    if (is_tty()) {
        printf("  %s %s%s%s", icon, COLOR_BOLD, step, COLOR_RESET);
        if (detail) {
            printf(" %s%s%s", COLOR_CYAN, detail, COLOR_RESET);
        }
        printf("\n");
    } else {
        printf("  %s %s", icon, step);
        if (detail) {
            printf(" %s", detail);
        }
        printf("\n");
    }
}

#ifdef __cplusplus
}
#endif

#endif // TUI_H

