//! Interactive terminal UI (`tsi ui`): a btop-inspired, multi-tab workspace.
//!
//! Layout is a vertical stack — a one-line tab bar, the active tab's content,
//! an optional operation log pane, and a one-line footer. Long-running actions
//! (install/upgrade/…) run as `tsi` subprocesses whose output streams into the
//! log pane (see [`runner`]); only one runs at a time.

mod runner;
mod tabs;
mod theme;

use crate::core::database::Database;
use crate::core::package::Package;
use crate::core::registry::Registry;
use crate::ui as term;
use anyhow::Result;
use clap::Args;
use crossterm::event::{self, Event, KeyCode, KeyEventKind};
use crossterm::terminal::{
    disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen,
};
use crossterm::tty::IsTty;
use crossterm::{execute, ExecutableCommand};
use ratatui::backend::CrosstermBackend;
use ratatui::layout::{Alignment, Constraint, Direction, Layout, Rect};
use ratatui::style::Style;
use ratatui::text::{Line, Span};
use ratatui::widgets::{Clear, Paragraph, Wrap};
use ratatui::{Frame, Terminal};
use runner::OpRunner;
use std::collections::{HashSet, VecDeque};
use std::io::{stdout, Stdout};
use std::path::PathBuf;
use std::time::Duration;
use tabs::packages::{PendingAction, View};

#[derive(Args)]
pub struct UiArgs {
    #[arg(long)]
    pub prefix: Option<String>,
}

/// The workspace tabs, switched with the `1`/`2`/`3` keys.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tab {
    Packages,
    System,
    Tsi,
}

impl Tab {
    fn all() -> [Tab; 3] {
        [Tab::Packages, Tab::System, Tab::Tsi]
    }

    fn label(self) -> &'static str {
        match self {
            Tab::Packages => "Packages",
            Tab::System => "System",
            Tab::Tsi => "TSI",
        }
    }
}

/// Entry point for `tsi ui`.
pub fn run(args: UiArgs) -> Result<()> {
    if !stdout().is_tty() {
        term::output::error("tsi ui requires an interactive terminal (stdout is not a TTY)");
        return Err(anyhow::anyhow!("stdout is not a TTY"));
    }

    let (prefix, packages_dir) = crate::cli::resolve_packages_dir(args.prefix.as_deref())?;
    let registry = Registry::load_from_dir(&packages_dir)?;

    if registry.count() == 0 {
        term::output::info("No packages found in the registry.");
        term::output::info("Run 'tsi update' to fetch the latest package definitions.");
        return Ok(());
    }

    let db_dir = prefix.join("db");
    let db = Database::new(&db_dir)?;
    let packages = collect_packages(&registry);
    let mut app = App::new(prefix, packages, db);

    install_panic_hook();
    let _guard = TerminalGuard::new()?;
    let mut terminal = Terminal::new(CrosstermBackend::new(stdout()))?;
    terminal.clear()?;

    let result = run_app(&mut terminal, &mut app);

    // Restore the terminal before surfacing any error to the caller.
    drop(_guard);

    result
}

/// One entry per package name, at its latest known version.
fn collect_packages(registry: &Registry) -> Vec<Package> {
    let mut names: Vec<&String> = registry.package_names().collect();
    names.sort();
    names
        .into_iter()
        .filter_map(|n| registry.get(n).cloned())
        .collect()
}

/// Shared UI state. Packages-tab fields and navigation live here so
/// [`tabs::packages`] (a descendant module) can reach them directly.
pub struct App {
    pub(crate) prefix: PathBuf,
    pub(crate) packages: Vec<Package>,
    pub(crate) db: Database,
    pub(crate) filtered: Vec<usize>,
    pub(crate) selected: usize,
    pub(crate) filter: String,
    pub(crate) filter_mode: bool,
    pub(crate) view: View,
    pub(crate) pending_confirm: Option<PendingAction>,
    /// Package names marked for a batch action (survives filter/view changes).
    pub(crate) marked: HashSet<String>,
    tab: Tab,
    help_open: bool,
    /// The running or just-finished operation, if any (log pane visible).
    op: Option<OpRunner>,
    /// Operations waiting to run once the current one finishes.
    queue: VecDeque<(String, Vec<String>)>,
    /// Awaiting y/N to quit while an operation is still running.
    quit_confirm: bool,
    /// Animation tick, advanced roughly every poll interval (for the spinner).
    tick: usize,
    /// TSI tab: highlighted action index.
    pub(crate) tsi_selected: usize,
    /// TSI tab: a maintenance action awaiting confirmation.
    pub(crate) tsi_confirm: Option<tabs::tsi::TsiConfirm>,
}

impl App {
    pub(crate) fn new(prefix: PathBuf, packages: Vec<Package>, db: Database) -> Self {
        let filtered = (0..packages.len()).collect();
        Self {
            prefix,
            packages,
            db,
            filtered,
            selected: 0,
            filter: String::new(),
            filter_mode: false,
            view: View::default(),
            pending_confirm: None,
            marked: HashSet::new(),
            tab: Tab::Packages,
            help_open: false,
            op: None,
            queue: VecDeque::new(),
            quit_confirm: false,
            tick: 0,
            tsi_selected: 0,
            tsi_confirm: None,
        }
    }

    /// Enqueues a `tsi <args>` operation, starting it immediately if the runner
    /// is idle (no op, or the previous one already finished). Queued ops run one
    /// at a time; each launches when its predecessor finishes.
    pub(crate) fn start_op(&mut self, label: String, args: Vec<String>) -> Result<()> {
        self.queue.push_back((label, args));
        self.start_next_if_idle()
    }

    /// Pops the next queued operation and spawns it, unless one is still running.
    /// A finished (but still displayed) op counts as idle and is superseded.
    fn start_next_if_idle(&mut self) -> Result<()> {
        if self.op_running() {
            return Ok(());
        }
        if let Some((label, args)) = self.queue.pop_front() {
            self.op = Some(OpRunner::spawn(label, args)?);
        }
        Ok(())
    }

    /// True while an operation subprocess is still executing.
    pub(crate) fn op_running(&self) -> bool {
        self.op.as_ref().is_some_and(|o| o.running())
    }

    /// Number of operations still waiting in the queue.
    pub(crate) fn queued(&self) -> usize {
        self.queue.len()
    }

    /// Reloads the installed-package database (after an operation finishes).
    pub(crate) fn refresh_db(&mut self) -> Result<()> {
        self.db.load()
    }

    /// Reloads the package definitions from disk, so a `tsi update` run from the
    /// TSI tab is reflected in the list without restarting the UI.
    ///
    /// A registry that fails to load or comes back empty (mid-update, or the prefix
    /// was just removed) leaves the previous snapshot in place rather than blanking
    /// the list.
    pub(crate) fn refresh_registry(&mut self) {
        let packages_dir = self.prefix.join("packages");
        if !packages_dir.is_dir() {
            return;
        }
        match Registry::load_from_dir(&packages_dir) {
            Ok(registry) if registry.count() > 0 => {
                self.packages = collect_packages(&registry);
            }
            Ok(_) => {}
            Err(e) => log::warn!("Could not reload package definitions: {e}"),
        }
    }

    /// Rebuilds derived state after an operation finishes, keeping the cursor on
    /// the same package instead of wherever its old index happens to land.
    fn refresh_after_op(&mut self) -> Result<()> {
        let previous = self.selected_package().map(|p| p.name.clone());
        self.refresh_db()?;
        self.refresh_registry();
        self.apply_filter();
        if let Some(name) = previous {
            if let Some(pos) = self
                .filtered
                .iter()
                .position(|&i| self.packages[i].name == name)
            {
                self.selected = pos;
            }
        }
        Ok(())
    }
}

/// RAII guard that enters raw mode + the alternate screen and always restores
/// the terminal on drop (including on early return via `?` or panic unwind).
struct TerminalGuard;

impl TerminalGuard {
    fn new() -> Result<Self> {
        enable_raw_mode()?;
        stdout().execute(EnterAlternateScreen)?;
        Ok(Self)
    }
}

impl Drop for TerminalGuard {
    fn drop(&mut self) {
        let _ = restore_terminal();
    }
}

fn restore_terminal() -> Result<()> {
    disable_raw_mode()?;
    execute!(stdout(), LeaveAlternateScreen)?;
    Ok(())
}

/// Belt-and-suspenders: also restore the terminal from a panic hook, in case
/// unwinding is disabled or something panics while the guard isn't in scope.
fn install_panic_hook() {
    let original_hook = std::panic::take_hook();
    std::panic::set_hook(Box::new(move |panic_info| {
        let _ = restore_terminal();
        original_hook(panic_info);
    }));
}

fn run_app(terminal: &mut Terminal<CrosstermBackend<Stdout>>, app: &mut App) -> Result<()> {
    loop {
        terminal.draw(|f| render(f, app))?;

        // Drain subprocess output and detect completion; refresh on finish.
        if let Some(op) = app.op.as_mut() {
            let was_running = op.running();
            op.poll();
            if was_running && op.finished() {
                app.refresh_after_op()?;
                app.start_next_if_idle()?; // launch the next queued op, if any
            }
        }

        app.tick = app.tick.wrapping_add(1);

        if !event::poll(Duration::from_millis(200))? {
            continue;
        }
        let key = match event::read()? {
            Event::Key(k) => k,
            _ => continue,
        };
        if key.kind != KeyEventKind::Press {
            continue;
        }
        let code = key.code;

        // 1. Help overlay swallows everything until dismissed.
        if app.help_open {
            if matches!(code, KeyCode::Char('?') | KeyCode::Esc) {
                app.help_open = false;
            }
            continue;
        }

        // 2. Quit-while-running confirmation.
        if app.quit_confirm {
            match code {
                KeyCode::Char('y') | KeyCode::Char('Y') => {
                    if let Some(op) = app.op.as_mut() {
                        op.kill();
                    }
                    return Ok(());
                }
                _ => app.quit_confirm = false,
            }
            continue;
        }

        // 3. Finished-op log pane owns Esc/PgUp/PgDn until closed.
        if !app.filter_mode && app.op.as_ref().is_some_and(|o| o.finished()) {
            match code {
                KeyCode::Esc => {
                    app.op = None;
                    continue;
                }
                KeyCode::PageUp => {
                    if let Some(op) = app.op.as_mut() {
                        op.scroll_page_up();
                    }
                    continue;
                }
                KeyCode::PageDown => {
                    if let Some(op) = app.op.as_mut() {
                        op.scroll_page_down();
                    }
                    continue;
                }
                _ => {}
            }
        }

        // 4. Active-tab key handling. Returns true when it consumed the key.
        let consumed = match app.tab {
            Tab::Packages => tabs::packages::handle_key(app, code)?,
            Tab::System => tabs::system::handle_key(app, code)?,
            Tab::Tsi => tabs::tsi::handle_key(app, code)?,
        };
        if consumed {
            continue;
        }

        // 5. Global keys.
        match code {
            KeyCode::Char('1') => app.tab = Tab::Packages,
            KeyCode::Char('2') => app.tab = Tab::System,
            KeyCode::Char('3') => app.tab = Tab::Tsi,
            KeyCode::Char('?') => app.help_open = true,
            KeyCode::Char('q') => {
                if app.op_running() {
                    app.quit_confirm = true;
                } else {
                    return Ok(());
                }
            }
            _ => {}
        }
    }
}

fn render(f: &mut Frame, app: &mut App) {
    let root = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1), // tab bar
            Constraint::Min(0),    // body (content + optional log)
            Constraint::Length(1), // footer
        ])
        .split(f.area());

    render_tab_bar(f, app, root[0]);

    // Split the body into tab content and (optionally) the log pane.
    let body = root[1];
    let content_area = if app.op.is_some() {
        let log_h = log_pane_height(body.height);
        let split = Layout::default()
            .direction(Direction::Vertical)
            .constraints([Constraint::Min(0), Constraint::Length(log_h)])
            .split(body);
        if let Some(op) = app.op.as_mut() {
            runner::render_log(f, op, app.tick, split[1]);
        }
        split[0]
    } else {
        body
    };

    match app.tab {
        Tab::Packages => tabs::packages::render(f, app, content_area),
        Tab::System => tabs::system::render(f, app, content_area),
        Tab::Tsi => tabs::tsi::render(f, app, content_area),
    }

    render_footer(f, app, root[2]);

    if app.quit_confirm {
        render_quit_confirm(f, f.area());
    }
    if app.help_open {
        render_help(f, f.area());
    }
}

fn render_tab_bar(f: &mut Frame, app: &App, area: Rect) {
    let cols = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Min(0), Constraint::Length(14)])
        .split(area);

    let mut spans = vec![Span::raw(" ")];
    for (i, tab) in Tab::all().iter().enumerate() {
        if i > 0 {
            spans.push(Span::raw("  "));
        }
        let label = format!("{}{} ", superscript(i + 1), tab.label());
        if *tab == app.tab {
            spans.push(Span::styled(label, theme::accent_bold()));
        } else {
            spans.push(Span::styled(label, theme::dim()));
        }
    }
    f.render_widget(Paragraph::new(Line::from(spans)), cols[0]);

    let version = format!("tsi v{} ", env!("CARGO_PKG_VERSION"));
    let right =
        Paragraph::new(Line::from(Span::styled(version, theme::dim()))).alignment(Alignment::Right);
    f.render_widget(right, cols[1]);
}

/// Unicode superscript digit prefix for a tab number (1..=3).
fn superscript(n: usize) -> &'static str {
    match n {
        1 => "\u{00b9}",
        2 => "\u{00b2}",
        3 => "\u{00b3}",
        _ => "",
    }
}

fn render_footer(f: &mut Frame, app: &App, area: Rect) {
    let mut spans: Vec<Span<'static>> = Vec::new();

    if let Some(action) = &app.pending_confirm {
        let style = if action.warns() {
            theme::warn()
        } else {
            theme::accent()
        };
        spans.push(Span::styled(action.prompt(), style));
    } else if app.op_running() {
        spans.push(Span::styled("operation running", theme::dim()));
        if app.queued() > 0 {
            spans.push(Span::styled(
                format!("  ·  {} queued", app.queued()),
                theme::accent(),
            ));
        }
    } else {
        let mut hints: Vec<(&str, &str)> = match app.tab {
            Tab::Packages => vec![
                ("space", "mark"),
                ("i", "install"),
                ("r", "remove"),
                ("u", "upgrade"),
            ],
            Tab::System => vec![("d", "doctor")],
            Tab::Tsi => vec![("enter", "run")],
        };
        hints.extend_from_slice(&[("1/2/3", "tabs"), ("?", "help"), ("q", "quit")]);
        spans.extend(theme::footer_hints(&hints));
    }

    let footer = Paragraph::new(Line::from(spans));
    f.render_widget(footer, area);
}

/// Height of the log pane: ~40% of the body (min 5 rows), but never so tall it
/// crowds out the content pane on a short terminal. The `max` before `min`
/// avoids a `clamp` min>max panic when `body_height` is small.
fn log_pane_height(body_height: u16) -> u16 {
    ((body_height * 2 / 5).max(5)).min(body_height.saturating_sub(3))
}

/// Returns a `Rect` of `percent_x`% x `percent_y`% centered within `area`.
fn centered_rect(percent_x: u16, percent_y: u16, area: Rect) -> Rect {
    let vertical = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Percentage((100 - percent_y) / 2),
            Constraint::Percentage(percent_y),
            Constraint::Percentage((100 - percent_y) / 2),
        ])
        .split(area);
    Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Percentage((100 - percent_x) / 2),
            Constraint::Percentage(percent_x),
            Constraint::Percentage((100 - percent_x) / 2),
        ])
        .split(vertical[1])[1]
}

fn render_quit_confirm(f: &mut Frame, area: Rect) {
    let popup = centered_rect(50, 20, area);
    let block = theme::panel(theme::panel_title("quit"), true);
    let text = vec![
        Line::from(""),
        Line::from(Span::styled(
            "An operation is still running.",
            Style::default(),
        )),
        Line::from(Span::styled("Quit and kill it? y/N", theme::accent())),
    ];
    let paragraph = Paragraph::new(text)
        .block(block)
        .alignment(Alignment::Center)
        .wrap(Wrap { trim: true });
    f.render_widget(Clear, popup);
    f.render_widget(paragraph, popup);
}

fn render_help(f: &mut Frame, area: Rect) {
    let popup = centered_rect(64, 74, area);

    let heading = |t: &str| Line::from(Span::styled(t.to_string(), theme::accent_bold()));
    let row = |key: &str, desc: &str| {
        Line::from(vec![
            Span::styled(format!("  {key:<16}"), theme::accent()),
            Span::styled(desc.to_string(), Style::default()),
        ])
    };

    let lines = vec![
        heading("Tabs"),
        row("1 / 2 / 3", "Switch to Packages / System / TSI"),
        Line::from(""),
        heading("Navigation"),
        row("↑/↓, j/k", "Move selection"),
        row("PgUp/PgDn", "Move by 10 (scroll log when open)"),
        row("Home/g, End/G", "Jump to first / last"),
        Line::from(""),
        heading("Filter & views"),
        row("/", "Filter packages"),
        row("Tab", "Cycle view (all/installed/available)"),
        row("Esc", "Clear filter"),
        Line::from(""),
        heading("Actions"),
        row("space", "Mark / unmark package for batch"),
        row("i", "Install selected (or all marked)"),
        row("r", "Remove selected (or all marked)"),
        row("u", "Upgrade selected (or all marked)"),
        row("y / n", "Confirm / cancel a pending action"),
        Line::from(Span::styled(
            "  a removal that other installed packages need names them",
            theme::dim(),
        )),
        Line::from(Span::styled(
            "  in the prompt and forces only with your confirmation",
            theme::dim(),
        )),
        Line::from(""),
        heading("Operations"),
        row("", "Batch actions queue and run one at a time"),
        row("PgUp/PgDn", "Scroll finished log"),
        row("Esc", "Close finished log pane"),
        row("q", "Quit (confirms if an op is running)"),
        row("?", "Toggle this help"),
    ];

    let block = theme::panel(theme::panel_title("help"), true);
    let paragraph = Paragraph::new(lines)
        .block(block)
        .wrap(Wrap { trim: false });
    f.render_widget(Clear, popup);
    f.render_widget(paragraph, popup);
}

#[cfg(test)]
mod tests {
    use super::*;

    fn write_pkg(dir: &std::path::Path, name: &str, version: &str) {
        let json = format!(
            r#"{{"name":"{name}","version":"{version}",
                 "source":{{"type":"tarball","url":"https://e/x.tar.gz"}}}}"#
        );
        std::fs::write(dir.join(format!("{name}.json")), json).unwrap();
    }

    /// An App rooted at a real temp prefix, so registry reloads hit the filesystem.
    fn app_at(prefix: &std::path::Path, names: &[&str]) -> App {
        let packages_dir = prefix.join("packages");
        std::fs::create_dir_all(&packages_dir).unwrap();
        for n in names {
            write_pkg(&packages_dir, n, "1.0.0");
        }
        let registry = Registry::load_from_dir(&packages_dir).unwrap();
        let db = Database::new(&prefix.join("db")).unwrap();
        App::new(prefix.to_path_buf(), collect_packages(&registry), db)
    }

    #[test]
    fn refresh_registry_picks_up_definitions_added_by_tsi_update() {
        let temp = tempfile::tempdir().unwrap();
        let mut app = app_at(temp.path(), &["curl"]);
        assert_eq!(app.packages.len(), 1);

        // A `tsi update` run in the log pane drops new definitions into the prefix.
        write_pkg(&temp.path().join("packages"), "jq", "1.7.1");
        app.refresh_registry();

        let names: Vec<&str> = app.packages.iter().map(|p| p.name.as_str()).collect();
        assert_eq!(names, vec!["curl", "jq"]);
    }

    #[test]
    fn refresh_registry_keeps_the_old_snapshot_when_definitions_vanish() {
        let temp = tempfile::tempdir().unwrap();
        let mut app = app_at(temp.path(), &["curl"]);

        // Mid-update (or after `tsi remove`) the directory can be empty — the list
        // must not blank out.
        std::fs::remove_file(temp.path().join("packages/curl.json")).unwrap();
        app.refresh_registry();
        assert_eq!(app.packages.len(), 1, "kept the previous snapshot");

        std::fs::remove_dir_all(temp.path().join("packages")).unwrap();
        app.refresh_registry();
        assert_eq!(app.packages.len(), 1);
    }

    #[test]
    fn refresh_after_op_keeps_the_cursor_on_the_same_package() {
        let temp = tempfile::tempdir().unwrap();
        let mut app = app_at(temp.path(), &["curl", "jq", "zlib"]);
        app.apply_filter();
        app.selected = 2; // zlib
        assert_eq!(app.selected_package().unwrap().name, "zlib");

        // A new definition sorts ahead of zlib and would otherwise shift it down.
        write_pkg(&temp.path().join("packages"), "aaa", "1.0.0");
        app.refresh_after_op().unwrap();

        assert_eq!(app.packages.len(), 4);
        assert_eq!(
            app.selected_package().unwrap().name,
            "zlib",
            "cursor followed the package, not the index"
        );
    }

    #[test]
    fn refresh_after_op_survives_the_selected_package_disappearing() {
        let temp = tempfile::tempdir().unwrap();
        let mut app = app_at(temp.path(), &["curl", "jq"]);
        app.apply_filter();
        app.selected = 1;

        std::fs::remove_file(temp.path().join("packages/jq.json")).unwrap();
        app.refresh_after_op().unwrap();
        assert!(app.selected < app.filtered.len().max(1));
    }

    #[test]
    fn version_bump_in_the_registry_is_visible_after_refresh() {
        let temp = tempfile::tempdir().unwrap();
        let mut app = app_at(temp.path(), &["curl"]);
        write_pkg(&temp.path().join("packages"), "curl", "9.9.9");

        app.refresh_registry();
        assert_eq!(app.packages[0].version, "9.9.9");
    }

    /// Draws one frame into an off-screen buffer. Layout math that underflows or
    /// splits a zero-sized `Rect` panics inside `draw`, so "it rendered" is the
    /// assertion — this is what catches a TUI that dies on a small terminal.
    fn draw(app: &mut App, width: u16, height: u16) -> String {
        let backend = ratatui::backend::TestBackend::new(width, height);
        let mut terminal = Terminal::new(backend).unwrap();
        terminal.draw(|f| render(f, app)).unwrap();
        terminal
            .backend()
            .buffer()
            .content()
            .iter()
            .map(|c| c.symbol())
            .collect()
    }

    #[test]
    fn every_tab_renders_at_sizes_down_to_one_cell() {
        let temp = tempfile::tempdir().unwrap();
        let mut app = app_at(temp.path(), &["curl", "jq", "zlib"]);
        app.apply_filter();

        for tab in Tab::all() {
            app.tab = tab;
            for (w, h) in [(1, 1), (2, 3), (20, 4), (40, 10), (80, 24), (200, 60)] {
                draw(&mut app, w, h);
            }
        }
    }

    #[test]
    fn overlays_render_without_clipping_panics() {
        let temp = tempfile::tempdir().unwrap();
        let mut app = app_at(temp.path(), &["curl"]);
        app.apply_filter();

        app.help_open = true;
        for (w, h) in [(1, 1), (10, 5), (80, 24)] {
            draw(&mut app, w, h);
        }
        app.help_open = false;

        app.quit_confirm = true;
        for (w, h) in [(1, 1), (10, 5), (80, 24)] {
            draw(&mut app, w, h);
        }
        app.quit_confirm = false;

        app.tab = Tab::Tsi;
        app.tsi_confirm = Some(tabs::tsi::TsiConfirm::for_test());
        for (w, h) in [(1, 1), (10, 5), (80, 24)] {
            draw(&mut app, w, h);
        }
    }

    #[test]
    fn packages_tab_renders_an_empty_filter_result() {
        let temp = tempfile::tempdir().unwrap();
        let mut app = app_at(temp.path(), &["curl"]);
        app.filter = "nothing-matches".into();
        app.apply_filter();
        assert!(app.filtered.is_empty());

        let screen = draw(&mut app, 80, 24);
        assert!(screen.contains("no package matches"), "{screen}");
    }

    #[test]
    fn a_breaking_removal_prompt_reaches_the_footer() {
        let temp = tempfile::tempdir().unwrap();
        let mut app = app_at(temp.path(), &["curl", "zlib"]);
        app.apply_filter();
        app.db
            .add("zlib", "1.0.0", &temp.path().join("install/zlib"), &[])
            .unwrap();
        app.db
            .add(
                "curl",
                "1.0.0",
                &temp.path().join("install/curl"),
                &["zlib".to_string()],
            )
            .unwrap();

        // Select zlib and ask to remove it: curl depends on it.
        app.apply_filter();
        app.selected = app
            .filtered
            .iter()
            .position(|&i| app.packages[i].name == "zlib")
            .unwrap();
        tabs::packages::handle_key(&mut app, KeyCode::Char('r')).unwrap();

        let screen = draw(&mut app, 120, 24);
        assert!(screen.contains("breaks curl"), "{screen}");
    }

    #[test]
    fn log_pane_height_never_exceeds_body_and_never_panics() {
        for h in 0u16..=200 {
            let lh = log_pane_height(h);
            assert!(lh <= h, "log pane {lh} taller than body {h}");
        }
        // Roomy terminal: ~40% of the body.
        assert_eq!(log_pane_height(40), 16);
        // Tall enough to honor the 5-row minimum.
        assert_eq!(log_pane_height(12), 5);
        // Cramped: capped so ≥3 rows remain for content.
        assert_eq!(log_pane_height(6), 3);
    }
}
