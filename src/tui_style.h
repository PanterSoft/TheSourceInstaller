#ifndef TUI_STYLE_H
#define TUI_STYLE_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TUI_COLOR_RESET = 0,
    TUI_COLOR_BOLD,
    TUI_COLOR_DIM,
    TUI_COLOR_ITALIC,
    TUI_COLOR_UNDERLINE,
    TUI_COLOR_BLACK,
    TUI_COLOR_RED,
    TUI_COLOR_GREEN,
    TUI_COLOR_YELLOW,
    TUI_COLOR_BLUE,
    TUI_COLOR_MAGENTA,
    TUI_COLOR_CYAN,
    TUI_COLOR_WHITE,
    TUI_COLOR_BRIGHT_BLACK,
    TUI_COLOR_BRIGHT_RED,
    TUI_COLOR_BRIGHT_GREEN,
    TUI_COLOR_BRIGHT_YELLOW,
    TUI_COLOR_BRIGHT_BLUE,
    TUI_COLOR_BRIGHT_MAGENTA,
    TUI_COLOR_BRIGHT_CYAN,
    TUI_COLOR_BRIGHT_WHITE,
    TUI_COLOR_BG_RED,
    TUI_COLOR_BG_GREEN,
    TUI_COLOR_BG_YELLOW,
    TUI_COLOR_BG_BLUE,
    TUI_COLOR_BG_MAGENTA,
    TUI_COLOR_BG_CYAN,
    TUI_COLOR_SUCCESS,
    TUI_COLOR_ERROR,
    TUI_COLOR_WARNING,
    TUI_COLOR_INFO,
    TUI_COLOR_HIGHLIGHT,
    TUI_COLOR_ACCENT,
    TUI_COLOR_COUNT
} TuiColorRole;

typedef enum {
    TUI_ICON_SUCCESS = 0,
    TUI_ICON_ERROR,
    TUI_ICON_WARNING,
    TUI_ICON_INFO,
    TUI_ICON_PROGRESS,
    TUI_ICON_ARROW,
    TUI_ICON_STAR,
    TUI_ICON_SPARKLE,
    TUI_ICON_ROCKET,
    TUI_ICON_PACKAGE,
    TUI_ICON_GEAR,
    TUI_ICON_FLAME,
    TUI_ICON_CHECK,
    TUI_ICON_DOWNLOAD,
    TUI_ICON_BUILD,
    TUI_ICON_INSTALL,
    TUI_ICON_COUNT
} TuiIconRole;

void tui_style_reload_from_env(void);
bool tui_style_apply(const char *name);
const char *tui_style_active_name(void);
const char *tui_style_color(TuiColorRole role);
const char *tui_style_icon(TuiIconRole role);
const char *tui_style_spinner_frame(size_t idx);
size_t tui_style_spinner_frame_count(void);
bool tui_style_colors_enabled(void);
void tui_style_set_colors_enabled(bool enabled);

#endif // TUI_STYLE_H
