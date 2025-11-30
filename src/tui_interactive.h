#ifndef TUI_INTERACTIVE_H
#define TUI_INTERACTIVE_H

#include <stdbool.h>
#include <stddef.h>
#include "package.h"
#include "database.h"
#include "resolver.h"

#ifdef __cplusplus
extern "C" {
#endif

// Terminal control
void tui_setup_terminal(void);
void tui_restore_terminal(void);
void tui_clear_screen(void);
void tui_move_cursor(int row, int col);
void tui_hide_cursor(void);
void tui_show_cursor(void);
void tui_enable_raw_mode(void);
void tui_disable_raw_mode(void);

// Input handling
typedef enum {
    KEY_UNKNOWN = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ENTER,
    KEY_ESC,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_DELETE,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_CTRL_C,
    KEY_CTRL_D,
    KEY_CTRL_L,
    KEY_CHAR
} KeyCode;

typedef struct {
    KeyCode code;
    char ch;
} KeyEvent;

bool tui_read_key(KeyEvent *event);
int tui_get_terminal_width(void);
int tui_get_terminal_height(void);

// Menu system
typedef struct MenuItem {
    char *label;
    char *description;
    void *data;
    bool enabled;
    struct MenuItem *next;
} MenuItem;

typedef struct {
    char *title;
    MenuItem *items;
    size_t item_count;
    int selected_index;
    int scroll_offset;
    int max_visible;
    bool loop_selection;
} Menu;

Menu* menu_new(const char *title);
void menu_free(Menu *menu);
void menu_add_item(Menu *menu, const char *label, const char *description, void *data, bool enabled);
int menu_show(Menu *menu);
void menu_draw(Menu *menu, int x, int y, int width, int height);

// Panel system
typedef struct Panel {
    int x, y;
    int width, height;
    char *title;
    bool border;
    bool visible;
    void (*draw)(struct Panel *panel, void *data);
    void *data;
} Panel;

Panel* panel_new(int x, int y, int width, int height, const char *title);
void panel_free(Panel *panel);
void panel_draw(Panel *panel);
void panel_set_title(Panel *panel, const char *title);
void panel_set_draw_callback(Panel *panel, void (*draw)(Panel *panel, void *data), void *data);

// List view with search
typedef struct {
    char **items;
    size_t item_count;
    size_t selected_index;
    size_t scroll_offset;
    size_t max_visible;
    char *filter;
    bool case_sensitive;
} ListView;

ListView* listview_new(void);
void listview_free(ListView *lv);
void listview_add_item(ListView *lv, const char *item);
void listview_set_filter(ListView *lv, const char *filter);
void listview_draw(ListView *lv, int x, int y, int width, int height);
int listview_handle_key(ListView *lv, KeyEvent *key);
const char* listview_get_selected(ListView *lv);

// Progress visualization
typedef struct {
    char *label;
    int current;
    int total;
    char *status_text;
    bool active;
} ProgressItem;

typedef struct {
    ProgressItem *items;
    size_t item_count;
    int max_visible;
} ProgressView;

ProgressView* progressview_new(void);
void progressview_free(ProgressView *pv);
void progressview_add(ProgressView *pv, const char *label, int total);
void progressview_update(ProgressView *pv, size_t index, int current, const char *status);
void progressview_draw(ProgressView *pv, int x, int y, int width, int height);
void progressview_finish(ProgressView *pv, size_t index, bool success);

// Package upgrade status
typedef struct {
    char *package_name;
    char *installed_version;
    char *latest_version;
    bool upgradable;
} PackageUpgradeInfo;

// Dropdown menu for selections
typedef struct {
    char **items;
    size_t item_count;
    size_t selected_index;
    size_t scroll_offset;
    int x, y;
    int width, height;
    bool visible;
    char *title;
} DropdownMenu;

DropdownMenu* dropdown_new(const char *title);
void dropdown_free(DropdownMenu *dd);
void dropdown_add_item(DropdownMenu *dd, const char *item);
int dropdown_show(DropdownMenu *dd, int x, int y, int width, int height);
void dropdown_draw(DropdownMenu *dd);
const char* dropdown_get_selected(DropdownMenu *dd);

// Check if package is upgradable
bool tui_check_package_upgradable(Repository *repo, const char *package_name, const char *installed_version, char **latest_version);
PackageUpgradeInfo* tui_get_upgrade_info(Database *db, Repository *repo, size_t *count);

// Tab system
typedef enum {
    TAB_OVERVIEW = 0,
    TAB_BROWSE,
    TAB_INSTALLED,
    TAB_HISTORY,
    TAB_COUNT
} TabType;

// Enhanced dashboard with multi-panel layout
void tui_show_dashboard_enhanced(Database *db, const char *repo_dir);

// Dashboard view
void tui_show_dashboard(Database *db, const char *repo_dir);

// Package browser
void tui_show_package_browser(const char *repo_dir);

// Interactive install with version selection
int tui_interactive_install(const char *repo_dir, const char *db_dir, const char *tsi_prefix);

// Interactive upgrade
int tui_interactive_upgrade(Database *db, const char *repo_dir, const char *tsi_prefix);

// Interactive update repository
int tui_interactive_update(const char *repo_dir, const char *tsi_prefix);

// Main TUI entry point
int tui_main_menu(void);

#ifdef __cplusplus
}
#endif

#endif // TUI_INTERACTIVE_H
