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
    int dash_len = OUTPUT_WINDOW_WIDTH - (int)len - 2;
    if (dash_len < 2) dash_len = 2;
    if (color) {
        printf("%s┌ %s%s%s ", COLOR_DIM, COLOR_INFO, title_buf, COLOR_RESET);
        for (int i = 0; i < dash_len; i++) printf("─");
        printf("┐%s\n", COLOR_DIM);
        printf("%s", COLOR_RESET);
    } else {
        printf("/ %s ", title_buf);
        for (int i = 0; i < dash_len; i++) printf("-");
        printf("\\\n");
    }
}

static inline void output_window_print_line(const char *content) {
    bool color = supports_colors();
    char display_line[OUTPUT_WINDOW_WIDTH + 1];
    size_t width = OUTPUT_WINDOW_WIDTH - 2;
    if (width >= sizeof(display_line)) width = sizeof(display_line) - 1;

    if (content && *content) {
        strncpy(display_line, content, width);
        display_line[width] = '\0';
    } else {
        display_line[0] = '\0';
    }

    size_t text_len = strlen(display_line);
    for (size_t i = text_len; i < width; i++) {
        display_line[i] = ' ';
    }
    display_line[width] = '\0';

    if (color) {
        printf("%s│ %s%s%s %s│%s\n",
               COLOR_DIM, COLOR_RESET, display_line, COLOR_RESET, COLOR_DIM, COLOR_RESET);
    } else {
        printf("| %s |\n", display_line);
    }
}

static inline void output_window_print_footer(void) {
    bool color = supports_colors();
    if (color) {
        printf("%s└", COLOR_DIM);
        for (int i = 0; i < OUTPUT_WINDOW_WIDTH; i++) printf("─");
        printf("┘%s\n", COLOR_RESET);
    } else {
        printf("\\");
        for (int i = 0; i < OUTPUT_WINDOW_WIDTH; i++) printf("-");
        printf("/\n");
    }
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

    printf("\033[%dA", OUTPUT_BUFFER_LINES + 1);

    for (int i = 0; i < OUTPUT_BUFFER_LINES; i++) {
        int idx;
        if (buf->line_count == 0) {
            output_window_print_line("");
            continue;
        }

        if (buf->line_count <= OUTPUT_BUFFER_LINES) {
            idx = i < buf->line_count ? i : -1;
        } else {
            idx = (buf->current_index - OUTPUT_BUFFER_LINES + i + OUTPUT_BUFFER_LINES) % OUTPUT_BUFFER_LINES;
        }

        if (idx >= 0 && i < buf->line_count) {
            char display_line[OUTPUT_LINE_LENGTH];
            strncpy(display_line, buf->lines[idx], sizeof(display_line) - 1);
            display_line[sizeof(display_line) - 1] = '\0';
            output_window_print_line(display_line);
        } else {
            output_window_print_line("");
        }
    }
    output_window_print_footer();
    fflush(stdout);
    buf->display_started = true;
}

// Start output capture area with titled window
static inline void output_capture_start(const char *title) {
    if (!is_tty()) return;
    output_window_print_header(title);
    for (int i = 0; i < OUTPUT_BUFFER_LINES; i++) {
        output_window_print_line("");
    }
    output_window_print_footer();
}

// End output capture area (clear output lines, keep status line)
static inline void output_capture_end(OutputBuffer *buf) {
    if (!buf || !is_tty()) return;
    if (!buf->display_started) {
        // Still clear the placeholder window
        printf("\033[%dA", OUTPUT_BUFFER_LINES + 2);
        for (int i = 0; i < OUTPUT_BUFFER_LINES + 2; i++) {
            printf("\r\033[2K\n");
        }
        buf->display_started = false;
        return;
    }

    printf("\033[%dA", OUTPUT_BUFFER_LINES + 2);
    for (int i = 0; i < OUTPUT_BUFFER_LINES + 2; i++) {
        printf("\r\033[2K\n");
    }
    buf->display_started = false;
}

#ifdef __cplusplus
}
#endif

#endif // TUI_H

