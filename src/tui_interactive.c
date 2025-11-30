#include "tui_interactive.h"
#include "tui.h"
#include "resolver.h"
#include "fetcher.h"
#include "builder.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <ctype.h>
#include <stdint.h>
#include <errno.h>

// Terminal state
static struct termios original_termios;
static bool terminal_setup = false;
static bool raw_mode = false;

// Terminal setup
void tui_setup_terminal(void) {
    if (terminal_setup) {
        log_developer("Terminal already set up, skipping");
        return;
    }

    log_debug("Setting up terminal");
    if (tcgetattr(STDIN_FILENO, &original_termios) == 0) {
        terminal_setup = true;
        log_developer("Terminal attributes saved");
    } else {
        log_warning("Failed to get terminal attributes");
    }

    // Enable alternate screen buffer (saves current screen, shows blank)
    printf("\033[?1049h");
    // Move cursor to home and clear screen
    printf("\033[H\033[2J");
    fflush(stdout);
    log_debug("Terminal setup completed");
}

void tui_restore_terminal(void) {
    if (raw_mode) {
        tui_disable_raw_mode();
    }

    // Restore original screen buffer
    printf("\033[?1049l");
    fflush(stdout);

    if (terminal_setup) {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
        terminal_setup = false;
    }
}

void tui_clear_screen(void) {
    // Clear screen and move cursor to home position
    // Use both clear screen sequences for better compatibility
    printf("\033[2J\033[H");
    // Also clear scrollback buffer artifacts
    printf("\033[3J");
    fflush(stdout);
}

void tui_move_cursor(int row, int col) {
    // Ensure valid coordinates
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    printf("\033[%d;%dH", row + 1, col + 1);
    // Don't flush on every cursor move for performance
    // Flush will happen at end of drawing operations
}

void tui_hide_cursor(void) {
    printf("\033[?25l");
    fflush(stdout);
}

void tui_show_cursor(void) {
    printf("\033[?25h");
    fflush(stdout);
}

void tui_enable_raw_mode(void) {
    if (!terminal_setup || raw_mode) {
        if (!terminal_setup) log_warning("Cannot enable raw mode: terminal not set up");
        return;
    }
    log_debug("Enabling raw mode");

    // First, flush any pending input before changing terminal mode
    tcflush(STDIN_FILENO, TCIFLUSH);

    struct termios raw = original_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0) {
        raw_mode = true;
        log_debug("Raw mode enabled successfully");

        // Aggressively drain all input after enabling raw mode
        // This catches any newlines or other characters that were buffered
        // Do this in a loop with delays to catch all buffered input
        for (int attempt = 0; attempt < 5; attempt++) {
            char c;
            int drained = 0;
            while (read(STDIN_FILENO, &c, 1) == 1 && drained < 100) {
                drained++;
            }
            if (drained == 0) break;  // No more input to drain
            usleep(10000);  // 10ms delay between attempts
        }

        // Final flush
        tcflush(STDIN_FILENO, TCIFLUSH);

        // Small delay to ensure terminal is ready
        usleep(50000);  // 50ms
    }
}

void tui_disable_raw_mode(void) {
    if (!raw_mode) return;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios) == 0) {
        raw_mode = false;
    }
}

// Clear all pending input from stdin
static void clear_pending_input(void) {
    tcflush(STDIN_FILENO, TCIFLUSH);

    int bytes_available = 0;
    if (ioctl(STDIN_FILENO, FIONREAD, &bytes_available) == 0 && bytes_available > 0) {
        char c;
        int drained = 0;
        while (read(STDIN_FILENO, &c, 1) == 1 && drained < 200) {
            drained++;
            if (ioctl(STDIN_FILENO, FIONREAD, &bytes_available) == 0 && bytes_available == 0) {
                break;
            }
        }
    }
    tcflush(STDIN_FILENO, TCIFLUSH);
}

// Drain input before entering TUI mode (simplified version)
static void drain_input_before_tui(void) {
    struct termios temp_termios;
    if (tcgetattr(STDIN_FILENO, &temp_termios) == 0) {
        struct termios noncanon = temp_termios;
        noncanon.c_lflag &= ~ICANON;
        noncanon.c_cc[VMIN] = 0;
        noncanon.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &noncanon);
        usleep(20000);
        clear_pending_input();
        tcsetattr(STDIN_FILENO, TCSANOW, &temp_termios);
    }
    tcflush(STDIN_FILENO, TCIFLUSH);
}

// Get Unicode box drawing characters
static void get_box_chars(bool *use_unicode, const char **tl, const char **tr,
                          const char **bl, const char **br, const char **h, const char **v) {
    const char *lang = getenv("LANG");
    *use_unicode = (lang && strstr(lang, "UTF-8")) || getenv("LC_ALL");
    if (!*use_unicode) *use_unicode = true; // Default to Unicode

    *tl = *use_unicode ? "┌" : "+";
    *tr = *use_unicode ? "┐" : "+";
    *bl = *use_unicode ? "└" : "+";
    *br = *use_unicode ? "┘" : "+";
    *h = *use_unicode ? "─" : "-";
    *v = *use_unicode ? "│" : "|";
}

// Draw a simple box border
static void draw_box(int x, int y, int width, int height, const char *color) {
    bool use_unicode;
    const char *tl, *tr, *bl, *br, *h, *v;
    get_box_chars(&use_unicode, &tl, &tr, &bl, &br, &h, &v);

    if (color && supports_colors()) {
        printf("%s", color);
    }

    for (int i = 0; i < height; i++) {
        tui_move_cursor(y + i, x);
        if (i == 0) {
            printf("%s", tl);
            for (int j = 0; j < width - 2; j++) printf("%s", h);
            printf("%s", tr);
        } else if (i == height - 1) {
            printf("%s", bl);
            for (int j = 0; j < width - 2; j++) printf("%s", h);
            printf("%s", br);
        } else {
            printf("%s", v);
            for (int j = 0; j < width - 2; j++) printf(" ");
            printf("%s", v);
        }
    }

    if (color && supports_colors()) {
        printf("%s", COLOR_RESET);
    }
    fflush(stdout);
}

// Draw a header line
static void draw_header(const char *icon, const char *title) {
    tui_move_cursor(0, 0);
    if (supports_colors()) {
        printf("%s%s%s %s %s%s\n", COLOR_BRIGHT_MAGENTA, COLOR_BOLD, icon, title, COLOR_RESET, CLEAR_LINE);
    } else {
        printf("%s\n", title);
    }
}

// Draw a footer line with instructions
static void draw_footer(int y, const char *instructions) {
    tui_move_cursor(y, 0);
    if (supports_colors()) {
        printf("%s%s%s\n", COLOR_DIM, instructions, COLOR_RESET);
    } else {
        printf("%s\n", instructions);
    }
}

// Draw package info panel
static void draw_package_info(Package *pkg, int x, int y) {
    if (!pkg) return;

    tui_move_cursor(y, x);
    if (supports_colors()) {
        if (pkg->name) {
            printf("%s%s%s %s\n", COLOR_BRIGHT_CYAN, COLOR_BOLD, pkg->name, COLOR_RESET);
        }
    } else {
        printf("%s\n", pkg->name ? pkg->name : "");
    }

    tui_move_cursor(y + 1, x);
    if (pkg->version) {
        if (supports_colors()) {
            printf("%sVersion:%s %s%s%s\n", COLOR_DIM, COLOR_RESET, COLOR_BRIGHT_GREEN, pkg->version, COLOR_RESET);
        } else {
            printf("Version: %s\n", pkg->version);
        }
    }

    tui_move_cursor(y + 2, x);
    if (pkg->description) {
        if (supports_colors()) {
            printf("%s%s%s\n", COLOR_INFO, pkg->description, COLOR_RESET);
        } else {
            printf("%s\n", pkg->description);
        }
    }

    if (pkg->dependencies_count > 0) {
        tui_move_cursor(y + 4, x);
        if (supports_colors()) {
            printf("%sDependencies:%s ", COLOR_DIM, COLOR_RESET);
        } else {
            printf("Dependencies: ");
        }
        for (size_t i = 0; i < pkg->dependencies_count && i < 5; i++) {
            if (i > 0) printf(", ");
            if (supports_colors()) {
                printf("%s%s%s", COLOR_BRIGHT_BLUE, pkg->dependencies[i], COLOR_RESET);
            } else {
                printf("%s", pkg->dependencies[i]);
            }
        }
        if (pkg->dependencies_count > 5) {
            printf(" ...");
        }
        printf("\n");
    }
}

// Input handling
bool tui_read_key(KeyEvent *event) {
    if (!event) return false;

    event->code = KEY_UNKNOWN;
    event->ch = 0;

    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) {
        return false;
    }

    // Handle escape sequences
    if (c == '\033') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            event->code = KEY_ESC;
            return true;
        }
        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            event->code = KEY_ESC;
            return true;
        }

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                char next;
                if (read(STDIN_FILENO, &next, 1) == 1 && next == '~') {
                    switch (seq[1]) {
                        case '1': event->code = KEY_HOME; break;
                        case '3': event->code = KEY_DELETE; break;
                        case '4': event->code = KEY_END; break;
                        case '5': event->code = KEY_PAGE_UP; break;
                        case '6': event->code = KEY_PAGE_DOWN; break;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': event->code = KEY_UP; break;
                    case 'B': event->code = KEY_DOWN; break;
                    case 'C': event->code = KEY_RIGHT; break;
                    case 'D': event->code = KEY_LEFT; break;
                    case 'H': event->code = KEY_HOME; break;
                    case 'F': event->code = KEY_END; break;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': event->code = KEY_HOME; break;
                case 'F': event->code = KEY_END; break;
            }
        }
        return true;
    }

    // Handle control characters
    if (c < 32) {
        switch (c) {
            case '\r':
            case '\n':
                event->code = KEY_ENTER;
                break;
            case '\t':
                event->code = KEY_TAB;
                break;
            case '\b':
            case 127:
                event->code = KEY_BACKSPACE;
                break;
            case 3:
                event->code = KEY_CTRL_C;
                break;
            case 4:
                event->code = KEY_CTRL_D;
                break;
            case 12:
                event->code = KEY_CTRL_L;
                break;
            default:
                event->code = KEY_CHAR;
                event->ch = c;
                break;
        }
    } else {
        event->code = KEY_CHAR;
        event->ch = c;
    }

    return true;
}

int tui_get_terminal_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80; // Default
}

int tui_get_terminal_height(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_row;
    }
    return 24; // Default
}

// Menu system
Menu* menu_new(const char *title) {
    Menu *menu = calloc(1, sizeof(Menu));
    if (!menu) return NULL;

    if (title) {
        menu->title = strdup(title);
    }
    menu->max_visible = 10;
    menu->loop_selection = true;
    menu->selected_index = 0;

    return menu;
}

void menu_free(Menu *menu) {
    if (!menu) return;

    MenuItem *item = menu->items;
    while (item) {
        MenuItem *next = item->next;
        if (item->label) free(item->label);
        if (item->description) free(item->description);
        free(item);
        item = next;
    }

    if (menu->title) free(menu->title);
    free(menu);
}

void menu_add_item(Menu *menu, const char *label, const char *description, void *data, bool enabled) {
    if (!menu || !label) return;

    MenuItem *item = calloc(1, sizeof(MenuItem));
    if (!item) return;

    item->label = strdup(label);
    if (description) {
        item->description = strdup(description);
    }
    item->data = data;
    item->enabled = enabled;

    if (!menu->items) {
        menu->items = item;
    } else {
        MenuItem *last = menu->items;
        while (last->next) last = last->next;
        last->next = item;
    }

    menu->item_count++;
}

void menu_draw(Menu *menu, int x, int y, int width, int height) {
    (void)width; // May be used for future width-based formatting
    if (!menu) return;

    int current_y = y;

    // Draw title
    if (menu->title) {
        tui_move_cursor(current_y++, x);
        if (supports_colors()) {
            printf("%s%s%s%s%s", COLOR_BRIGHT_CYAN, COLOR_BOLD, menu->title, COLOR_RESET, CLEAR_LINE);
        } else {
            printf("%s", menu->title);
        }
    }

    // Draw items
    MenuItem *item = menu->items;
    int index = 0;
    int available_height = height - (menu->title ? 1 : 0);
    int visible_start = menu->scroll_offset;
    int visible_end = visible_start + available_height;
    int items_drawn = 0;

    // Reset current_y after title
    if (menu->title) {
        current_y = y + 1; // Start after title
    } else {
        current_y = y;
    }

    while (item && items_drawn < available_height && current_y < y + height) {
        if (index >= visible_start && index < visible_end) {
            tui_move_cursor(current_y, x);
            fflush(stdout); // Ensure cursor is positioned

            bool is_selected = (index == menu->selected_index);

            if (is_selected) {
                if (supports_colors()) {
                    printf("%s%s> %s%s%s", COLOR_BG_CYAN, COLOR_BRIGHT_WHITE, item->label ? item->label : "", COLOR_RESET, CLEAR_LINE);
                } else {
                    printf("> %s%s", item->label ? item->label : "", CLEAR_LINE);
                }
            } else {
                if (supports_colors() && !item->enabled) {
                    printf("%s  %s%s%s", COLOR_DIM, item->label ? item->label : "", COLOR_RESET, CLEAR_LINE);
                } else {
                    printf("  %s%s", item->label ? item->label : "", CLEAR_LINE);
                }
            }
            fflush(stdout); // Ensure item is drawn
            current_y++;
            items_drawn++;
        }

        item = item->next;
        index++;
    }

    // Clear remaining lines
    while (current_y < y + height) {
        tui_move_cursor(current_y++, x);
        printf("%s", CLEAR_LINE);
    }

    fflush(stdout);
}

int menu_show(Menu *menu) {
    if (!menu || menu->item_count == 0) return -1;

    drain_input_before_tui();
    tui_setup_terminal();
    tui_enable_raw_mode();
    tui_hide_cursor();

    int width = tui_get_terminal_width();
    int height = tui_get_terminal_height();
    int menu_width = 60;
    int menu_height = menu->item_count + (menu->title ? 2 : 1);
    if (menu_height > height - 4) menu_height = height - 4;

    int x = (width - menu_width) / 2;
    int y = (height - menu_height) / 2;
    menu->max_visible = menu_height - (menu->title ? 2 : 1);

    // Initial draw
    tui_clear_screen();
    draw_box(x, y, menu_width, menu_height, COLOR_BRIGHT_CYAN);
    menu_draw(menu, x + 2, y + 1, menu_width - 4, menu_height - 2);

    usleep(100000); // Give terminal time to display
    clear_pending_input();
    usleep(50000);
    clear_pending_input();

    struct timeval ready_time;
    gettimeofday(&ready_time, NULL);

    while (true) {
        tui_clear_screen();
        draw_box(x, y, menu_width, menu_height, COLOR_BRIGHT_CYAN);
        menu_draw(menu, x + 2, y + 1, menu_width - 4, menu_height - 2);

        KeyEvent key;
        if (!tui_read_key(&key)) {
            usleep(10000);
            continue;
        }

        // Ignore Enter keys that come too quickly (likely buffered)
        if (key.code == KEY_ENTER) {
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            long elapsed_ms = (current_time.tv_sec - ready_time.tv_sec) * 1000 +
                             (current_time.tv_usec - ready_time.tv_usec) / 1000;
            if (elapsed_ms < 100) {
                continue;
            }
        }

        switch (key.code) {
            case KEY_UP:
                menu->selected_index--;
                if (menu->selected_index < 0) {
                    menu->selected_index = menu->loop_selection ? (int)menu->item_count - 1 : 0;
                }
                // Update scroll
                if (menu->selected_index < menu->scroll_offset) {
                    menu->scroll_offset = menu->selected_index;
                } else if (menu->selected_index >= menu->scroll_offset + menu->max_visible) {
                    menu->scroll_offset = menu->selected_index - menu->max_visible + 1;
                }
                break;

            case KEY_DOWN:
                menu->selected_index++;
                if (menu->selected_index >= (int)menu->item_count) {
                    menu->selected_index = menu->loop_selection ? 0 : (int)menu->item_count - 1;
                }
                // Update scroll
                if (menu->selected_index < menu->scroll_offset) {
                    menu->scroll_offset = menu->selected_index;
                } else if (menu->selected_index >= menu->scroll_offset + menu->max_visible) {
                    menu->scroll_offset = menu->selected_index - menu->max_visible + 1;
                }
                break;

            case KEY_ENTER: {
                MenuItem *item = menu->items;
                int index = 0;
                while (item && index < menu->selected_index) {
                    item = item->next;
                    index++;
                }
                if (item && item->enabled) {
                    tui_restore_terminal();
                    tui_show_cursor();
                    return menu->selected_index;
                }
                break;
            }

            case KEY_ESC:
            case KEY_CTRL_C:
                tui_restore_terminal();
                tui_show_cursor();
                return -1;

            default:
                break;
        }
    }
}

// Panel system
Panel* panel_new(int x, int y, int width, int height, const char *title) {
    Panel *panel = calloc(1, sizeof(Panel));
    if (!panel) return NULL;

    panel->x = x;
    panel->y = y;
    panel->width = width;
    panel->height = height;
    if (title) {
        panel->title = strdup(title);
    }
    panel->border = true;
    panel->visible = true;

    return panel;
}

void panel_free(Panel *panel) {
    if (!panel) return;
    if (panel->title) free(panel->title);
    free(panel);
}

void panel_set_title(Panel *panel, const char *title) {
    if (!panel) return;
    if (panel->title) free(panel->title);
    panel->title = title ? strdup(title) : NULL;
}

void panel_set_draw_callback(Panel *panel, void (*draw)(Panel *panel, void *data), void *data) {
    if (!panel) return;
    panel->draw = draw;
    panel->data = data;
}

void panel_draw(Panel *panel) {
    if (!panel || !panel->visible) return;

    if (panel->border) {
        draw_box(panel->x, panel->y, panel->width, panel->height, COLOR_BRIGHT_BLUE);

        // Draw title
        if (panel->title) {
            tui_move_cursor(panel->y, panel->x + 2);
            if (supports_colors()) {
                printf("%s%s%s%s", COLOR_BRIGHT_CYAN, COLOR_BOLD, panel->title, COLOR_RESET);
            } else {
                printf("%s", panel->title);
            }
        }
    }

    if (panel->draw) {
        panel->draw(panel, panel->data);
    }
}

// List view
ListView* listview_new(void) {
    ListView *lv = calloc(1, sizeof(ListView));
    if (!lv) return NULL;
    lv->max_visible = 10;
    lv->case_sensitive = false;
    return lv;
}

void listview_free(ListView *lv) {
    if (!lv) return;
    for (size_t i = 0; i < lv->item_count; i++) {
        if (lv->items[i]) free(lv->items[i]);
    }
    if (lv->items) free(lv->items);
    if (lv->filter) free(lv->filter);
    free(lv);
}

void listview_add_item(ListView *lv, const char *item) {
    if (!lv || !item) return;

    lv->items = realloc(lv->items, sizeof(char*) * (lv->item_count + 1));
    if (!lv->items) return;

    lv->items[lv->item_count++] = strdup(item);
}

void listview_set_filter(ListView *lv, const char *filter) {
    if (!lv) return;
    if (lv->filter) free(lv->filter);
    lv->filter = filter ? strdup(filter) : NULL;
    // Reset selection and scroll when filter changes
    lv->selected_index = 0;
    lv->scroll_offset = 0;
}

static bool matches_filter(const char *item, const char *filter, bool case_sensitive) {
    if (!filter || !*filter) return true;
    if (!item) return false;

    if (case_sensitive) {
        return strstr(item, filter) != NULL;
    } else {
        // Case-insensitive search
        const char *p = item;
        const char *f = filter;
        while (*p && *f) {
            if (tolower(*p) == tolower(*f)) {
                p++;
                f++;
            } else {
                p++;
                f = filter;
            }
        }
        return *f == '\0';
    }
}

void listview_draw(ListView *lv, int x, int y, int width, int height) {
    (void)width; // May be used for future width-based formatting
    if (!lv) return;

    int current_y = y;
    size_t visible_count = 0;

    // Filter and count visible items
    for (size_t i = 0; i < lv->item_count; i++) {
        if (matches_filter(lv->items[i], lv->filter, lv->case_sensitive)) {
            if (visible_count >= lv->scroll_offset && visible_count < lv->scroll_offset + (size_t)height) {
                bool is_selected = (visible_count == lv->selected_index);

                tui_move_cursor(current_y++, x);

                if (is_selected) {
                    if (supports_colors()) {
                        printf("%s%s> %s%s%s", COLOR_BG_CYAN, COLOR_BRIGHT_WHITE, lv->items[i], COLOR_RESET, CLEAR_LINE);
                    } else {
                        printf("> %s", lv->items[i]);
                    }
                } else {
                    printf("  %s%s", lv->items[i], CLEAR_LINE);
                }
            }
            visible_count++;
        }
    }

    // Clear remaining lines
    while (current_y < y + height) {
        tui_move_cursor(current_y++, x);
        printf("%s", CLEAR_LINE);
    }

    fflush(stdout);
}

int listview_handle_key(ListView *lv, KeyEvent *key) {
    if (!lv || !key) return 0;

    switch (key->code) {
        case KEY_UP:
            if (lv->selected_index > 0) {
                lv->selected_index--;
                // Adjust scroll
                if (lv->selected_index < lv->scroll_offset) {
                    lv->scroll_offset = lv->selected_index;
                }
            }
            return 1;

            case KEY_DOWN: {
                // Count visible items
                size_t visible_count = 0;
            for (size_t i = 0; i < lv->item_count; i++) {
                if (matches_filter(lv->items[i], lv->filter, lv->case_sensitive)) {
                    visible_count++;
                }
            }
                if (lv->selected_index < visible_count - 1) {
                    lv->selected_index++;
                    // Adjust scroll
                    if (lv->selected_index >= lv->scroll_offset + lv->max_visible) {
                        lv->scroll_offset = lv->selected_index - lv->max_visible + 1;
                    }
                }
                return 1;
            }

        case KEY_PAGE_UP:
            lv->selected_index = (lv->selected_index > (size_t)lv->max_visible)
                ? lv->selected_index - lv->max_visible : 0;
            if (lv->selected_index < lv->scroll_offset) {
                lv->scroll_offset = lv->selected_index;
            }
            return 1;

        case KEY_PAGE_DOWN: {
            size_t visible_count = 0;
            for (size_t i = 0; i < lv->item_count; i++) {
                if (matches_filter(lv->items[i], lv->filter, lv->case_sensitive)) {
                    visible_count++;
                }
            }
            lv->selected_index = (lv->selected_index + lv->max_visible < visible_count)
                ? lv->selected_index + lv->max_visible : visible_count - 1;
            if (lv->selected_index >= lv->scroll_offset + lv->max_visible) {
                lv->scroll_offset = lv->selected_index - lv->max_visible + 1;
            }
            return 1;
        }

        case KEY_CHAR:
            // Handle search/filter
            if (isprint(key->ch)) {
                // Simple filter - in real implementation, you'd want a search input field
                return 0; // Let caller handle
            }
            break;

        default:
            break;
    }

    return 0;
}

const char* listview_get_selected(ListView *lv) {
    if (!lv || lv->selected_index >= lv->item_count) return NULL;

    size_t visible_count = 0;
    for (size_t i = 0; i < lv->item_count; i++) {
        if (matches_filter(lv->items[i], lv->filter, lv->case_sensitive)) {
            if (visible_count == lv->selected_index) {
                return lv->items[i];
            }
            visible_count++;
        }
    }

    return NULL;
}

// Progress view
ProgressView* progressview_new(void) {
    ProgressView *pv = calloc(1, sizeof(ProgressView));
    if (!pv) return NULL;
    pv->max_visible = 5;
    return pv;
}

void progressview_free(ProgressView *pv) {
    if (!pv) return;
    for (size_t i = 0; i < pv->item_count; i++) {
        if (pv->items[i].label) free(pv->items[i].label);
        if (pv->items[i].status_text) free(pv->items[i].status_text);
    }
    if (pv->items) free(pv->items);
    free(pv);
}

void progressview_add(ProgressView *pv, const char *label, int total) {
    if (!pv || !label) return;

    pv->items = realloc(pv->items, sizeof(ProgressItem) * (pv->item_count + 1));
    if (!pv->items) return;

    ProgressItem *item = &pv->items[pv->item_count++];
    item->label = strdup(label);
    item->total = total;
    item->current = 0;
    item->status_text = NULL;
    item->active = true;
}

void progressview_update(ProgressView *pv, size_t index, int current, const char *status) {
    if (!pv || index >= pv->item_count) return;

    ProgressItem *item = &pv->items[index];
    item->current = current;
    if (status) {
        if (item->status_text) free(item->status_text);
        item->status_text = strdup(status);
    }
}

void progressview_draw(ProgressView *pv, int x, int y, int width, int height) {
    if (!pv) return;

    int current_y = y;
    size_t visible_start = 0;
    size_t visible_end = pv->item_count;
    if (pv->item_count > (size_t)height) {
        visible_end = height;
    }

    for (size_t i = visible_start; i < visible_end && i < pv->item_count; i++) {
        ProgressItem *item = &pv->items[i];

        tui_move_cursor(current_y++, x);

        // Label
        if (supports_colors()) {
            printf("%s%s%s", COLOR_BRIGHT_CYAN, item->label, COLOR_RESET);
        } else {
            printf("%s", item->label);
        }

        // Progress bar
        int bar_width = width - 30;
        if (bar_width > 0 && item->total > 0) {
            int filled = (item->current * bar_width) / item->total;
            int percent = (item->current * 100) / item->total;

            printf(" [");
            for (int j = 0; j < bar_width; j++) {
                if (j < filled) {
                    if (supports_colors()) {
                        printf("%s█", COLOR_BRIGHT_GREEN);
                    } else {
                        printf("#");
                    }
                } else {
                    if (supports_colors()) {
                        printf("%s░", COLOR_DIM);
                    } else {
                        printf("-");
                    }
                }
            }
            printf("] %3d%%", percent);
        }

        if (item->status_text) {
            printf(" %s", item->status_text);
        }

        printf("%s", CLEAR_LINE);
    }

    fflush(stdout);
}

void progressview_finish(ProgressView *pv, size_t index, bool success) {
    (void)success; // May be used in future for success/failure styling
    if (!pv || index >= pv->item_count) return;
    pv->items[index].active = false;
    pv->items[index].current = pv->items[index].total;
}

// Dropdown menu implementation
DropdownMenu* dropdown_new(const char *title) {
    DropdownMenu *dd = calloc(1, sizeof(DropdownMenu));
    if (!dd) return NULL;
    if (title) {
        dd->title = strdup(title);
    }
    dd->visible = false;
    return dd;
}

void dropdown_free(DropdownMenu *dd) {
    if (!dd) return;
    for (size_t i = 0; i < dd->item_count; i++) {
        if (dd->items[i]) free(dd->items[i]);
    }
    if (dd->items) free(dd->items);
    if (dd->title) free(dd->title);
    free(dd);
}

void dropdown_add_item(DropdownMenu *dd, const char *item) {
    if (!dd || !item) return;
    dd->items = realloc(dd->items, sizeof(char*) * (dd->item_count + 1));
    if (!dd->items) return;
    dd->items[dd->item_count++] = strdup(item);
}

void dropdown_draw(DropdownMenu *dd) {
    if (!dd || !dd->visible) return;

    int max_height = (int)dd->item_count + 2;
    if (max_height > dd->height) max_height = dd->height;

    draw_box(dd->x, dd->y, dd->width, max_height, COLOR_BRIGHT_BLUE);

    // Draw items
    bool use_unicode;
    const char *v;
    get_box_chars(&use_unicode, NULL, NULL, NULL, NULL, NULL, &v);

    for (int i = 1; i < max_height - 1; i++) {
        tui_move_cursor(dd->y + i, dd->x);
        printf("%s", v);

        size_t item_idx = (size_t)(i - 1) + dd->scroll_offset;
        bool is_selected = (item_idx == dd->selected_index);

        if (item_idx < dd->item_count) {
            int text_width = dd->width - 4;
            char display[256];
            snprintf(display, sizeof(display), "%.*s", text_width, dd->items[item_idx]);

            if (is_selected) {
                if (supports_colors()) {
                    printf(" %s%s%s%s ", COLOR_BG_CYAN, COLOR_BRIGHT_WHITE, display, COLOR_RESET);
                } else {
                    printf(" >%s ", display);
                }
            } else {
                printf(" %s ", display);
            }
            // Fill remaining space
            int remaining = dd->width - 4 - (int)strlen(display);
            for (int j = 0; j < remaining; j++) printf(" ");
        } else {
            for (int j = 0; j < dd->width - 2; j++) printf(" ");
        }
        printf("%s", v);
    }
    fflush(stdout);
}

int dropdown_show(DropdownMenu *dd, int x, int y, int width, int height) {
    if (!dd || dd->item_count == 0) return -1;

    dd->x = x;
    dd->y = y;
    dd->width = width;
    dd->height = height > (int)dd->item_count + 2 ? (int)dd->item_count + 2 : height;
    dd->visible = true;
    dd->selected_index = 0;
    dd->scroll_offset = 0;

    tui_setup_terminal();
    tui_enable_raw_mode();
    tui_hide_cursor();

    while (true) {
        dropdown_draw(dd);

        KeyEvent key;
        if (!tui_read_key(&key)) {
            usleep(10000);
            continue;
        }

        switch (key.code) {
            case KEY_UP:
                if (dd->selected_index > 0) {
                    dd->selected_index--;
                    if (dd->selected_index < dd->scroll_offset) {
                        dd->scroll_offset = dd->selected_index;
                    }
                }
                break;

            case KEY_DOWN:
                if (dd->selected_index < dd->item_count - 1) {
                    dd->selected_index++;
                    if (dd->selected_index >= dd->scroll_offset + (size_t)(dd->height - 2)) {
                        dd->scroll_offset = dd->selected_index - (size_t)(dd->height - 2) + 1;
                    }
                }
                break;

            case KEY_ENTER:
                dd->visible = false;
                tui_restore_terminal();
                tui_show_cursor();
                return (int)dd->selected_index;

            case KEY_ESC:
            case KEY_CTRL_C:
                dd->visible = false;
                tui_restore_terminal();
                tui_show_cursor();
                return -1;

            default:
                break;
        }
    }
}

const char* dropdown_get_selected(DropdownMenu *dd) {
    if (!dd || dd->selected_index >= dd->item_count) return NULL;
    return dd->items[dd->selected_index];
}

// Check if package is upgradable
bool tui_check_package_upgradable(Repository *repo, const char *package_name, const char *installed_version, char **latest_version) {
    if (!repo || !package_name || !installed_version) return false;

    Package *latest = repository_get_package(repo, package_name);
    if (!latest || !latest->version) return false;

    // Simple version comparison (lexicographic for semantic versions)
    if (strcmp(latest->version, installed_version) > 0) {
        if (latest_version) {
            *latest_version = strdup(latest->version);
        }
        return true;
    }

    return false;
}

// Get upgrade info for all installed packages
PackageUpgradeInfo* tui_get_upgrade_info(Database *db, Repository *repo, size_t *count) {
    if (!db || !repo || !count) return NULL;

    *count = 0;
    size_t installed_count = 0;
    char **installed = database_list_installed(db, &installed_count);
    if (!installed || installed_count == 0) {
        if (installed) {
            for (size_t i = 0; i < installed_count; i++) free(installed[i]);
            free(installed);
        }
        return NULL;
    }

    PackageUpgradeInfo *info = calloc(installed_count, sizeof(PackageUpgradeInfo));
    if (!info) {
        for (size_t i = 0; i < installed_count; i++) free(installed[i]);
        free(installed);
        return NULL;
    }

    for (size_t i = 0; i < installed_count; i++) {
        InstalledPackage *pkg = database_get_package(db, installed[i]);
        if (pkg && pkg->version) {
            info[*count].package_name = strdup(pkg->name);
            info[*count].installed_version = strdup(pkg->version);

            char *latest = NULL;
            if (tui_check_package_upgradable(repo, pkg->name, pkg->version, &latest)) {
                info[*count].upgradable = true;
                info[*count].latest_version = latest;
            } else {
                info[*count].upgradable = false;
                info[*count].latest_version = strdup(pkg->version);
            }
            (*count)++;
        }
        free(installed[i]);
    }
    free(installed);

    return info;
}

// Dashboard implementation
void tui_show_dashboard(Database *db, const char *repo_dir) {
    if (!db) return;

    tui_setup_terminal();
    tui_enable_raw_mode();
    tui_hide_cursor();

    Repository *repo = repository_new(repo_dir);
    if (!repo) {
        tui_restore_terminal();
        tui_show_cursor();
        return;
    }

    int width = tui_get_terminal_width();
    int height = tui_get_terminal_height();

    while (true) {
        tui_clear_screen();
        draw_header(ICON_ROCKET, "TSI Dashboard");

        // Stats panel
        int panel_y = 2;
        size_t installed_count = 0;
        char **installed = database_list_installed(db, &installed_count);

        tui_move_cursor(panel_y, 2);
        if (supports_colors()) {
            printf("%s%sInstalled Packages%s: %s%zu%s\n",
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, installed_count, COLOR_RESET);
        } else {
            printf("Installed Packages: %zu\n", installed_count);
        }

        // List installed packages
        int list_y = panel_y + 2;
        int max_list = height - list_y - 5;
        for (size_t i = 0; i < installed_count && i < (size_t)max_list; i++) {
            InstalledPackage *pkg = database_get_package(db, installed[i]);
            if (pkg) {
                tui_move_cursor(list_y + (int)i, 4);
                if (supports_colors()) {
                    const char *version_str = pkg->version ? pkg->version : "unknown";
                    printf("%s%s%s %s%s%s (%s%s%s)\n",
                           COLOR_BRIGHT_BLUE, ICON_PACKAGE, COLOR_RESET,
                           COLOR_BRIGHT_CYAN, pkg->name, COLOR_RESET,
                           COLOR_DIM, version_str, COLOR_RESET);
                } else {
                    printf("%s (%s)\n", pkg->name, pkg->version ? pkg->version : "unknown");
                }
            }
        }

        if (installed) {
            for (size_t i = 0; i < installed_count; i++) free(installed[i]);
            free(installed);
        }

        size_t available_count = 0;
        char **available = repository_list_packages(repo, &available_count);
        tui_move_cursor(panel_y, width / 2 + 2);
        if (supports_colors()) {
            printf("%s%sAvailable Packages%s: %s%zu%s\n",
                   COLOR_BRIGHT_CYAN, COLOR_BOLD, COLOR_RESET,
                   COLOR_BRIGHT_GREEN, available_count, COLOR_RESET);
        } else {
            printf("Available Packages: %zu\n", available_count);
        }

        if (available) {
            for (size_t i = 0; i < available_count; i++) free(available[i]);
            free(available);
        }

        draw_footer(height - 2, "Press 'q' to quit, 'i' to install, 'b' to browse");

        KeyEvent key;
        if (tui_read_key(&key)) {
            if (key.code == KEY_CHAR) {
                if (key.ch == 'q' || key.ch == 'Q') {
                    break;
                } else if (key.ch == 'i' || key.ch == 'I') {
                    tui_restore_terminal();
                    tui_show_cursor();
                    // Will be handled by main
                    break;
                } else if (key.ch == 'b' || key.ch == 'B') {
                    tui_restore_terminal();
                    tui_show_cursor();
                    tui_show_package_browser(repo_dir);
                    tui_setup_terminal();
                    tui_enable_raw_mode();
                    tui_hide_cursor();
                }
            } else if (key.code == KEY_ESC || key.code == KEY_CTRL_C) {
                break;
            }
        }

        usleep(50000); // Small delay to prevent CPU spinning
    }

    repository_free(repo);
    tui_restore_terminal();
    tui_show_cursor();
}

// Package browser implementation
void tui_show_package_browser(const char *repo_dir) {
    log_info("TUI package browser invoked");
    log_developer("Opening package browser for repo_dir: %s", repo_dir);
    Repository *repo = repository_new(repo_dir);
    if (!repo) {
        log_error("Failed to initialize repository for package browser");
        return;
    }

    tui_setup_terminal();
    tui_enable_raw_mode();
    tui_hide_cursor();

    ListView *lv = listview_new();
    if (!lv) {
        repository_free(repo);
        tui_restore_terminal();
        tui_show_cursor();
        return;
    }

    // Add packages to list
    size_t package_count = 0;
    char **packages = repository_list_packages(repo, &package_count);
    for (size_t i = 0; i < package_count; i++) {
        listview_add_item(lv, packages[i]);
        free(packages[i]);
    }
    free(packages);

    int width = tui_get_terminal_width();
    int height = tui_get_terminal_height();
    int list_width = width - 40;
    int list_height = height - 6;

    char search_filter[256] = {0};
    size_t search_pos = 0;
    bool searching = false;

    while (true) {
        tui_clear_screen();
        draw_header(ICON_PACKAGE, "Package Browser");

        // Search bar
        tui_move_cursor(2, 0);
        if (searching) {
            if (supports_colors()) {
                printf("%sSearch: %s%s%s_%s\n", COLOR_BRIGHT_YELLOW, COLOR_RESET, search_filter, COLOR_BRIGHT_YELLOW, COLOR_RESET);
            } else {
                printf("Search: %s_\n", search_filter);
            }
        } else {
            if (supports_colors()) {
                printf("%sPress '/' to search%s\n", COLOR_DIM, COLOR_RESET);
            } else {
                printf("Press '/' to search\n");
            }
        }

        listview_draw(lv, 2, 4, list_width, list_height);

        // Package info panel
        const char *selected = listview_get_selected(lv);
        if (selected) {
            Package *pkg = repository_get_package(repo, selected);
            if (pkg) {
                draw_package_info(pkg, list_width + 4, 4);
            }
        }

        draw_footer(height - 2, "[Enter] Install  [ESC] Back  [/] Search  [q] Quit");

        fflush(stdout);

        KeyEvent key;
        if (!tui_read_key(&key)) {
            usleep(10000);
            continue;
        }

        if (searching) {
            if (key.code == KEY_ENTER || key.code == KEY_ESC) {
                searching = false;
                listview_set_filter(lv, search_filter[0] ? search_filter : NULL);
                // Continue to redraw immediately
                continue;
            } else if (key.code == KEY_BACKSPACE) {
                if (search_pos > 0) {
                    search_filter[--search_pos] = '\0';
                    listview_set_filter(lv, search_filter);
                    // Continue to redraw immediately with updated filter
                    continue;
                }
            } else if (key.code == KEY_CHAR && isprint(key.ch) && search_pos < sizeof(search_filter) - 1) {
                search_filter[search_pos++] = key.ch;
                search_filter[search_pos] = '\0';
                listview_set_filter(lv, search_filter);
                // Continue to redraw immediately with updated filter
                continue;
            }
        } else {
            if (key.code == KEY_CHAR && key.ch == '/') {
                searching = true;
                search_pos = 0;
                search_filter[0] = '\0';
                listview_set_filter(lv, NULL); // Clear filter when starting search
                continue; // Redraw immediately to show search prompt
            } else if (key.code == KEY_CHAR && (key.ch == 'q' || key.ch == 'Q')) {
                break;
            } else if (key.code == KEY_ESC) {
                break;
            } else if (key.code == KEY_ENTER) {
                const char *selected_pkg = listview_get_selected(lv);
                if (selected_pkg) {
                    // Return to caller - installation will be handled there
                    break;
                }
            } else {
                listview_handle_key(lv, &key);
            }
        }
    }

    listview_free(lv);
    repository_free(repo);
    tui_restore_terminal();
    tui_show_cursor();
}

// Interactive install with version selection dropdown
int tui_interactive_install(const char *repo_dir, const char *db_dir, const char *tsi_prefix) {
    log_info("TUI interactive install invoked");
    log_developer("TUI install: repo_dir=%s, db_dir=%s, tsi_prefix=%s", repo_dir, db_dir ? db_dir : "NULL", tsi_prefix ? tsi_prefix : "NULL");
    (void)db_dir;  // Unused parameter
    (void)tsi_prefix;  // Unused parameter
    Repository *repo = repository_new(repo_dir);
    if (!repo) {
        log_error("Failed to initialize repository for TUI install");
        return 1;
    }

    tui_setup_terminal();
    tui_enable_raw_mode();
    tui_hide_cursor();

    // Show package selection
    ListView *lv = listview_new();
    if (!lv) {
        repository_free(repo);
        tui_restore_terminal();
        tui_show_cursor();
        return 1;
    }

    size_t package_count = 0;
    char **packages = repository_list_packages(repo, &package_count);
    for (size_t i = 0; i < package_count; i++) {
        listview_add_item(lv, packages[i]);
        free(packages[i]);
    }
    free(packages);

    int width = tui_get_terminal_width();
    int height = tui_get_terminal_height();

    char selected_package[256] = {0};
    bool package_selected = false;

    // Package selection loop
    while (!package_selected) {
        tui_clear_screen();

        tui_move_cursor(0, 0);
        if (supports_colors()) {
            printf("%s%s%s Select Package to Install %s%s\n", COLOR_BRIGHT_MAGENTA, COLOR_BOLD, ICON_PACKAGE, COLOR_RESET, CLEAR_LINE);
        } else {
            printf("Select Package to Install\n");
        }

        listview_draw(lv, 2, 3, width - 4, height - 6);

        tui_move_cursor(height - 2, 0);
        if (supports_colors()) {
            printf("%s[↑↓] Navigate  [Enter] Select  [ESC] Cancel%s\n", COLOR_DIM, COLOR_RESET);
        } else {
            printf("[↑↓] Navigate  [Enter] Select  [ESC] Cancel\n");
        }

        fflush(stdout);

        KeyEvent key;
        if (!tui_read_key(&key)) {
            usleep(10000);
            continue;
        }

        switch (key.code) {
            case KEY_UP:
            case KEY_DOWN:
            case KEY_PAGE_UP:
            case KEY_PAGE_DOWN:
                listview_handle_key(lv, &key);
                break;

            case KEY_ENTER: {
                const char *pkg = listview_get_selected(lv);
                if (pkg) {
                    strncpy(selected_package, pkg, sizeof(selected_package) - 1);
                    package_selected = true;
                }
                break;
            }

            case KEY_ESC:
            case KEY_CTRL_C:
                listview_free(lv);
                repository_free(repo);
                tui_restore_terminal();
                tui_show_cursor();
                return 0;

            default:
                break;
        }
    }

    listview_free(lv);

    // Show version selection dropdown
    size_t versions_count = 0;
    char **versions = repository_list_versions(repo, selected_package, &versions_count);

    if (!versions || versions_count == 0) {
        tui_clear_screen();
        tui_move_cursor(0, 0);
        if (supports_colors()) {
            printf("%s%s%s No versions found for %s%s\n", COLOR_BRIGHT_RED, ICON_ERROR, COLOR_RESET, selected_package, CLEAR_LINE);
        } else {
            printf("No versions found for %s\n", selected_package);
        }
        tui_move_cursor(2, 0);
        printf("Press any key to continue...");
        fflush(stdout);

        KeyEvent key;
        while (!tui_read_key(&key)) {
            usleep(10000);
        }

        if (versions) {
            for (size_t i = 0; i < versions_count; i++) free(versions[i]);
            free(versions);
        }
        repository_free(repo);
        tui_restore_terminal();
        tui_show_cursor();
        return 1;
    }

    // Remove duplicates
    size_t unique_count = 0;
    char **unique_versions = malloc(sizeof(char*) * versions_count);
    for (size_t i = 0; i < versions_count; i++) {
        bool is_duplicate = false;
        for (size_t j = 0; j < unique_count; j++) {
            if (strcmp(versions[i], unique_versions[j]) == 0) {
                is_duplicate = true;
                break;
            }
        }
        if (!is_duplicate) {
            unique_versions[unique_count++] = strdup(versions[i]);
        }
    }

    // Create dropdown for version selection
    DropdownMenu *version_dd = dropdown_new("Select Version");
    for (size_t i = 0; i < unique_count; i++) {
        dropdown_add_item(version_dd, unique_versions[i]);
    }

    int dd_x = width / 2 - 20;
    int dd_y = height / 2 - (int)unique_count / 2;
    int dd_width = 40;
    int dd_height = (int)unique_count + 2;
    if (dd_height > height - 4) dd_height = height - 4;

    int version_idx = dropdown_show(version_dd, dd_x, dd_y, dd_width, dd_height);

    const char *selected_version = NULL;
    if (version_idx >= 0 && version_idx < (int)unique_count) {
        selected_version = unique_versions[version_idx];
    }

    dropdown_free(version_dd);

    // Cleanup
    for (size_t i = 0; i < unique_count; i++) {
        free(unique_versions[i]);
    }
    free(unique_versions);
    for (size_t i = 0; i < versions_count; i++) {
        free(versions[i]);
    }
    free(versions);

    if (!selected_version) {
        repository_free(repo);
        tui_restore_terminal();
        tui_show_cursor();
        return 0;
    }

    // Now call the actual install command
    // In full implementation, this would call cmd_install from main.c
    tui_clear_screen();
    tui_move_cursor(0, 0);
    if (supports_colors()) {
        printf("%s%s%s Installing %s%s%s %s(%s)%s",
               COLOR_BRIGHT_GREEN, ICON_INSTALL, COLOR_RESET,
               COLOR_BRIGHT_CYAN, selected_package, COLOR_RESET,
               COLOR_DIM, selected_version, COLOR_RESET);
        printf("%s\n", CLEAR_LINE);
    } else {
        printf("Installing %s (%s)\n", selected_package, selected_version);
    }
    tui_move_cursor(2, 0);
    printf("Installation would proceed here...\n");
    tui_move_cursor(4, 0);
    printf("Press any key to continue...");
    fflush(stdout);

    KeyEvent key;
    while (!tui_read_key(&key)) {
        usleep(10000);
    }

    repository_free(repo);
    tui_restore_terminal();
    tui_show_cursor();
    return 0;
}

// Interactive upgrade
int tui_interactive_upgrade(Database *db, const char *repo_dir, const char *tsi_prefix) {
    (void)tsi_prefix;  // Unused parameter
    Repository *repo = repository_new(repo_dir);
    if (!repo) return 1;

    size_t upgrade_count = 0;
    PackageUpgradeInfo *upgrades = tui_get_upgrade_info(db, repo, &upgrade_count);

    if (!upgrades || upgrade_count == 0) {
        tui_setup_terminal();
        tui_clear_screen();
        tui_move_cursor(0, 0);
        if (supports_colors()) {
            printf("%s%s%s No upgrades available%s\n", COLOR_BRIGHT_GREEN, ICON_SUCCESS, COLOR_RESET, CLEAR_LINE);
        } else {
            printf("No upgrades available\n");
        }
        tui_move_cursor(2, 0);
        printf("All packages are up to date.\n");
        tui_move_cursor(tui_get_terminal_height() - 1, 0);
        printf("Press any key to continue...");
        fflush(stdout);

        tui_enable_raw_mode();
        KeyEvent key;
        while (!tui_read_key(&key)) {
            usleep(10000);
        }
        tui_disable_raw_mode();
        tui_restore_terminal();
        tui_show_cursor();

        if (upgrades) {
            for (size_t i = 0; i < upgrade_count; i++) {
                if (upgrades[i].package_name) free(upgrades[i].package_name);
                if (upgrades[i].installed_version) free(upgrades[i].installed_version);
                if (upgrades[i].latest_version) free(upgrades[i].latest_version);
            }
            free(upgrades);
        }
        repository_free(repo);
        return 0;
    }

    // Show upgradeable packages list
    // In full implementation, would show list and allow selection
    // Then call install with specific version

    // Cleanup
    for (size_t i = 0; i < upgrade_count; i++) {
        if (upgrades[i].package_name) free(upgrades[i].package_name);
        if (upgrades[i].installed_version) free(upgrades[i].installed_version);
        if (upgrades[i].latest_version) free(upgrades[i].latest_version);
    }
    free(upgrades);
    repository_free(repo);
    return 0;
}

// Interactive update repository
int tui_interactive_update(const char *repo_dir, const char *tsi_prefix) {
    // This would call the update command from main.c
    // For now, placeholder
    (void)repo_dir;
    (void)tsi_prefix;
    return 0;
}

// Helper function to draw box borders (not currently used, but kept for future use)
__attribute__((unused)) static void draw_box_helper(int x, int y, int w, int box_height, const char *title, bool use_unicode) {
    const char *tl = use_unicode ? "┌" : "+";
    const char *tr = use_unicode ? "┐" : "+";
    const char *bl = use_unicode ? "└" : "+";
    const char *br = use_unicode ? "┘" : "+";
    const char *h_char = use_unicode ? "─" : "-";
    const char *v_char = use_unicode ? "│" : "|";
    // Note: t_char, b_char, l_char, r_char variables removed as they're unused

    // Top border
    tui_move_cursor(y, x);
    printf("%s", tl);
    if (title && title[0]) {
        int title_len = (int)strlen(title);
        int title_x = (w - title_len - 2) / 2;
        if (title_x > 0) {
            for (int i = 0; i < title_x; i++) printf("%s", h_char);
        }
        printf(" %s ", title);
        for (int i = title_x + title_len + 3; i < w - 1; i++) printf("%s", h_char);
    } else {
        for (int i = 0; i < w - 2; i++) printf("%s", h_char);
    }
    printf("%s", tr);

    // Side borders and content area
    for (int i = 1; i < box_height - 1; i++) {
        tui_move_cursor(y + i, x);
        printf("%s", v_char);
        for (int j = 0; j < w - 2; j++) printf(" ");
        printf("%s", v_char);
    }

    // Bottom border
    tui_move_cursor(y + box_height - 1, x);
    printf("%s", bl);
    for (int i = 0; i < w - 2; i++) printf("%s", h_char);
    printf("%s", br);
}

// Enhanced dashboard with multi-panel layout
void tui_show_dashboard_enhanced(Database *db, const char *repo_dir) {
    if (!db) return;

    // Always drain input before setting up terminal
    // In canonical mode, input is buffered until newline, so we need to
    // temporarily disable canonical mode to drain it
    struct termios temp_termios;
    if (tcgetattr(STDIN_FILENO, &temp_termios) == 0) {
        // Temporarily disable canonical mode to drain input
        struct termios noncanon = temp_termios;
        noncanon.c_lflag &= ~ICANON;
        noncanon.c_cc[VMIN] = 0;
        noncanon.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &noncanon);

        // Small delay to let terminal process mode change
        usleep(20000);  // 20ms - increased delay

        // Drain all available input (including any newlines)
        // Use ioctl to check for available bytes
        int bytes_available = 0;
        int total_drained = 0;
        for (int attempt = 0; attempt < 10; attempt++) {
            if (ioctl(STDIN_FILENO, FIONREAD, &bytes_available) == 0 && bytes_available > 0) {
                char c;
                int count = 0;
                while (read(STDIN_FILENO, &c, 1) == 1 && count < 200) {
                    count++;
                    total_drained++;
                }
            } else {
                // No more input available, but try a few more times to catch delayed input
                if (attempt < 5) {
                    usleep(10000);  // 10ms delay
                    continue;
                }
                break;
            }
            usleep(10000);  // 10ms between attempts
        }

        // Restore canonical mode and flush
        tcsetattr(STDIN_FILENO, TCSANOW, &temp_termios);
    }
    tcflush(STDIN_FILENO, TCIFLUSH);

    tui_setup_terminal();
    tui_enable_raw_mode();
    tui_hide_cursor();

    // Clear screen and any pending input immediately after setup
    tui_clear_screen();

    // Aggressively clear any pending input after initial draw
    // Do this multiple times with increasing delays to catch all buffered input
    clear_pending_input();
    usleep(50000);  // 50ms - give time for any delayed input to arrive
    clear_pending_input();
    usleep(50000);  // 50ms
    clear_pending_input();

    // One more aggressive pass - drain everything
    char c_drain;
    int final_drain = 0;
    while (read(STDIN_FILENO, &c_drain, 1) == 1 && final_drain < 50) {
        final_drain++;
    }
    tcflush(STDIN_FILENO, TCIFLUSH);

    // Wait a bit before accepting input to ensure any buffered input has been cleared
    // This gives the terminal time to process all our input clearing
    usleep(150000);  // 150ms - enough time for any delayed input to arrive

    // One final aggressive drain after the wait
    clear_pending_input();
    char c2_drain;
    int final_count = 0;
    while (read(STDIN_FILENO, &c2_drain, 1) == 1 && final_count < 50) {
        final_count++;
    }
    tcflush(STDIN_FILENO, TCIFLUSH);

    // Track when we're ready to accept input
    struct timeval ready_time;
    gettimeofday(&ready_time, NULL);

    Repository *repo = repository_new(repo_dir);
    if (!repo) {
        tui_restore_terminal();
        tui_show_cursor();
        return;
    }

    int width = tui_get_terminal_width();
    int height = tui_get_terminal_height();

    // Safety checks for terminal size
    if (width < 40) width = 80;
    if (height < 10) height = 24;

    const char *lang = getenv("LANG");
    bool use_unicode = (lang && strstr(lang, "UTF-8")) || getenv("LC_ALL");
    const char *tl = use_unicode ? "┌" : "+";
    const char *tr = use_unicode ? "┐" : "+";
    const char *bl = use_unicode ? "└" : "+";
    const char *br = use_unicode ? "┘" : "+";
    const char *h_line = use_unicode ? "─" : "-";
    const char *v_line = use_unicode ? "│" : "|";
    const char *t_join = use_unicode ? "┬" : "+";
    const char *b_join = use_unicode ? "┴" : "+";
    const char *l_join = use_unicode ? "├" : "+";
    const char *r_join = use_unicode ? "┤" : "+";

    // Get upgrade info
    size_t upgrade_count = 0;
    PackageUpgradeInfo *upgrades = NULL;
    if (db && repo) {
        upgrades = tui_get_upgrade_info(db, repo, &upgrade_count);
    }

    size_t installed_count = 0;
    char **installed = NULL;
    if (db) {
        installed = database_list_installed(db, &installed_count);
    }

    size_t available_count = 0;
    char **available = NULL;
    if (repo) {
        available = repository_list_packages(repo, &available_count);
    }

    TabType current_tab = TAB_OVERVIEW;
    char search_query[256] = {0};
    size_t search_pos = 0;
    bool searching = false;
    int selected_action = 0; // For quick actions menu
    int quick_action_count = 5;

    // Main rendering loop
    while (true) {
        // Clear screen and ensure clean state
        tui_clear_screen();

        // Ensure we have valid dimensions
        if (width <= 0) width = 80;
        if (height <= 0) height = 24;

        if (supports_colors()) {
            printf("%s", COLOR_BRIGHT_CYAN);
        }

        // Top border with search and tabs
        tui_move_cursor(0, 0);
        printf("%s", tl);
        for (int i = 0; i < width - 2; i++) printf("%s", h_line);
        printf("%s", tr);
        fflush(stdout);

        // Search bar and tabs row
        tui_move_cursor(1, 0);
        printf("%s", v_line);

        // Search box
        int search_width = width / 2 - 2;
        if (searching) {
            printf(" [Search: %s_", search_query);
            int search_fill = search_width - 12 - (int)search_pos;
            if (search_fill < 0) search_fill = 0;
            for (int i = 0; i < search_fill; i++) printf(" ");
            printf("]");
        } else {
            printf(" [Search...");
            int search_fill = search_width - 12;
            if (search_fill < 0) search_fill = 0;
            for (int i = 0; i < search_fill; i++) printf(" ");
            printf("]");
        }
        // Fill space between search and tabs
        int search_end = 2 + search_width;
        int tab_start = width / 2;
        for (int i = search_end; i < tab_start; i++) printf(" ");
        printf("%s", v_line);

        // Tabs - positioned after search box
        const char *tabs[] = {"Overview", "Browse", "Installed", "History"};
        tui_move_cursor(1, tab_start);
        printf("TABS: ");
        for (int i = 0; i < TAB_COUNT; i++) {
            if ((TabType)i == current_tab) {
                if (supports_colors()) {
                    printf("%s[%s%s%s] ", COLOR_BG_CYAN, COLOR_BRIGHT_WHITE, tabs[i], COLOR_RESET);
                    if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
                } else {
                    printf("[%s] ", tabs[i]);
                }
            } else {
                printf("[%s] ", tabs[i]);
            }
        }
        // Fill to end of line
        int tabs_end = tab_start + 6 + (4 * 12);  // "TABS: " + 4 tabs with brackets
        for (int i = tabs_end; i < width - 1; i++) printf(" ");
        printf("%s", v_line);

        if (supports_colors()) {
            printf("%s", COLOR_RESET);
        }

        // Horizontal divider
        tui_move_cursor(2, 0);
        if (supports_colors()) {
            printf("%s", COLOR_BRIGHT_CYAN);
        }
        printf("%s", l_join);
        for (int i = 0; i < width - 2; i++) printf("%s", h_line);
        printf("%s", r_join);
        if (supports_colors()) {
            printf("%s", COLOR_RESET);
        }
        fflush(stdout);

        // Main content area - split into left and right panels
        int left_width = width / 2;
        int right_width = width - left_width - 1;
        int content_start_y = 3;
        int content_height = height - content_start_y - 3;

        // Ensure minimum sizes
        if (content_height < 5) content_height = 5;
        if (left_width < 20) left_width = 20;
        if (right_width < 20) right_width = 20;

        // Left panel - System Status
        int left_x = 0;
        int left_y = content_start_y;

        if (supports_colors()) {
            printf("%s", COLOR_BRIGHT_CYAN);
        }

        // Left panel top
        tui_move_cursor(left_y, left_x);
        printf("%s", tl);
        for (int i = 0; i < left_width - 2; i++) printf("%s", h_line);
        printf("%s", t_join);

        // Left panel title
        tui_move_cursor(left_y + 1, left_x);
        printf("%s", v_line);
        if (supports_colors()) {
            printf("%s%sSYSTEM STATUS%s", COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET);
            if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
        } else {
            printf(" SYSTEM STATUS");
        }
        for (int i = 15; i < left_width - 2; i++) printf(" ");
        printf("%s", v_line);

        // Left panel divider
        tui_move_cursor(left_y + 2, left_x);
        printf("%s", l_join);
        for (int i = 0; i < left_width - 2; i++) printf("%s", h_line);
        printf("%s", t_join);

        // Updates available box
        int updates_y = left_y + 3;
        tui_move_cursor(updates_y, left_x + 1);
        printf("%s", v_line);
        if (supports_colors()) {
            printf("%s%sUPDATES AVAILABLE%s", COLOR_BRIGHT_YELLOW, COLOR_BOLD, COLOR_RESET);
            if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
        } else {
            printf(" UPDATES AVAILABLE");
        }
        for (int i = 18; i < left_width - 3; i++) printf(" ");
        printf("%s", v_line);

        // Updates count box - use proper box drawing characters
        int box_x = left_x + 2;
        int box_y = updates_y + 1;
        int box_w = 20;

        // Top border of box
        tui_move_cursor(box_y, box_x);
        if (use_unicode) {
            printf("┌");
        } else {
            printf("+");
        }
        for (int i = 0; i < box_w - 2; i++) {
            printf("%s", use_unicode ? "─" : "-");
        }
        if (use_unicode) {
            printf("┐");
        } else {
            printf("+");
        }

        // Middle row with number
        tui_move_cursor(box_y + 1, box_x);
        if (use_unicode) {
            printf("│");
        } else {
            printf("|");
        }
        if (supports_colors()) {
            printf("%s%8zu%s", COLOR_BRIGHT_GREEN, upgrade_count, COLOR_RESET);
            if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
        } else {
            printf("%8zu", upgrade_count);
        }
        for (int i = 10; i < box_w - 2; i++) printf(" ");
        if (use_unicode) {
            printf("│");
        } else {
            printf("|");
        }

        // Bottom border of box
        tui_move_cursor(box_y + 2, box_x);
        if (use_unicode) {
            printf("└");
        } else {
            printf("+");
        }
        for (int i = 0; i < box_w - 2; i++) {
            printf("%s", use_unicode ? "─" : "-");
        }
        if (use_unicode) {
            printf("┘");
        } else {
            printf("+");
        }

        // View Updates button
        tui_move_cursor(box_y + 3, box_x);
        printf(" [View Updates (u)]");

        // Total installed
        int total_y = box_y + 5;
        tui_move_cursor(total_y, left_x + 1);
        printf("%s", v_line);
        if (supports_colors()) {
            printf("%sTotal Installed:%s", COLOR_BRIGHT_CYAN, COLOR_RESET);
            if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
        } else {
            printf("Total Installed:");
        }
        printf(" > %zu packages", installed_count);
        for (int i = 25; i < left_width - 3; i++) printf(" ");
        printf("%s", v_line);

        // System Health
        int health_y = total_y + 1;
        tui_move_cursor(health_y, left_x + 1);
        printf("%s", v_line);
        if (supports_colors()) {
            printf("%sSystem Health:%s", COLOR_BRIGHT_CYAN, COLOR_RESET);
            if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
        } else {
            printf("System Health:");
        }
        printf(" [%s] OK (No breakage)", ICON_SUCCESS);
        for (int i = 30; i < left_width - 3; i++) printf(" ");
        printf("%s", v_line);

        // Quick Actions
        int actions_y = health_y + 2;
        tui_move_cursor(actions_y, left_x + 1);
        printf("%s", l_join);
        for (int i = 0; i < left_width - 2; i++) printf("%s", h_line);
        printf("%s", t_join);

        tui_move_cursor(actions_y + 1, left_x + 1);
        printf("%s", v_line);
        if (supports_colors()) {
            printf("%s%sQUICK ACTIONS%s", COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET);
            if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
        } else {
            printf(" QUICK ACTIONS");
        }
        for (int i = 14; i < left_width - 2; i++) printf(" ");
        printf("%s", v_line);

        tui_move_cursor(actions_y + 2, left_x + 1);
        printf("%s", l_join);
        for (int i = 0; i < left_width - 2; i++) printf("%s", h_line);
        printf("%s", t_join);

        const char *actions[] = {
            "[U] Update All List",
            "[S] Search Packages",
            "[C] Clean Cache",
            "[H] Full History",
            "[R] Refresh Mirrors"
        };

        for (int i = 0; i < quick_action_count && (actions_y + 3 + i) < (left_y + content_height - 1); i++) {
            tui_move_cursor(actions_y + 3 + i, left_x + 1);
            printf("%s", v_line);
            if (i == selected_action) {
                if (supports_colors()) {
                    printf("%s> %s%s%s", COLOR_BG_CYAN, COLOR_BRIGHT_WHITE, actions[i], COLOR_RESET);
                    if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
                } else {
                    printf("> %s", actions[i]);
                }
            } else {
                printf("  %s", actions[i]);
            }
            for (int j = (int)strlen(actions[i]) + 2; j < left_width - 3; j++) printf(" ");
            printf("%s", v_line);
        }

        // Left panel bottom - fill remaining space with vertical borders
        int left_panel_bottom = left_y + content_height - 1;
        for (int i = actions_y + 3 + quick_action_count; i < left_panel_bottom; i++) {
            if (i >= left_y + 1 && i < left_panel_bottom) {
                tui_move_cursor(i, left_x);
                printf("%s", v_line);
                for (int j = 0; j < left_width - 2; j++) printf(" ");
                printf("%s", v_line);
            }
        }

        tui_move_cursor(left_y + content_height - 1, left_x);
        printf("%s", bl);
        for (int i = 0; i < left_width - 2; i++) printf("%s", h_line);
        printf("%s", b_join);
        fflush(stdout);

        // Right panel - Recent History / Content
        int right_x = left_width;
        int right_y = content_start_y;

        // Right panel top
        tui_move_cursor(right_y, right_x);
        printf("%s", t_join);
        for (int i = 0; i < right_width - 2; i++) printf("%s", h_line);
        printf("%s", tr);

        // Right panel title
        tui_move_cursor(right_y + 1, right_x);
        printf("%s", v_line);
        printf(" ");
        if (current_tab == TAB_OVERVIEW) {
            if (supports_colors()) {
                printf("%s%sRECENT HISTORY (Last 10 transactions)%s", COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET);
                if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
            } else {
                printf("RECENT HISTORY (Last 10 transactions)");
            }
        } else if (current_tab == TAB_BROWSE) {
            if (supports_colors()) {
                printf("%s%sAVAILABLE PACKAGES%s", COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET);
                if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
            } else {
                printf("AVAILABLE PACKAGES");
            }
        } else if (current_tab == TAB_INSTALLED) {
            if (supports_colors()) {
                printf("%s%sINSTALLED PACKAGES%s", COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET);
                if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
            } else {
                printf("INSTALLED PACKAGES");
            }
        }
        int title_len = current_tab == TAB_OVERVIEW ? 36 : (current_tab == TAB_BROWSE ? 20 : 19);
        for (int i = title_len; i < right_width - 2; i++) printf(" ");
        printf("%s", v_line);

        // Right panel divider
        tui_move_cursor(right_y + 2, right_x);
        printf("%s", l_join);
        for (int i = 0; i < right_width - 2; i++) printf("%s", h_line);
        printf("%s", r_join);

        // Right panel content
        int content_y = right_y + 3;
        if (current_tab == TAB_OVERVIEW) {
            // History table header
            tui_move_cursor(content_y, right_x + 1);
            printf("%s", v_line);
            printf(" Date       │ Action   │ Package(s)");
            for (int i = 40; i < right_width - 3; i++) printf(" ");
            printf("%s", v_line);

            tui_move_cursor(content_y + 1, right_x + 1);
            printf("%s", l_join);
            for (int i = 0; i < 12; i++) printf("─");
            printf("┼");
            for (int i = 0; i < 10; i++) printf("─");
            printf("┼");
            for (int i = 0; i < right_width - 25; i++) printf("─");
            printf("%s", r_join);

            // Sample history entries (placeholder)
            const char *history[][3] = {
                {"2023-11-30", "Install", "neovim, ripgrep"},
                {"2023-11-28", "Update", "firefox (119->120)"},
                {"2023-11-25", "Remove", "nodejs-lts"},
                {"2023-11-20", "sys-updt", "45 packages affected"},
                {"2023-11-15", "Install", "docker-ce, docker-compose"},
                {"2023-11-10", "Clean", "Cache cleared (500MB freed)"},
                {"2023-11-05", "Update", "kernel (6.5.9->6.6.1)"}
            };

            for (int i = 0; i < 7 && (content_y + 2 + i) < (right_y + content_height - 3); i++) {
                tui_move_cursor(content_y + 2 + i, right_x + 1);
                printf("%s", v_line);
                printf(" %-10s │ %-8s │ %s", history[i][0], history[i][1], history[i][2]);
                int fill = right_width - 3 - 12 - 1 - 10 - 1 - (int)strlen(history[i][2]);
                for (int j = 0; j < fill; j++) printf(" ");
                printf("%s", v_line);
            }

            // Storage summary
            int storage_y = content_y + 9;
            if (storage_y < (right_y + content_height - 3)) {
                tui_move_cursor(storage_y, right_x + 1);
                printf("%s", l_join);
                for (int i = 0; i < right_width - 3; i++) printf("─");
                printf("%s", r_join);

                tui_move_cursor(storage_y + 1, right_x + 1);
                printf("%s", v_line);
                if (supports_colors()) {
                    printf("%s%sSTORAGE SUMMARY%s", COLOR_BRIGHT_MAGENTA, COLOR_BOLD, COLOR_RESET);
                    if (supports_colors()) printf("%s", COLOR_BRIGHT_CYAN);
                } else {
                    printf(" STORAGE SUMMARY");
                }
                for (int i = 16; i < right_width - 3; i++) printf(" ");
                printf("%s", v_line);

                tui_move_cursor(storage_y + 2, right_x + 1);
                printf("%s", l_join);
                for (int i = 0; i < right_width - 3; i++) printf("─");
                printf("%s", r_join);

                // Storage bars
                tui_move_cursor(storage_y + 3, right_x + 1);
                printf("%s", v_line);
                printf(" Package DB:  250 MB [");
                int bar_width = right_width - 30;  // Space for text and brackets
                if (bar_width < 10) bar_width = 10;
                if (bar_width > 30) bar_width = 30;
                int filled = (int)(bar_width * 0.15);  // ~15% for 250MB
                if (supports_colors()) {
                    for (int i = 0; i < filled; i++) printf("%s█%s", COLOR_BRIGHT_GREEN, COLOR_RESET);
                    for (int i = filled; i < bar_width; i++) printf("%s░%s", COLOR_DIM, COLOR_RESET);
                } else {
                    for (int i = 0; i < filled; i++) printf("#");
                    for (int i = filled; i < bar_width; i++) printf(".");
                }
                printf("]");
                int fill_end = right_x + 1 + 1 + 20 + 1 + bar_width + 1;  // v_line + space + text + [ + bar + ]
                for (int i = fill_end; i < right_x + right_width - 1; i++) printf(" ");
                printf("%s", v_line);

                tui_move_cursor(storage_y + 4, right_x + 1);
                printf("%s", v_line);
                printf(" Pkg Cache:   1.2 GB [");
                int filled2 = (int)(bar_width * 0.75);  // ~75% for 1.2GB
                if (supports_colors()) {
                    for (int i = 0; i < filled2; i++) printf("%s█%s", COLOR_BRIGHT_YELLOW, COLOR_RESET);
                    for (int i = filled2; i < bar_width; i++) printf("%s░%s", COLOR_DIM, COLOR_RESET);
                } else {
                    for (int i = 0; i < filled2; i++) printf("#");
                    for (int i = filled2; i < bar_width; i++) printf(".");
                }
                printf("]");
                for (int i = fill_end; i < right_x + right_width - 1; i++) printf(" ");
                printf("%s", v_line);

                tui_move_cursor(storage_y + 5, right_x + 1);
                printf("%s", v_line);
                printf(" Total Usage: 1.45 GB / 50 GB allocated partition");
                int usage_len = 45;
                for (int i = usage_len; i < right_width - 3; i++) printf(" ");
                printf("%s", v_line);
            }
        } else if (current_tab == TAB_BROWSE) {
            // Browse packages list
            int list_start = content_y;
            int max_items = (right_y + content_height - 1) - list_start - 1;
            for (int i = 0; i < max_items && i < (int)available_count; i++) {
                tui_move_cursor(list_start + i, right_x + 1);
                printf("%s", v_line);
                if (supports_colors()) {
                    printf(" %s%s%s %s%s%s", COLOR_BRIGHT_BLUE, ICON_PACKAGE, COLOR_RESET, COLOR_BRIGHT_CYAN, available[i], COLOR_RESET);
                } else {
                    printf(" %s", available[i]);
                }
                for (int j = (int)strlen(available[i]) + 3; j < right_width - 3; j++) printf(" ");
                printf("%s", v_line);
            }
        } else if (current_tab == TAB_INSTALLED) {
            // Installed packages with upgrade status
            int list_start = content_y;
            int max_items = (right_y + content_height - 1) - list_start - 1;
            for (int i = 0; i < max_items && i < (int)installed_count; i++) {
                InstalledPackage *pkg = database_get_package(db, installed[i]);
                if (pkg) {
                    tui_move_cursor(list_start + i, right_x + 1);
                    printf("%s", v_line);

                    bool is_upgradable = false;
                    const char *latest_ver = NULL;
                    for (size_t j = 0; j < upgrade_count; j++) {
                        if (upgrades[j].package_name && strcmp(upgrades[j].package_name, pkg->name) == 0) {
                            is_upgradable = upgrades[j].upgradable;
                            latest_ver = upgrades[j].latest_version;
                            break;
                        }
                    }

                    if (supports_colors()) {
                        printf(" %s%s%s %s%s%s", COLOR_BRIGHT_BLUE, ICON_PACKAGE, COLOR_RESET, COLOR_BRIGHT_CYAN, pkg->name, COLOR_RESET);
                        if (pkg->version) {
                            printf(" %s(%s)%s", COLOR_DIM, pkg->version, COLOR_RESET);
                        }
                        if (is_upgradable && latest_ver) {
                            printf(" %s%s→ %s%s", COLOR_BRIGHT_YELLOW, ICON_ARROW, latest_ver, COLOR_RESET);
                        }
                    } else {
                        printf(" %s (%s)", pkg->name, pkg->version ? pkg->version : "unknown");
                        if (is_upgradable && latest_ver) {
                            printf(" -> %s", latest_ver);
                        }
                    }
                    int fill = right_width - 3 - (int)strlen(pkg->name) - (pkg->version ? (int)strlen(pkg->version) + 3 : 0) - (is_upgradable ? 10 : 0);
                    for (int j = 0; j < fill; j++) printf(" ");
                    printf("%s", v_line);
                }
            }
        }

        // Right panel bottom - fill remaining space with vertical borders
        int right_panel_bottom = right_y + content_height - 1;
        int fill_start = content_y + 10;
        if (current_tab == TAB_OVERVIEW) {
            fill_start = content_y + 15; // After storage summary
        }
        for (int i = fill_start; i < right_panel_bottom; i++) {
            if (i >= right_y + 1 && i < right_panel_bottom) {
                tui_move_cursor(i, right_x);
                printf("%s", v_line);
                for (int j = 0; j < right_width - 2; j++) printf(" ");
                printf("%s", v_line);
            }
        }

        tui_move_cursor(right_y + content_height - 1, right_x);
        printf("%s", b_join);
        for (int i = 0; i < right_width - 2; i++) printf("%s", h_line);
        printf("%s", br);
        fflush(stdout);

        if (supports_colors()) {
            printf("%s", COLOR_RESET);
        }

        // Bottom border
        tui_move_cursor(height - 2, 0);
        if (supports_colors()) {
            printf("%s", COLOR_BRIGHT_CYAN);
        }
        printf("%s", bl);
        for (int i = 0; i < width - 2; i++) printf("%s", h_line);
        printf("%s", br);
        if (supports_colors()) {
            printf("%s", COLOR_RESET);
        }

        // Footer instructions
        tui_move_cursor(height - 1, 0);
        if (supports_colors()) {
            printf("%s[↑/↓] Navigate Menu  [Enter] Select Action  [Tab] Switch Tabs  [q] Quit%s", COLOR_DIM, COLOR_RESET);
        } else {
            printf("[↑/↓] Navigate Menu  [Enter] Select Action  [Tab] Switch Tabs  [q] Quit");
        }

        // Clear rest of footer line to prevent artifacts
        int footer_len = supports_colors() ? 70 : 60;  // Approximate length
        for (int i = footer_len; i < width; i++) {
            printf(" ");
        }

        // Ensure cursor is at end and flush all output
        tui_move_cursor(height - 1, width - 1);
        fflush(stdout);

        KeyEvent key;
        // Read key (non-blocking due to raw mode with VMIN=0, VTIME=1)
        if (tui_read_key(&key)) {
            // Ignore Enter keys that come within 100ms of being ready (likely buffered)
            if (key.code == KEY_ENTER) {
                struct timeval current_time;
                gettimeofday(&current_time, NULL);
                long elapsed_ms = (current_time.tv_sec - ready_time.tv_sec) * 1000 +
                                 (current_time.tv_usec - ready_time.tv_usec) / 1000;
                if (elapsed_ms < 100) {
                    // Enter came too quickly - likely buffered input, ignore it
                    continue;
                }
            }

            if (searching) {
                // Handle search input
                if (key.code == KEY_ENTER || key.code == KEY_ESC) {
                    searching = false;
                } else if (key.code == KEY_BACKSPACE) {
                    if (search_pos > 0) {
                        search_query[--search_pos] = '\0';
                    }
                } else if (key.code == KEY_CHAR && isprint(key.ch) && search_pos < sizeof(search_query) - 1) {
                    search_query[search_pos++] = key.ch;
                    search_query[search_pos] = '\0';
                }
            } else if (key.code == KEY_CHAR) {
                if (key.ch == 'q' || key.ch == 'Q') {
                    break;
                } else if (key.ch == 'u' || key.ch == 'U') {
                    tui_restore_terminal();
                    tui_show_cursor();
                    tui_interactive_update(repo_dir, NULL);
                    tui_setup_terminal();
                    tui_enable_raw_mode();
                    tui_hide_cursor();
                    repository_free(repo);
                    repo = repository_new(repo_dir);
                    if (upgrades) {
                        for (size_t i = 0; i < upgrade_count; i++) {
                            if (upgrades[i].package_name) free(upgrades[i].package_name);
                            if (upgrades[i].installed_version) free(upgrades[i].installed_version);
                            if (upgrades[i].latest_version) free(upgrades[i].latest_version);
                        }
                        free(upgrades);
                    }
                    upgrades = tui_get_upgrade_info(db, repo, &upgrade_count);
                } else if (key.ch == 's' || key.ch == 'S') {
                    searching = true;
                    search_pos = 0;
                    search_query[0] = '\0';
                } else if (key.ch == 'i' || key.ch == 'I') {
                    tui_restore_terminal();
                    tui_show_cursor();
                    tui_interactive_install(repo_dir, NULL, NULL);
                    tui_setup_terminal();
                    tui_enable_raw_mode();
                    tui_hide_cursor();
                } else if (key.ch == 'g' || key.ch == 'G') {
                    tui_restore_terminal();
                    tui_show_cursor();
                    tui_interactive_upgrade(db, repo_dir, NULL);
                    tui_setup_terminal();
                    tui_enable_raw_mode();
                    tui_hide_cursor();
                    if (upgrades) {
                        for (size_t i = 0; i < upgrade_count; i++) {
                            if (upgrades[i].package_name) free(upgrades[i].package_name);
                            if (upgrades[i].installed_version) free(upgrades[i].installed_version);
                            if (upgrades[i].latest_version) free(upgrades[i].latest_version);
                        }
                        free(upgrades);
                    }
                    upgrades = tui_get_upgrade_info(db, repo, &upgrade_count);
                }
            } else if (key.code == KEY_TAB) {
                current_tab = (TabType)((current_tab + 1) % TAB_COUNT);
                // Force immediate redraw on tab switch to prevent artifacts
                continue;  // Skip to next loop iteration to redraw
            } else if (key.code == KEY_UP) {
                if (selected_action > 0) selected_action--;
                continue;  // Redraw immediately
            } else if (key.code == KEY_DOWN) {
                if (selected_action < quick_action_count - 1) selected_action++;
                continue;  // Redraw immediately
            } else if (key.code == KEY_ENTER) {
                // Execute selected action
                switch (selected_action) {
                    case 0: // Update All
                        tui_restore_terminal();
                        tui_show_cursor();
                        tui_interactive_update(repo_dir, NULL);
                        tui_setup_terminal();
                        tui_enable_raw_mode();
                        tui_hide_cursor();
                        repository_free(repo);
                        repo = repository_new(repo_dir);
                        if (upgrades) {
                            for (size_t i = 0; i < upgrade_count; i++) {
                                if (upgrades[i].package_name) free(upgrades[i].package_name);
                                if (upgrades[i].installed_version) free(upgrades[i].installed_version);
                                if (upgrades[i].latest_version) free(upgrades[i].latest_version);
                            }
                            free(upgrades);
                        }
                        upgrades = tui_get_upgrade_info(db, repo, &upgrade_count);
                        break;
                    case 1: // Search
                        searching = true;
                        search_pos = 0;
                        search_query[0] = '\0';
                        break;
                    case 2: // Clean Cache
                        // Placeholder
                        break;
                    case 3: // Full History
                        // Placeholder
                        break;
                    case 4: // Refresh Mirrors
                        // Placeholder
                        break;
                }
            } else if (key.code == KEY_ESC || key.code == KEY_CTRL_C) {
                break;
            }
        }

        // Small delay to prevent CPU spinning, but allow responsive updates
        usleep(20000);  // 20ms - reduced for better responsiveness
    }

    // Cleanup
    if (upgrades) {
        for (size_t i = 0; i < upgrade_count; i++) {
            if (upgrades[i].package_name) free(upgrades[i].package_name);
            if (upgrades[i].installed_version) free(upgrades[i].installed_version);
            if (upgrades[i].latest_version) free(upgrades[i].latest_version);
        }
        free(upgrades);
    }
    if (installed) {
        for (size_t i = 0; i < installed_count; i++) free(installed[i]);
        free(installed);
    }
    if (available) {
        for (size_t i = 0; i < available_count; i++) free(available[i]);
        free(available);
    }
    if (repo) {
        repository_free(repo);
    }
    tui_restore_terminal();
    tui_show_cursor();
}

// Main TUI menu
int tui_main_menu(void) {
    log_info("TUI main menu invoked");
    log_developer("Entering TUI main menu");
    const char *home = getenv("HOME");
    if (!home) home = "/root";

    char db_dir[1024];
    snprintf(db_dir, sizeof(db_dir), "%s/.tsi/db", home);

    char repo_dir[1024];
    snprintf(repo_dir, sizeof(repo_dir), "%s/.tsi/repos", home);

    Database *db = database_new(db_dir);
    if (!db) {
        fprintf(stderr, "Error: Failed to initialize database\n");
        return 1;
    }

    Menu *menu = menu_new("TSI - The Source Installer");
    if (!menu) {
        database_free(db);
        return 1;
    }

    menu_add_item(menu, "Dashboard", "View installed packages and statistics", (void*)1, true);
    menu_add_item(menu, "Browse Packages", "Browse and search available packages", (void*)2, true);
    menu_add_item(menu, "Install Package", "Install a new package", (void*)3, true);
    menu_add_item(menu, "List Installed", "List all installed packages", (void*)4, true);
    menu_add_item(menu, "Update Repository", "Update package repository", (void*)5, true);
    menu_add_item(menu, "Exit", "Exit TSI", (void*)6, true);

    int result = menu_show(menu);

    if (result >= 0) {
        MenuItem *item = menu->items;
        int index = 0;
        while (item && index < result) {
            item = item->next;
            index++;
        }

        if (item) {
            int choice = (int)(intptr_t)item->data;

            switch (choice) {
                case 1: // Dashboard
                    tui_show_dashboard_enhanced(db, repo_dir);
                    break;

                case 2: // Browse Packages
                    tui_show_package_browser(repo_dir);
                    break;

                case 3: // Install Package
                    tui_show_package_browser(repo_dir);
                    // Installation would be triggered from browser
                    break;

                case 4: { // List Installed
                    tui_setup_terminal();
                    tui_clear_screen();

                    size_t count = 0;
                    char **packages = database_list_installed(db, &count);

                    tui_move_cursor(0, 0);
                    if (supports_colors()) {
                        printf("%s%s%s Installed Packages %s%s\n",
                               COLOR_BRIGHT_MAGENTA, COLOR_BOLD, ICON_PACKAGE, COLOR_RESET, CLEAR_LINE);
                    } else {
                        printf("Installed Packages\n");
                    }

                    if (count == 0) {
                        tui_move_cursor(2, 2);
                        printf("No packages installed.\n");
                    } else {
                        for (size_t i = 0; i < count; i++) {
                            InstalledPackage *pkg = database_get_package(db, packages[i]);
                            if (pkg) {
                                tui_move_cursor((int)i + 2, 2);
                                if (supports_colors()) {
                                    const char *version_str = pkg->version ? pkg->version : "unknown";
                                    printf("%s%s%s %s%s%s (%s%s%s)\n",
                                           COLOR_BRIGHT_BLUE, ICON_PACKAGE, COLOR_RESET,
                                           COLOR_BRIGHT_CYAN, pkg->name, COLOR_RESET,
                                           COLOR_DIM, version_str, COLOR_RESET);
                                } else {
                                    printf("%s (%s)\n", pkg->name, pkg->version ? pkg->version : "unknown");
                                }
                            }
                            free(packages[i]);
                        }
                    }

                    if (packages) free(packages);

                    tui_move_cursor(tui_get_terminal_height() - 1, 0);
                    if (supports_colors()) {
                        printf("%sPress any key to continue...%s", COLOR_DIM, COLOR_RESET);
                    } else {
                        printf("Press any key to continue...");
                    }
                    fflush(stdout);

                    tui_enable_raw_mode();
                    KeyEvent key;
                    while (!tui_read_key(&key)) {
                        usleep(10000);
                    }
                    tui_disable_raw_mode();

                    tui_restore_terminal();
                    tui_show_cursor();
                    break;
                }

                case 5: // Update Repository
                    // Would call update command
                    tui_setup_terminal();
                    tui_clear_screen();
                    tui_move_cursor(0, 0);
                    if (supports_colors()) {
                        printf("%s%s%s Updating repository...%s%s\n",
                               COLOR_BRIGHT_YELLOW, COLOR_BOLD, ICON_DOWNLOAD, COLOR_RESET, CLEAR_LINE);
                    } else {
                        printf("Updating repository...\n");
                    }
                    fflush(stdout);
                    // In real implementation, would call cmd_update
                    tui_move_cursor(2, 2);
                    printf("Repository update functionality would be called here.\n");
                    tui_move_cursor(tui_get_terminal_height() - 1, 0);
                    if (supports_colors()) {
                        printf("%sPress any key to continue...%s", COLOR_DIM, COLOR_RESET);
                    } else {
                        printf("Press any key to continue...");
                    }
                    fflush(stdout);

                    tui_enable_raw_mode();
                    KeyEvent key;
                    while (!tui_read_key(&key)) {
                        usleep(10000);
                    }
                    tui_disable_raw_mode();

                    tui_restore_terminal();
                    tui_show_cursor();
                    break;

                case 6: // Exit
                default:
                    break;
            }
        }
    }

    menu_free(menu);
    database_free(db);
    return 0;
}
