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

// Print "Building" message (Homebrew style: "==> Building package")
static inline void print_building(const char *package, const char *version) {
    if (version) {
        printf("==> Building %s %s\n", package, version);
    } else {
        printf("==> Building %s\n", package);
    }
}

// Print "Installing" message (Homebrew style: "==> Installing package")
static inline void print_installing(const char *package, const char *version) {
    if (version) {
        printf("==> Installing %s %s\n", package, version);
    } else {
        printf("==> Installing %s\n", package);
    }
}

// Print installation summary (Homebrew style: "/path/to/package/version (X files, Y MB)")
static inline void print_summary(const char *install_path, int file_count, const char *size) {
    if (file_count > 0 && size) {
        printf("==> Summary\n");
        printf("  %s (%d files, %s)\n", install_path, file_count, size);
    } else if (install_path) {
        printf("==> Summary\n");
        printf("  %s\n", install_path);
    }
}

// Print caveats section (Homebrew style: "==> Caveats")
static inline void print_caveats_start(void) {
    printf("==> Caveats\n");
}

// Print a caveat line
static inline void print_caveat(const char *caveat) {
    printf("  %s\n", caveat);
}

// Print cleanup message (Homebrew style: "==> Cleaning up")
static inline void print_cleanup(const char *package, const char *old_version, int file_count, const char *size) {
    printf("==> Cleaning up %s\n", package);
    if (old_version) {
        if (file_count > 0 && size) {
            printf("  Removed %s (%d files, %s)\n", old_version, file_count, size);
        } else {
            printf("  Removed %s\n", old_version);
        }
    }
}

// Terminal control sequences
#define CLEAR_LINE "\033[2K"  // Clear entire line
#define HIDE_CURSOR "\033[?25l"  // Hide cursor
#define SHOW_CURSOR "\033[?25h"  // Show cursor

// Print status that updates in place (single line)
static inline void print_status_inline(const char *status) {
    if (is_tty()) {
        printf("\r\033[2K%s", status);
        fflush(stdout);
    } else {
        printf("%s\n", status);
    }
}

// Print compact building status (prints on new line)
static inline void print_building_compact(const char *package, const char *version) {
    if (version) {
        printf("==> Building %s %s\n", package, version);
    } else {
        printf("==> Building %s\n", package);
    }
    fflush(stdout);
}

// Print compact installing status (prints on new line)
static inline void print_installing_compact(const char *package, const char *version) {
    if (version) {
        printf("==> Installing %s %s\n", package, version);
    } else {
        printf("==> Installing %s\n", package);
    }
    fflush(stdout);
}

// Print final status (new line after inline update)
static inline void print_status_done(const char *status) {
    if (is_tty()) {
        printf("\r%s%s%s\n", CLEAR_LINE, status, COLOR_RESET);
    } else {
        printf("%s\n", status);
    }
}

// Rolling output buffer for showing last N lines
#define OUTPUT_BUFFER_LINES 5
#define OUTPUT_LINE_LENGTH 256

typedef struct {
    char lines[OUTPUT_BUFFER_LINES][OUTPUT_LINE_LENGTH];
    int line_count;
    int current_index;
} OutputBuffer;

// Initialize output buffer
static inline void output_buffer_init(OutputBuffer *buf) {
    buf->line_count = 0;
    buf->current_index = 0;
    for (int i = 0; i < OUTPUT_BUFFER_LINES; i++) {
        buf->lines[i][0] = '\0';
    }
}

// Add a line to the buffer (rolling)
static inline void output_buffer_add(OutputBuffer *buf, const char *line) {
    if (!buf || !line) return;

    // Truncate line if too long
    size_t len = strlen(line);
    if (len > OUTPUT_LINE_LENGTH - 1) {
        len = OUTPUT_LINE_LENGTH - 1;
    }

    // Copy line (handle newline)
    strncpy(buf->lines[buf->current_index], line, len);
    // Remove trailing newline if present
    if (buf->lines[buf->current_index][len - 1] == '\n') {
        buf->lines[buf->current_index][len - 1] = '\0';
    } else {
        buf->lines[buf->current_index][len] = '\0';
    }

    buf->current_index = (buf->current_index + 1) % OUTPUT_BUFFER_LINES;
    if (buf->line_count < OUTPUT_BUFFER_LINES) {
        buf->line_count++;
    }
}

// Display the output buffer (moves cursor up and redraws, preserves status line above)
static inline void output_buffer_display(OutputBuffer *buf) {
    if (!buf || !is_tty()) return;

    // Move cursor up to overwrite previous output (but not the status line)
    if (buf->line_count > 0) {
        printf("\033[%dA", buf->line_count);  // Move up N lines
    }

    // Display each line, clearing it first and truncating long lines
    for (int i = 0; i < buf->line_count; i++) {
        int idx = (buf->current_index - buf->line_count + i + OUTPUT_BUFFER_LINES) % OUTPUT_BUFFER_LINES;
        // Truncate line to reasonable width (120 chars)
        char display_line[130];
        size_t len = strlen(buf->lines[idx]);
        if (len > 120) {
            strncpy(display_line, buf->lines[idx], 117);
            display_line[117] = '.';
            display_line[118] = '.';
            display_line[119] = '.';
            display_line[120] = '\0';
        } else {
            strncpy(display_line, buf->lines[idx], sizeof(display_line) - 1);
            display_line[sizeof(display_line) - 1] = '\0';
        }
        printf("\r\033[2K  %s\n", display_line);  // Add indentation to distinguish from status
    }

    fflush(stdout);
}

// Start output capture area (after status line)
static inline void output_capture_start(void) {
    if (is_tty()) {
        printf("\n");  // Start on new line after status
    }
}

// End output capture area (clear output lines, keep status line)
static inline void output_capture_end(OutputBuffer *buf) {
    if (!buf || !is_tty()) return;

    // Clear the output area but preserve status line
    if (buf->line_count > 0) {
        // Move up to the output area
        printf("\033[%dA", buf->line_count);  // Move up
        // Clear each output line
        for (int i = 0; i < buf->line_count; i++) {
            printf("\r\033[2K\n");  // Clear each line
        }
        // Move back to after status line
        printf("\033[%dA", buf->line_count);  // Move back up to where we started
    }
}

#ifdef __cplusplus
}
#endif

#endif // TUI_H

