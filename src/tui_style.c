#include "tui_style.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *name;
    const char *aliases[4];
    const char **colors;
    const char **icons;
    const char **spinner_frames;
    size_t spinner_count;
    bool colors_enabled_default;
} TuiStyleDefinition;

static const char *VIBRANT_COLORS[TUI_COLOR_COUNT] = {
    [TUI_COLOR_RESET] = "\033[0m",
    [TUI_COLOR_BOLD] = "\033[1m",
    [TUI_COLOR_DIM] = "\033[2m",
    [TUI_COLOR_ITALIC] = "\033[3m",
    [TUI_COLOR_UNDERLINE] = "\033[4m",
    [TUI_COLOR_BLACK] = "\033[30m",
    [TUI_COLOR_RED] = "\033[31m",
    [TUI_COLOR_GREEN] = "\033[32m",
    [TUI_COLOR_YELLOW] = "\033[33m",
    [TUI_COLOR_BLUE] = "\033[34m",
    [TUI_COLOR_MAGENTA] = "\033[35m",
    [TUI_COLOR_CYAN] = "\033[36m",
    [TUI_COLOR_WHITE] = "\033[37m",
    [TUI_COLOR_BRIGHT_BLACK] = "\033[90m",
    [TUI_COLOR_BRIGHT_RED] = "\033[91m",
    [TUI_COLOR_BRIGHT_GREEN] = "\033[92m",
    [TUI_COLOR_BRIGHT_YELLOW] = "\033[93m",
    [TUI_COLOR_BRIGHT_BLUE] = "\033[94m",
    [TUI_COLOR_BRIGHT_MAGENTA] = "\033[95m",
    [TUI_COLOR_BRIGHT_CYAN] = "\033[96m",
    [TUI_COLOR_BRIGHT_WHITE] = "\033[97m",
    [TUI_COLOR_BG_RED] = "\033[41m",
    [TUI_COLOR_BG_GREEN] = "\033[42m",
    [TUI_COLOR_BG_YELLOW] = "\033[43m",
    [TUI_COLOR_BG_BLUE] = "\033[44m",
    [TUI_COLOR_BG_MAGENTA] = "\033[45m",
    [TUI_COLOR_BG_CYAN] = "\033[46m",
    [TUI_COLOR_SUCCESS] = "\033[92m",
    [TUI_COLOR_ERROR] = "\033[91m",
    [TUI_COLOR_WARNING] = "\033[93m",
    [TUI_COLOR_INFO] = "\033[96m",
    [TUI_COLOR_HIGHLIGHT] = "\033[95m",
    [TUI_COLOR_ACCENT] = "\033[94m"
};

static const char *VIBRANT_ICONS[TUI_ICON_COUNT] = {
    [TUI_ICON_SUCCESS] = "✓",
    [TUI_ICON_ERROR] = "✗",
    [TUI_ICON_WARNING] = "⚠",
    [TUI_ICON_INFO] = "ℹ",
    [TUI_ICON_PROGRESS] = "⟳",
    [TUI_ICON_ARROW] = "→",
    [TUI_ICON_STAR] = "★",
    [TUI_ICON_SPARKLE] = "✨",
    [TUI_ICON_ROCKET] = "🚀",
    [TUI_ICON_PACKAGE] = "📦",
    [TUI_ICON_GEAR] = "⚙",
    [TUI_ICON_FLAME] = "🔥",
    [TUI_ICON_CHECK] = "✔",
    [TUI_ICON_DOWNLOAD] = "⬇",
    [TUI_ICON_BUILD] = "🔨",
    [TUI_ICON_INSTALL] = "📥"
};

static const char *PLAIN_ICONS[TUI_ICON_COUNT] = {
    [TUI_ICON_SUCCESS] = "[ok]",
    [TUI_ICON_ERROR] = "[x]",
    [TUI_ICON_WARNING] = "[!]",
    [TUI_ICON_INFO] = "(i)",
    [TUI_ICON_PROGRESS] = "...",
    [TUI_ICON_ARROW] = "->",
    [TUI_ICON_STAR] = "*",
    [TUI_ICON_SPARKLE] = "*",
    [TUI_ICON_ROCKET] = "^",
    [TUI_ICON_PACKAGE] = "[pkg]",
    [TUI_ICON_GEAR] = "[cfg]",
    [TUI_ICON_FLAME] = "[~]",
    [TUI_ICON_CHECK] = "[v]",
    [TUI_ICON_DOWNLOAD] = "DL",
    [TUI_ICON_BUILD] = "[build]",
    [TUI_ICON_INSTALL] = "[inst]"
};

static const char *VIBRANT_SPINNER_FRAMES[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"
};

static const char *ASCII_SPINNER_FRAMES[] = {
    "-", "\\", "|", "/"
};

static const TuiStyleDefinition STYLE_DEFS[] = {
    {
        .name = "vibrant",
        .aliases = {"colorful", "default", NULL, NULL},
        .colors = VIBRANT_COLORS,
        .icons = VIBRANT_ICONS,
        .spinner_frames = VIBRANT_SPINNER_FRAMES,
        .spinner_count = sizeof(VIBRANT_SPINNER_FRAMES) / sizeof(VIBRANT_SPINNER_FRAMES[0]),
        .colors_enabled_default = true
    },
    {
        .name = "plain",
        .aliases = {"mono", "ascii", "minimal", NULL},
        .colors = VIBRANT_COLORS,
        .icons = PLAIN_ICONS,
        .spinner_frames = ASCII_SPINNER_FRAMES,
        .spinner_count = sizeof(ASCII_SPINNER_FRAMES) / sizeof(ASCII_SPINNER_FRAMES[0]),
        .colors_enabled_default = false
    }
};

static struct {
    const TuiStyleDefinition *def;
    bool initialized;
    bool colors_enabled;
} style_state = {0};

static bool equals_ignore_case(const char *a, const char *b) {
    if (!a || !b) return false;
    return strcasecmp(a, b) == 0;
}

static const TuiStyleDefinition* find_style(const char *name) {
    if (!name || !*name) {
        return &STYLE_DEFS[0];
    }

    size_t count = sizeof(STYLE_DEFS) / sizeof(STYLE_DEFS[0]);
    for (size_t i = 0; i < count; i++) {
        const TuiStyleDefinition *def = &STYLE_DEFS[i];
        if (equals_ignore_case(name, def->name)) {
            return def;
        }
        for (size_t alias = 0; alias < sizeof(def->aliases) / sizeof(def->aliases[0]); alias++) {
            if (def->aliases[alias] && equals_ignore_case(name, def->aliases[alias])) {
                return def;
            }
        }
    }
    return NULL;
}

static void apply_style(const TuiStyleDefinition *def) {
    style_state.def = def ? def : &STYLE_DEFS[0];
    style_state.colors_enabled = style_state.def->colors_enabled_default;

    const char *no_color = getenv("NO_COLOR");
    if (no_color && *no_color) {
        style_state.colors_enabled = false;
    }

    const char *force_color = getenv("TSI_FORCE_COLOR");
    if (force_color && *force_color) {
        style_state.colors_enabled = true;
    }

    style_state.initialized = true;
}

static void ensure_initialized(void) {
    if (style_state.initialized) {
        return;
    }

    const char *env_style = getenv("TSI_TUI_STYLE");
    if (!env_style || !*env_style) {
        env_style = getenv("TSI_STYLE");
    }

    const TuiStyleDefinition *def = find_style(env_style);
    apply_style(def ? def : &STYLE_DEFS[0]);
}

void tui_style_reload_from_env(void) {
    style_state.initialized = false;
    style_state.def = NULL;
    ensure_initialized();
}

bool tui_style_apply(const char *name) {
    const TuiStyleDefinition *def = find_style(name);
    if (!def) {
        return false;
    }
    apply_style(def);
    return true;
}

const char *tui_style_active_name(void) {
    ensure_initialized();
    return style_state.def ? style_state.def->name : STYLE_DEFS[0].name;
}

const char *tui_style_color(TuiColorRole role) {
    ensure_initialized();
    if (role < 0 || role >= TUI_COLOR_COUNT) {
        return "";
    }
    if (!style_state.colors_enabled) {
        return "";
    }
    const char *value = style_state.def->colors[role];
    return value ? value : "";
}

const char *tui_style_icon(TuiIconRole role) {
    ensure_initialized();
    if (role < 0 || role >= TUI_ICON_COUNT) {
        return "";
    }
    const char *value = style_state.def->icons[role];
    if (!value || !*value) {
        value = STYLE_DEFS[0].icons[role];
    }
    return value ? value : "";
}

const char *tui_style_spinner_frame(size_t idx) {
    ensure_initialized();
    if (!style_state.def || style_state.def->spinner_count == 0) {
        return "";
    }
    size_t frame_idx = idx % style_state.def->spinner_count;
    return style_state.def->spinner_frames[frame_idx];
}

size_t tui_style_spinner_frame_count(void) {
    ensure_initialized();
    return style_state.def ? style_state.def->spinner_count : 0;
}

bool tui_style_colors_enabled(void) {
    ensure_initialized();
    return style_state.colors_enabled;
}

void tui_style_set_colors_enabled(bool enabled) {
    ensure_initialized();
    style_state.colors_enabled = enabled;
}
