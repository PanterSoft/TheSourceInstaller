#ifndef TUI_H
#define TUI_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "tui_style.h"

#ifdef __cplusplus
extern "C" {
#endif

// Terminal capability detection
static inline bool is_tty(void) {
    return isatty(fileno(stdout));
}

// Check if terminal supports colors (works on bash, zsh, and most serial consoles)
static inline bool supports_colors(void) {
    if (!tui_style_colors_enabled()) return false;
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
#define COLOR_RESET        tui_style_color(TUI_COLOR_RESET)
#define COLOR_BOLD         tui_style_color(TUI_COLOR_BOLD)
#define COLOR_DIM          tui_style_color(TUI_COLOR_DIM)
#define COLOR_ITALIC       tui_style_color(TUI_COLOR_ITALIC)
#define COLOR_UNDERLINE    tui_style_color(TUI_COLOR_UNDERLINE)

// Standard colors
#define COLOR_BLACK        tui_style_color(TUI_COLOR_BLACK)
#define COLOR_RED          tui_style_color(TUI_COLOR_RED)
#define COLOR_GREEN        tui_style_color(TUI_COLOR_GREEN)
#define COLOR_YELLOW       tui_style_color(TUI_COLOR_YELLOW)
#define COLOR_BLUE         tui_style_color(TUI_COLOR_BLUE)
#define COLOR_MAGENTA      tui_style_color(TUI_COLOR_MAGENTA)
#define COLOR_CYAN         tui_style_color(TUI_COLOR_CYAN)
#define COLOR_WHITE        tui_style_color(TUI_COLOR_WHITE)

// Bright colors
#define COLOR_BRIGHT_BLACK   tui_style_color(TUI_COLOR_BRIGHT_BLACK)
#define COLOR_BRIGHT_RED     tui_style_color(TUI_COLOR_BRIGHT_RED)
#define COLOR_BRIGHT_GREEN   tui_style_color(TUI_COLOR_BRIGHT_GREEN)
#define COLOR_BRIGHT_YELLOW  tui_style_color(TUI_COLOR_BRIGHT_YELLOW)
#define COLOR_BRIGHT_BLUE    tui_style_color(TUI_COLOR_BRIGHT_BLUE)
#define COLOR_BRIGHT_MAGENTA tui_style_color(TUI_COLOR_BRIGHT_MAGENTA)
#define COLOR_BRIGHT_CYAN    tui_style_color(TUI_COLOR_BRIGHT_CYAN)
#define COLOR_BRIGHT_WHITE   tui_style_color(TUI_COLOR_BRIGHT_WHITE)

// Background colors
#define COLOR_BG_RED        tui_style_color(TUI_COLOR_BG_RED)
#define COLOR_BG_GREEN      tui_style_color(TUI_COLOR_BG_GREEN)
#define COLOR_BG_YELLOW     tui_style_color(TUI_COLOR_BG_YELLOW)
#define COLOR_BG_BLUE       tui_style_color(TUI_COLOR_BG_BLUE)
#define COLOR_BG_MAGENTA    tui_style_color(TUI_COLOR_BG_MAGENTA)
#define COLOR_BG_CYAN       tui_style_color(TUI_COLOR_BG_CYAN)

// Style roles
#define COLOR_SUCCESS       tui_style_color(TUI_COLOR_SUCCESS)
#define COLOR_ERROR         tui_style_color(TUI_COLOR_ERROR)
#define COLOR_WARNING       tui_style_color(TUI_COLOR_WARNING)
#define COLOR_INFO          tui_style_color(TUI_COLOR_INFO)
#define COLOR_HIGHLIGHT     tui_style_color(TUI_COLOR_HIGHLIGHT)
#define COLOR_ACCENT        tui_style_color(TUI_COLOR_ACCENT)

// Icons
#define ICON_SUCCESS        tui_style_icon(TUI_ICON_SUCCESS)
#define ICON_ERROR          tui_style_icon(TUI_ICON_ERROR)
#define ICON_WARNING        tui_style_icon(TUI_ICON_WARNING)
#define ICON_INFO           tui_style_icon(TUI_ICON_INFO)
#define ICON_IN_PROGRESS    tui_style_icon(TUI_ICON_PROGRESS)
#define ICON_ARROW          tui_style_icon(TUI_ICON_ARROW)
#define ICON_STAR           tui_style_icon(TUI_ICON_STAR)
#define ICON_SPARKLE        tui_style_icon(TUI_ICON_SPARKLE)
#define ICON_ROCKET         tui_style_icon(TUI_ICON_ROCKET)
#define ICON_PACKAGE        tui_style_icon(TUI_ICON_PACKAGE)
#define ICON_GEAR           tui_style_icon(TUI_ICON_GEAR)
#define ICON_FLAME          tui_style_icon(TUI_ICON_FLAME)
#define ICON_CHECK          tui_style_icon(TUI_ICON_CHECK)
#define ICON_DOWNLOAD       tui_style_icon(TUI_ICON_DOWNLOAD)
#define ICON_BUILD          tui_style_icon(TUI_ICON_BUILD)
#define ICON_INSTALL        tui_style_icon(TUI_ICON_INSTALL)

// Homebrew-style section header
static inline void print_section(const char *title) {
    if (supports_colors()) {
        printf("%s%s==>%s %s%s%s%s\n",
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_GREEN, COLOR_BOLD, title, COLOR_RESET);
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

// Homebrew-style package update listing (package old -> new)
// Format: "  package old -> new" (plain text, no colors for the list)
static inline void print_package_update(const char *name, const char *old_version, const char *new_version) {
    if (old_version && new_version) {
        printf("  %s %s -> %s\n", name, old_version, new_version);
    } else if (new_version) {
        printf("  %s -> %s\n", name, new_version);
    } else {
        printf("  %s\n", name);
    }
}

// Homebrew-style upgrade header: "==> Upgrading X outdated packages:"
static inline void print_upgrading_header(size_t count) {
    if (supports_colors()) {
        printf("%s%s==>%s %s%sUpgrading %zu outdated package%s:%s\n",
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_GREEN, COLOR_BOLD, count, count == 1 ? "" : "s", COLOR_RESET);
    } else {
        printf("==> Upgrading %zu outdated package%s:\n", count, count == 1 ? "" : "s");
    }
}

// Homebrew-style fetching header: "==> Fetching downloads for: package1, package2, ..."
static inline void print_fetching_header(const char **packages, size_t count) {
    if (supports_colors()) {
        printf("%s%s==>%s %s%sFetching downloads for:%s",
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET);
    } else {
        printf("==> Fetching downloads for:");
    }

    if (packages && count > 0) {
        // Print comma-separated list with bright green package names
        for (size_t i = 0; i < count; i++) {
            if (i == 0) {
                printf(" ");
            } else {
                printf(", ");
            }
            if (supports_colors() && packages[i]) {
                printf("%s%s%s", COLOR_BRIGHT_GREEN, packages[i], COLOR_RESET);
            } else if (packages[i]) {
                printf("%s", packages[i]);
            }
        }
    }
    printf("\n");
}

// Homebrew-style version change line: "old -> new" (shown after "==> Upgrading package")
static inline void print_version_change(const char *old_version, const char *new_version) {
    if (old_version && new_version) {
        printf("%s -> %s\n", old_version, new_version);
    } else if (new_version) {
        printf("-> %s\n", new_version);
    }
}

// Homebrew-style "Pouring" message: "==> Pouring package--version.bottle.tar.gz"
static inline void print_pouring(const char *bottle_file) {
    if (supports_colors()) {
        printf("%s%s==>%s %s%sPouring%s %s%s%s\n",
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_CYAN, bottle_file ? bottle_file : "", COLOR_RESET);
    } else {
        printf("==> Pouring %s\n", bottle_file ? bottle_file : "");
    }
}

// Homebrew-style download progress with right-aligned status
// Format: "✓ Bottle package-name (version)" with right-aligned "[Downloaded X/Y]"
static inline void print_download(const char *icon, const char *package, const char *version,
                                  const char *size_info) {
    const char *use_icon = icon ? icon : ICON_CHECK;

    // Try to detect terminal width, default to 80
    int terminal_width = 80;
    if (is_tty()) {
        const char *cols = getenv("COLUMNS");
        if (cols) {
            int w = atoi(cols);
            if (w > 0) terminal_width = w;
        }
    }

    // Calculate widths for right-alignment
    int package_len = (int)strlen(package);
    int version_len = version ? (int)strlen(version) : 0;
    int icon_len = (int)strlen(use_icon);
    int size_len = size_info ? (int)strlen(size_info) : 0;

    // Calculate label width: "✓ Bottle package (version)"
    // icon + space + "Bottle" + space + package + space + ( + version + )
    int label_width = icon_len + 1 + 6 + 1 + package_len; // "✓ Bottle package"
    if (version) {
        label_width += 1 + version_len + 1; // " (version)"
    }

    if (supports_colors()) {
        // Green checkmark
        printf("%s%s%s", COLOR_BRIGHT_GREEN, use_icon, COLOR_RESET);
        // Plain "Bottle" text (not colored)
        printf(" Bottle ");
        // Bright cyan package name
        printf("%s%s%s", COLOR_BRIGHT_CYAN, package, COLOR_RESET);
        // Dim version in parentheses
        if (version) {
            printf(" %s(%s)%s", COLOR_DIM, version, COLOR_RESET);
        }

        // Right-align the size info (Homebrew style)
        if (size_info) {
            // Calculate padding: terminal_width - label_width - "[Downloaded " - size - "]"
            int downloaded_text_len = 11; // "Downloaded "
            int total_right_width = downloaded_text_len + size_len + 1; // +1 for closing bracket
            int padding = terminal_width - label_width - total_right_width;
            if (padding < 1) padding = 1;
            for (int i = 0; i < padding; i++) printf(" ");
            // Dim brackets and "Downloaded " text, bright green for size
            printf("%s[Downloaded %s%s%s]%s\n",
                   COLOR_DIM, COLOR_BRIGHT_GREEN, size_info, COLOR_DIM, COLOR_RESET);
        } else {
            printf("\n");
        }
    } else {
        printf("%s Bottle %s", use_icon, package);
        if (version) {
            printf(" (%s)", version);
        }
        if (size_info) {
            int downloaded_text_len = 11;
            int total_right_width = downloaded_text_len + size_len + 1;
            int padding = terminal_width - label_width - total_right_width;
            if (padding < 1) padding = 1;
            for (int i = 0; i < padding; i++) printf(" ");
            printf("[Downloaded %s]\n", size_info);
        } else {
            printf("\n");
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

// Styled overview line for step-oriented output
static inline void print_step_overview(const char *label, const char *detail) {
    if (supports_colors()) {
        printf("%s%s%s %s%s%s",
               COLOR_ACCENT, ICON_ARROW, COLOR_RESET,
               COLOR_BOLD, label ? label : "", COLOR_RESET);
        if (detail && *detail) {
            printf(" %s%s%s", COLOR_INFO, detail, COLOR_RESET);
        }
        printf("\n");
    } else {
        printf("-> %s", label ? label : "");
        if (detail && *detail) {
            printf(" %s", detail);
        }
        printf("\n");
    }
}

// Homebrew-style "Building" message
static inline void print_building(const char *package, const char *version) {
    if (supports_colors()) {
        if (version) {
            printf("%s%s==>%s %s%s%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, "Building", COLOR_RESET,
                   COLOR_BRIGHT_CYAN, package, COLOR_RESET,
                   COLOR_DIM, version, COLOR_RESET);
        } else {
            printf("%s%s==>%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, "Building", COLOR_RESET,
                   COLOR_BRIGHT_CYAN, package, COLOR_RESET);
        }
    } else {
        if (version) {
            printf("==> Building %s %s\n", package, version);
        } else {
            printf("==> Building %s\n", package);
        }
    }
}

// Homebrew-style "Installing" message
static inline void print_installing(const char *package, const char *version) {
    if (supports_colors()) {
        if (version) {
            printf("%s%s==>%s %s%s%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, "Installing", COLOR_RESET,
                   COLOR_BRIGHT_CYAN, package, COLOR_RESET,
                   COLOR_DIM, version, COLOR_RESET);
        } else {
            printf("%s%s==>%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, "Installing", COLOR_RESET,
                   COLOR_BRIGHT_CYAN, package, COLOR_RESET);
        }
    } else {
        if (version) {
            printf("==> Installing %s %s\n", package, version);
        } else {
            printf("==> Installing %s\n", package);
        }
    }
}

// Homebrew-style installation summary
static inline void print_summary(const char *install_path, int file_count, const char *size) {
    if (supports_colors()) {
        if (file_count > 0 && size) {
            printf("%s%s==>%s %s%sSummary%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET);
            printf("  %s%s%s %s(%d files, %s%s%s%s)\n",
                   COLOR_BRIGHT_CYAN, install_path, COLOR_RESET,
                   COLOR_DIM,
                   file_count,
                   COLOR_BRIGHT_GREEN, size, COLOR_RESET,
                   COLOR_DIM);
        } else if (install_path) {
            printf("%s%s==>%s %s%sSummary%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET);
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

// Homebrew-style caveats section
static inline void print_caveats_start(void) {
    if (supports_colors()) {
        printf("%s%s==>%s %s%sCaveats%s\n",
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET);
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

// Homebrew-style cleanup message
static inline void print_cleanup(const char *package, const char *old_version, int file_count, const char *size) {
    if (supports_colors()) {
        printf("%s%s==>%s %s%sCleaning up%s %s%s%s\n",
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
               COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
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

    size_t frame_count = tui_style_spinner_frame_count();
    if (frame_count == 0) {
        return;
    }
    const char *frame = tui_style_spinner_frame((size_t)spinner->frame);
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

// Homebrew-style compact building status
static inline void print_building_compact(const char *package, const char *version) {
    if (supports_colors()) {
        if (version) {
            printf("%s%s==>%s %s%s%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, "Building", COLOR_RESET,
                   COLOR_BRIGHT_CYAN, package,
                   COLOR_RESET, COLOR_DIM, version, COLOR_RESET);
        } else {
            printf("%s%s==>%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, "Building", COLOR_RESET,
                   COLOR_BRIGHT_CYAN, package, COLOR_RESET);
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

// Homebrew-style compact installing status
static inline void print_installing_compact(const char *package, const char *version) {
    if (supports_colors()) {
        if (version) {
            printf("%s%s==>%s %s%s%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, "Installing", COLOR_RESET,
                   COLOR_BRIGHT_CYAN, package,
                   COLOR_RESET, COLOR_DIM, version, COLOR_RESET);
        } else {
            printf("%s%s==>%s %s%s%s %s%s%s%s\n",
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, COLOR_BOLD, "Installing", COLOR_RESET,
                   COLOR_BRIGHT_CYAN, package, COLOR_RESET);
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

#define OUTPUT_WINDOW_WIDTH 70

static inline void output_window_print_header(const char *title) {
    bool color = supports_colors();
    const char *label = (title && *title) ? title : "Command output";
    char title_buf[OUTPUT_LINE_LENGTH];
    strncpy(title_buf, label, sizeof(title_buf) - 1);
    title_buf[sizeof(title_buf) - 1] = '\0';
    size_t len = strlen(title_buf);
    if (len > OUTPUT_WINDOW_WIDTH - 10) {
        title_buf[OUTPUT_WINDOW_WIDTH - 10] = '\0';
        len = strlen(title_buf);
    }
    if (color) {
        printf("%s%s%s%s\n", COLOR_INFO, COLOR_BOLD, title_buf, COLOR_RESET);
    } else {
        printf("%s\n", title_buf);
    }
}

static inline void output_window_print_line(const char *content) {
    bool color = supports_colors();
    char display_line[OUTPUT_WINDOW_WIDTH + 1];
    size_t width = OUTPUT_WINDOW_WIDTH;
    if (width >= sizeof(display_line)) width = sizeof(display_line) - 1;

    if (content && *content) {
        strncpy(display_line, content, width);
        display_line[width] = '\0';
    } else {
        display_line[0] = '\0';
    }

    if (color) {
        printf("%s%s%s\n", COLOR_RESET, display_line, COLOR_RESET);
    } else {
        printf("%s\n", display_line);
    }
}

static inline void output_window_print_footer(void) {
    // No footer needed when borders are removed
}

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

    // Safety check: ensure len is valid
    if (len == 0) {
        buf->lines[buf->current_index][0] = '\0';
    } else {
        // Copy line (handle newline)
        strncpy(buf->lines[buf->current_index], line, len);
        buf->lines[buf->current_index][len] = '\0';

        // Remove trailing newline if present
        if (len > 0 && buf->lines[buf->current_index][len - 1] == '\n') {
            buf->lines[buf->current_index][len - 1] = '\0';
        }
    }

    buf->current_index = (buf->current_index + 1) % OUTPUT_BUFFER_LINES;
    if (buf->line_count < OUTPUT_BUFFER_LINES) {
        buf->line_count++;
    }
}

// Display the output buffer (moves cursor up and redraws, preserves status line above)
static inline void output_buffer_display(OutputBuffer *buf) {
    // Window removed - do nothing
    (void)buf;
}

// Start output capture area with titled window
static inline void output_capture_start(const char *title) {
    // Window removed - do nothing
    (void)title;
}

// End output capture area (clear output lines, keep status line)
static inline void output_capture_end(OutputBuffer *buf) {
    // Window removed - do nothing
    (void)buf;
}

#ifdef __cplusplus
}
#endif

#endif // TUI_H

