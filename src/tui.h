#ifndef TUI_H
#define TUI_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Terminal capability detection
static inline bool is_tty(void) {
    return isatty(fileno(stdout));
}

// Check if terminal supports colors (works on bash, zsh, and most serial consoles)
static inline bool supports_colors(void) {
    if (!is_tty()) return false;

    const char *term = getenv("TERM");
    if (!term) return true; // Assume colors if TERM not set (common on serial consoles)

    // Serial console terminals that typically support basic ANSI
    if (strcmp(term, "linux") == 0 || strcmp(term, "vt100") == 0 ||
        strcmp(term, "vt102") == 0 || strcmp(term, "xterm") == 0 ||
        strcmp(term, "xterm-256color") == 0 || strcmp(term, "screen") == 0 ||
        strcmp(term, "tmux") == 0 || strncmp(term, "xterm", 5) == 0 ||
        strncmp(term, "vt", 2) == 0) {
        return true;
    }

    // Check for NO_COLOR environment variable (standard way to disable colors)
    if (getenv("NO_COLOR")) return false;

    // Default to true for most terminals (basic ANSI is widely supported)
    return true;
}

// Vibrant Colors (ANSI escape codes) - Works on bash, zsh, and serial consoles
#define COLOR_RESET        "\033[0m"
#define COLOR_BOLD         "\033[1m"
#define COLOR_DIM          "\033[2m"
#define COLOR_ITALIC       "\033[3m"
#define COLOR_UNDERLINE    "\033[4m"

// Standard colors
#define COLOR_BLACK        "\033[30m"
#define COLOR_RED          "\033[31m"
#define COLOR_GREEN        "\033[32m"
#define COLOR_YELLOW       "\033[33m"
#define COLOR_BLUE         "\033[34m"
#define COLOR_MAGENTA      "\033[35m"
#define COLOR_CYAN         "\033[36m"
#define COLOR_WHITE        "\033[37m"

// Bright colors (works on most terminals including serial consoles)
#define COLOR_BRIGHT_BLACK   "\033[90m"
#define COLOR_BRIGHT_RED     "\033[91m"
#define COLOR_BRIGHT_GREEN   "\033[92m"
#define COLOR_BRIGHT_YELLOW  "\033[93m"
#define COLOR_BRIGHT_BLUE    "\033[94m"
#define COLOR_BRIGHT_MAGENTA "\033[95m"
#define COLOR_BRIGHT_CYAN    "\033[96m"
#define COLOR_BRIGHT_WHITE   "\033[97m"

// Background colors
#define COLOR_BG_RED        "\033[41m"
#define COLOR_BG_GREEN      "\033[42m"
#define COLOR_BG_YELLOW     "\033[43m"
#define COLOR_BG_BLUE       "\033[44m"
#define COLOR_BG_MAGENTA    "\033[45m"
#define COLOR_BG_CYAN       "\033[46m"

// Color combinations for vibrant effects
#define COLOR_SUCCESS       COLOR_BRIGHT_GREEN
#define COLOR_ERROR          COLOR_BRIGHT_RED
#define COLOR_WARNING        COLOR_BRIGHT_YELLOW
#define COLOR_INFO           COLOR_BRIGHT_CYAN
#define COLOR_HIGHLIGHT      COLOR_BRIGHT_MAGENTA
#define COLOR_ACCENT         COLOR_BRIGHT_BLUE

// Enhanced Icons - colorful and full of life
#define ICON_SUCCESS        "✓"
#define ICON_ERROR          "✗"
#define ICON_WARNING        "⚠"
#define ICON_INFO           "ℹ"
#define ICON_IN_PROGRESS    "⟳"
#define ICON_ARROW          "→"
#define ICON_STAR           "★"
#define ICON_SPARKLE        "✨"
#define ICON_ROCKET         "🚀"
#define ICON_PACKAGE        "📦"
#define ICON_GEAR           "⚙"
#define ICON_FLAME          "🔥"
#define ICON_CHECK          "✔"
#define ICON_DOWNLOAD       "⬇"
#define ICON_BUILD          "🔨"
#define ICON_INSTALL        "📥"

// Spinner frames for animated progress
static const char *SPINNER_FRAMES[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};
#define SPINNER_FRAMES_COUNT 10

// Vibrant section header with colorful styling
static inline void print_section(const char *title) {
    if (supports_colors()) {
        printf("%s%s═══>%s %s%s%s%s\n",
               COLOR_BRIGHT_CYAN, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_MAGENTA, COLOR_BOLD, title, COLOR_RESET);
    } else {
    printf("==> %s\n", title);
    }
}

// Vibrant success message with sparkle
static inline void print_success(const char *msg) {
    if (supports_colors()) {
        printf("%s%s%s%s %s%s%s\n",
               COLOR_SUCCESS, COLOR_BOLD, ICON_SUCCESS, COLOR_RESET,
               COLOR_SUCCESS, msg, COLOR_RESET);
    } else {
        printf("%s %s\n", ICON_SUCCESS, msg);
    }
}

// Vibrant error message
static inline void print_error(const char *msg) {
    if (supports_colors()) {
        fprintf(stderr, "%s%s%s%s %s%s%s\n",
                COLOR_ERROR, COLOR_BOLD, ICON_ERROR, COLOR_RESET,
                COLOR_ERROR, msg, COLOR_RESET);
    } else {
        fprintf(stderr, "%s %s\n", ICON_ERROR, msg);
    }
}

// Vibrant warning message
static inline void print_warning(const char *msg) {
    if (supports_colors()) {
        printf("%s%s%s%s %s%s%s\n",
               COLOR_WARNING, COLOR_BOLD, ICON_WARNING, COLOR_RESET,
               COLOR_WARNING, msg, COLOR_RESET);
    } else {
        printf("%s %s\n", ICON_WARNING, msg);
    }
}

// Vibrant info message
static inline void print_info(const char *msg) {
    if (supports_colors()) {
        printf("%s%s%s %s%s%s\n",
               COLOR_INFO, ICON_INFO, COLOR_RESET,
               COLOR_INFO, msg, COLOR_RESET);
    } else {
    printf("%s\n", msg);
    }
}

// Print package with version (colorful)
static inline void print_package(const char *name, const char *version) {
    if (supports_colors()) {
        if (version) {
            printf("  %s%s%s %s%s%s\n",
                   COLOR_BRIGHT_BLUE, ICON_PACKAGE, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, name, COLOR_RESET);
            printf("    %s%s%s\n", COLOR_DIM, version, COLOR_RESET);
        } else {
            printf("  %s%s%s %s%s%s\n",
                   COLOR_BRIGHT_BLUE, ICON_PACKAGE, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, name, COLOR_RESET);
        }
    } else {
    if (version) {
        printf("  %s %s\n", name, version);
    } else {
        printf("  %s\n", name);
        }
    }
}

// Print package with version change (colorful with arrow)
static inline void print_package_update(const char *name, const char *old_version, const char *new_version) {
    if (supports_colors()) {
        if (old_version && new_version) {
            printf("  %s%s%s %s%s%s %s%s%s %s%s%s %s%s%s\n",
                   COLOR_BRIGHT_BLUE, ICON_PACKAGE, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, name, COLOR_RESET,
                   COLOR_DIM, old_version, COLOR_RESET,
                   COLOR_BRIGHT_MAGENTA, ICON_ARROW, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, new_version, COLOR_RESET);
        } else if (new_version) {
            printf("  %s%s%s %s%s%s %s%s%s %s%s%s\n",
                   COLOR_BRIGHT_BLUE, ICON_PACKAGE, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, name, COLOR_RESET,
                   COLOR_BRIGHT_MAGENTA, ICON_ARROW, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, new_version, COLOR_RESET);
        } else {
            printf("  %s%s%s %s%s%s\n",
                   COLOR_BRIGHT_BLUE, ICON_PACKAGE, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, name, COLOR_RESET);
        }
    } else {
    if (old_version && new_version) {
        printf("  %s %s -> %s\n", name, old_version, new_version);
    } else if (new_version) {
        printf("  %s -> %s\n", name, new_version);
    } else {
        printf("  %s\n", name);
        }
    }
}

// Print download progress (colorful with download icon)
static inline void print_download(const char *icon, const char *package, const char *version,
                                  const char *size_info) {
    if (supports_colors()) {
        const char *use_icon = icon ? icon : ICON_DOWNLOAD;
    if (version) {
            printf("%s%s%s %s%s%s %s%s%s: %s%s%s\n",
                   COLOR_BRIGHT_CYAN, use_icon, COLOR_RESET,
                   COLOR_BRIGHT_BLUE, package, COLOR_RESET,
                   COLOR_DIM, version, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, size_info, COLOR_RESET);
        } else {
            printf("%s%s%s %s%s%s: %s%s%s\n",
                   COLOR_BRIGHT_CYAN, use_icon, COLOR_RESET,
                   COLOR_BRIGHT_BLUE, package, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, size_info, COLOR_RESET);
        }
    } else {
        const char *use_icon = icon ? icon : ICON_DOWNLOAD;
        if (version) {
            printf("%s %s (%s): %s\n", use_icon, package, version, size_info);
        } else {
            printf("%s %s: %s\n", use_icon, package, size_info);
        }
    }
}

// Print in-progress operation (colorful with spinner)
static inline void print_progress(const char *operation, const char *detail) {
    if (supports_colors()) {
        printf("%s%s%s%s %s%s%s",
               COLOR_ACCENT, COLOR_BOLD, ICON_IN_PROGRESS, COLOR_RESET,
               COLOR_ACCENT, operation, COLOR_RESET);
        if (detail) {
            printf(": %s%s%s", COLOR_INFO, detail, COLOR_RESET);
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

// Print "Building" message (colorful with build icon)
static inline void print_building(const char *package, const char *version) {
    if (supports_colors()) {
        if (version) {
            printf("%s%s═══>%s %s%s%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_YELLOW, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_YELLOW, ICON_BUILD, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, package,
                   COLOR_RESET, COLOR_DIM, version, COLOR_RESET);
        } else {
            printf("%s%s═══>%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_YELLOW, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_YELLOW, ICON_BUILD, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, package, COLOR_RESET);
        }
    } else {
    if (version) {
        printf("==> Building %s %s\n", package, version);
    } else {
        printf("==> Building %s\n", package);
        }
    }
}

// Print "Installing" message (colorful with install icon)
static inline void print_installing(const char *package, const char *version) {
    if (supports_colors()) {
        if (version) {
            printf("%s%s═══>%s %s%s%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, ICON_INSTALL, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, package,
                   COLOR_RESET, COLOR_DIM, version, COLOR_RESET);
        } else {
            printf("%s%s═══>%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, ICON_INSTALL, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, package, COLOR_RESET);
        }
    } else {
    if (version) {
        printf("==> Installing %s %s\n", package, version);
    } else {
        printf("==> Installing %s\n", package);
        }
    }
}

// Print installation summary (colorful)
static inline void print_summary(const char *install_path, int file_count, const char *size) {
    if (supports_colors()) {
        if (file_count > 0 && size) {
            printf("%s%s═══>%s %s%sSummary%s\n",
                   COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET);
            printf("  %s%s%s %s(%d files, %s%s%s%s)\n",
                   COLOR_BRIGHT_CYAN, install_path, COLOR_RESET,
                   COLOR_DIM,
                   file_count,
                   COLOR_BRIGHT_GREEN, size, COLOR_RESET,
                   COLOR_DIM);
        } else if (install_path) {
            printf("%s%s═══>%s %s%sSummary%s\n",
                   COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET);
            printf("  %s%s%s\n", COLOR_BRIGHT_CYAN, install_path, COLOR_RESET);
        }
    } else {
    if (file_count > 0 && size) {
        printf("==> Summary\n");
        printf("  %s (%d files, %s)\n", install_path, file_count, size);
    } else if (install_path) {
        printf("==> Summary\n");
        printf("  %s\n", install_path);
        }
    }
}

// Print caveats section (colorful)
static inline void print_caveats_start(void) {
    if (supports_colors()) {
        printf("%s%s═══>%s %s%sCaveats%s\n",
               COLOR_BRIGHT_YELLOW, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_YELLOW, COLOR_BOLD, COLOR_RESET);
    } else {
    printf("==> Caveats\n");
    }
}

// Print a caveat line (colorful)
static inline void print_caveat(const char *caveat) {
    if (supports_colors()) {
        printf("  %s%s%s %s%s%s\n",
               COLOR_WARNING, ICON_WARNING, COLOR_RESET,
               COLOR_WARNING, caveat, COLOR_RESET);
    } else {
    printf("  %s\n", caveat);
    }
}

// Print cleanup message (colorful)
static inline void print_cleanup(const char *package, const char *old_version, int file_count, const char *size) {
    if (supports_colors()) {
        printf("%s%s═══>%s %s%sCleaning up%s %s%s%s\n",
               COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_CYAN, package, COLOR_RESET);
        if (old_version) {
            if (file_count > 0 && size) {
                printf("  %sRemoved%s %s%s%s %s(%d files, %s%s%s%s)\n",
                       COLOR_DIM, COLOR_RESET,
                       COLOR_BRIGHT_RED, old_version, COLOR_RESET,
                       COLOR_DIM,
                       file_count,
                       COLOR_BRIGHT_GREEN, size, COLOR_RESET,
                       COLOR_DIM);
            } else {
                printf("  %sRemoved%s %s%s%s\n",
                       COLOR_DIM, COLOR_RESET,
                       COLOR_BRIGHT_RED, old_version, COLOR_RESET);
            }
        }
    } else {
    printf("==> Cleaning up %s\n", package);
    if (old_version) {
        if (file_count > 0 && size) {
            printf("  Removed %s (%d files, %s)\n", old_version, file_count, size);
        } else {
            printf("  Removed %s\n", old_version);
            }
        }
    }
}

// Terminal control sequences
#define CLEAR_LINE "\033[2K"  // Clear entire line
#define HIDE_CURSOR "\033[?25l"  // Hide cursor
#define SHOW_CURSOR "\033[?25h"  // Show cursor

// Progress bar structure
typedef struct {
    int width;
    int current;
    int total;
    char label[128];
} ProgressBar;

// Initialize progress bar
static inline void progress_bar_init(ProgressBar *bar, const char *label, int total, int width) {
    bar->current = 0;
    bar->total = total > 0 ? total : 100;
    bar->width = width > 0 ? width : 40;
    if (label) {
        strncpy(bar->label, label, sizeof(bar->label) - 1);
        bar->label[sizeof(bar->label) - 1] = '\0';
    } else {
        bar->label[0] = '\0';
    }
}

// Update and display progress bar
static inline void progress_bar_update(ProgressBar *bar, int current) {
    if (!is_tty() || !supports_colors()) {
        // Fallback: just print percentage
        if (bar->label[0]) {
            printf("%s: %d%%\n", bar->label, (current * 100) / bar->total);
        } else {
            printf("%d%%\n", (current * 100) / bar->total);
        }
        return;
    }

    bar->current = current > bar->total ? bar->total : current;
    int percent = (bar->current * 100) / bar->total;
    int filled = (bar->current * bar->width) / bar->total;

    printf("\r%s", CLEAR_LINE);
    if (bar->label[0]) {
        printf("%s%s%s ", COLOR_BRIGHT_CYAN, bar->label, COLOR_RESET);
    }

    // Progress bar with gradient colors
    printf("%s[", COLOR_DIM);
    for (int i = 0; i < bar->width; i++) {
        if (i < filled) {
            // Gradient: green -> cyan -> blue
            if (i < filled / 3) {
                printf("%s█", COLOR_BRIGHT_GREEN);
            } else if (i < (filled * 2) / 3) {
                printf("%s█", COLOR_BRIGHT_CYAN);
            } else {
                printf("%s█", COLOR_BRIGHT_BLUE);
            }
        } else {
            printf("%s░", COLOR_DIM);
        }
    }
    printf("%s] %s%d%%%s", COLOR_DIM, COLOR_BRIGHT_YELLOW, percent, COLOR_RESET);
    fflush(stdout);
}

// Finish progress bar
static inline void progress_bar_finish(ProgressBar *bar) {
    progress_bar_update(bar, bar->total);
    printf("\n");
}

// Spinner state
typedef struct {
    int frame;
    char message[128];
} Spinner;

// Initialize spinner
static inline void spinner_init(Spinner *spinner, const char *message) {
    spinner->frame = 0;
    if (message) {
        strncpy(spinner->message, message, sizeof(spinner->message) - 1);
        spinner->message[sizeof(spinner->message) - 1] = '\0';
    } else {
        spinner->message[0] = '\0';
    }
}

// Update spinner (call this repeatedly)
static inline void spinner_update(Spinner *spinner) {
    if (!is_tty() || !supports_colors()) {
        return; // Don't show spinner on non-TTY
    }

    const char *frame = SPINNER_FRAMES[spinner->frame % SPINNER_FRAMES_COUNT];
    printf("\r%s%s%s %s%s%s",
           COLOR_ACCENT, frame, COLOR_RESET,
           COLOR_INFO, spinner->message, COLOR_RESET);
    fflush(stdout);
    spinner->frame++;
}

// Finish spinner
static inline void spinner_finish(Spinner *spinner, bool success) {
    if (!is_tty() || !supports_colors()) {
        printf("%s\n", spinner->message);
        return;
    }

    const char *icon = success ? ICON_SUCCESS : ICON_ERROR;
    const char *color = success ? COLOR_SUCCESS : COLOR_ERROR;
    printf("\r%s%s%s%s %s%s%s\n",
           CLEAR_LINE,
           color, icon, COLOR_RESET,
           color, spinner->message, COLOR_RESET);
    fflush(stdout);
}

// Print status that updates in place (single line, colorful)
static inline void print_status_inline(const char *status) {
    if (is_tty() && supports_colors()) {
        printf("\r%s%s%s%s%s", CLEAR_LINE, COLOR_ACCENT, status, COLOR_RESET, "");
        fflush(stdout);
    } else if (is_tty()) {
        printf("\r%s%s", CLEAR_LINE, status);
        fflush(stdout);
    } else {
        printf("%s\n", status);
    }
}

// Print compact building status (colorful, prints on new line)
static inline void print_building_compact(const char *package, const char *version) {
    if (supports_colors()) {
        if (version) {
            printf("%s%s═══>%s %s%s%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_YELLOW, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_YELLOW, ICON_BUILD, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, package,
                   COLOR_RESET, COLOR_DIM, version, COLOR_RESET);
        } else {
            printf("%s%s═══>%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_YELLOW, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_YELLOW, ICON_BUILD, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, package, COLOR_RESET);
        }
    } else {
    if (version) {
        printf("==> Building %s %s\n", package, version);
    } else {
        printf("==> Building %s\n", package);
        }
    }
    fflush(stdout);
}

// Print compact installing status (colorful, prints on new line)
static inline void print_installing_compact(const char *package, const char *version) {
    if (supports_colors()) {
        if (version) {
            printf("%s%s═══>%s %s%s%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, ICON_INSTALL, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, package,
                   COLOR_RESET, COLOR_DIM, version, COLOR_RESET);
        } else {
            printf("%s%s═══>%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, ICON_INSTALL, COLOR_RESET,
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, package, COLOR_RESET);
        }
    } else {
    if (version) {
        printf("==> Installing %s %s\n", package, version);
    } else {
        printf("==> Installing %s\n", package);
        }
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
    bool display_started;  // Track if we've started displaying
} OutputBuffer;

// Initialize output buffer
static inline void output_buffer_init(OutputBuffer *buf) {
    buf->line_count = 0;
    buf->current_index = 0;
    buf->display_started = false;
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

    // Always display exactly OUTPUT_BUFFER_LINES lines (reserved space)
    // If we have fewer lines, show empty lines for the rest

    // Move cursor up to the start of the output area
    printf("\033[%dA", OUTPUT_BUFFER_LINES);

    // Display exactly OUTPUT_BUFFER_LINES lines
    for (int i = 0; i < OUTPUT_BUFFER_LINES; i++) {
        // Calculate which line to show (most recent at bottom)
        int idx;
        if (i < buf->line_count) {
            // We have a line to show
            if (buf->line_count <= OUTPUT_BUFFER_LINES) {
                // Buffer not full yet, show from start
                idx = i;
            } else {
                // Buffer is full, show the last OUTPUT_BUFFER_LINES lines
                // Show oldest first (at top), newest last (at bottom)
                idx = (buf->current_index - OUTPUT_BUFFER_LINES + i + OUTPUT_BUFFER_LINES) % OUTPUT_BUFFER_LINES;
            }

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
            // Clear line and print with newline (colorful)
            if (supports_colors()) {
                printf("\r\033[2K  %s%s%s\n", COLOR_DIM, display_line, COLOR_RESET);
            } else {
            printf("\r\033[2K  %s\n", display_line);
            }
        } else {
            // No line to show yet, print empty line
            printf("\r\033[2K\n");
        }
    }
    // After displaying all lines, cursor is positioned correctly for next update
    fflush(stdout);
}

// Start output capture area (after status line)
// Reserve space for OUTPUT_BUFFER_LINES to prevent jumping
static inline void output_capture_start(void) {
    if (is_tty()) {
        // Reserve space by printing empty lines
        for (int i = 0; i < OUTPUT_BUFFER_LINES; i++) {
            printf("\n");
        }
        // Move cursor back up to the first output line
        printf("\033[%dA", OUTPUT_BUFFER_LINES);
    }
}

// End output capture area (clear output lines, keep status line)
static inline void output_capture_end(OutputBuffer *buf) {
    if (!buf || !is_tty()) return;

    // Clear the output area but preserve status line
    if (buf->display_started && buf->line_count > 0) {
        int lines_to_clear = buf->line_count < OUTPUT_BUFFER_LINES ? buf->line_count : OUTPUT_BUFFER_LINES;
        // Move up to the output area
        printf("\033[%dA", lines_to_clear);  // Move up
        // Clear each output line
        for (int i = 0; i < lines_to_clear; i++) {
            printf("\r\033[2K\n");  // Clear each line
        }
        // Reset display state
        buf->display_started = false;
    }
}

#ifdef __cplusplus
}
#endif

#endif // TUI_H

