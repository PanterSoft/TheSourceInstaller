use crate::cli::install::{self, InstallArgs};
use crate::cli::uninstall::{self, UninstallArgs};
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
use ratatui::layout::{Constraint, Direction, Layout, Rect};
use ratatui::style::{Color, Modifier, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, Borders, Clear, List, ListItem, ListState, Paragraph, Wrap};
use ratatui::{Frame, Terminal};
use std::io::{stdout, Stdout};
use std::path::PathBuf;
use std::time::Duration;

#[derive(Args)]
pub struct UiArgs {
    #[arg(long)]
    pub prefix: Option<String>,
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

/// Which subset of packages the list view shows.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
enum View {
    #[default]
    All,
    Installed,
    Available,
}

impl View {
    fn next(self) -> Self {
        match self {
            View::All => View::Installed,
            View::Installed => View::Available,
            View::Available => View::All,
        }
    }

    fn label(self) -> &'static str {
        match self {
            View::All => "All",
            View::Installed => "Installed",
            View::Available => "Available",
        }
    }
}

/// An install/uninstall action awaiting y/N confirmation.
#[derive(Clone)]
enum PendingAction {
    Install(Package),
    Uninstall(Package),
}

impl PendingAction {
    fn prompt(&self) -> String {
        match self {
            PendingAction::Install(pkg) => format!("Install {} {}? y/N", pkg.name, pkg.version),
            PendingAction::Uninstall(pkg) => {
                format!("Uninstall {} {}? y/N", pkg.name, pkg.version)
            }
        }
    }
}

/// Pure filtering logic: which package indices match the current filter text
/// and view, given an `is_installed` predicate. Kept free of `App`/`Database`
/// so it can be unit tested directly.
fn compute_filtered(
    packages: &[Package],
    filter: &str,
    view: View,
    is_installed: impl Fn(&str) -> bool,
) -> Vec<usize> {
    let q = filter.to_lowercase();
    packages
        .iter()
        .enumerate()
        .filter(|(_, p)| {
            let matches_view = match view {
                View::All => true,
                View::Installed => is_installed(&p.name),
                View::Available => !is_installed(&p.name),
            };
            matches_view
                && (q.is_empty()
                    || p.name.to_lowercase().contains(&q)
                    || p.description.to_lowercase().contains(&q))
        })
        .map(|(i, _)| i)
        .collect()
}

struct App {
    prefix: PathBuf,
    packages: Vec<Package>,
    db: Database,
    filtered: Vec<usize>,
    selected: usize,
    filter: String,
    filter_mode: bool,
    view: View,
    help_open: bool,
    pending_confirm: Option<PendingAction>,
}

impl App {
    fn new(prefix: PathBuf, packages: Vec<Package>, db: Database) -> Self {
        let filtered = (0..packages.len()).collect();
        Self {
            prefix,
            packages,
            db,
            filtered,
            selected: 0,
            filter: String::new(),
            filter_mode: false,
            view: View::All,
            help_open: false,
            pending_confirm: None,
        }
    }

    fn apply_filter(&mut self) {
        let db = &self.db;
        self.filtered = compute_filtered(&self.packages, &self.filter, self.view, |name| {
            db.is_installed(name)
        });
        if self.selected >= self.filtered.len() {
            self.selected = self.filtered.len().saturating_sub(1);
        }
    }

    /// Filter text (or view) changed: recompute and jump back to the top.
    fn refilter_from_top(&mut self) {
        self.selected = 0;
        self.apply_filter();
    }

    fn selected_package(&self) -> Option<&Package> {
        self.filtered.get(self.selected).map(|&i| &self.packages[i])
    }

    fn select_next(&mut self) {
        if self.filtered.is_empty() {
            return;
        }
        self.selected = (self.selected + 1).min(self.filtered.len() - 1);
    }

    fn select_prev(&mut self) {
        self.selected = self.selected.saturating_sub(1);
    }

    fn select_page_down(&mut self) {
        if self.filtered.is_empty() {
            return;
        }
        self.selected = (self.selected + 10).min(self.filtered.len() - 1);
    }

    fn select_page_up(&mut self) {
        self.selected = self.selected.saturating_sub(10);
    }

    fn select_first(&mut self) {
        self.selected = 0;
    }

    fn select_last(&mut self) {
        self.selected = self.filtered.len().saturating_sub(1);
    }

    fn refresh_db(&mut self) -> Result<()> {
        self.db.load()
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

fn suspend_terminal() -> Result<()> {
    disable_raw_mode()?;
    execute!(stdout(), LeaveAlternateScreen)?;
    Ok(())
}

fn resume_terminal() -> Result<()> {
    enable_raw_mode()?;
    execute!(stdout(), EnterAlternateScreen)?;
    Ok(())
}

fn run_app(terminal: &mut Terminal<CrosstermBackend<Stdout>>, app: &mut App) -> Result<()> {
    loop {
        terminal.draw(|f| render(f, app))?;

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

        if app.help_open {
            match key.code {
                KeyCode::Char('?') | KeyCode::Esc => app.help_open = false,
                _ => {}
            }
            continue;
        }

        if let Some(action) = app.pending_confirm.take() {
            match key.code {
                KeyCode::Char('y') | KeyCode::Char('Y') => match action {
                    PendingAction::Install(pkg) => {
                        run_install(terminal, &app.prefix, &pkg.spec())?;
                        app.refresh_db()?;
                        app.apply_filter();
                    }
                    PendingAction::Uninstall(pkg) => {
                        run_uninstall(terminal, &app.prefix, &pkg.name)?;
                        app.refresh_db()?;
                        app.apply_filter();
                    }
                },
                _ => {}
            }
            continue;
        }

        if app.filter_mode {
            match key.code {
                KeyCode::Esc => {
                    app.filter.clear();
                    app.filter_mode = false;
                    app.refilter_from_top();
                }
                KeyCode::Enter => app.filter_mode = false,
                KeyCode::Backspace => {
                    app.filter.pop();
                    app.refilter_from_top();
                }
                KeyCode::Char(c) => {
                    app.filter.push(c);
                    app.refilter_from_top();
                }
                _ => {}
            }
            continue;
        }

        match key.code {
            KeyCode::Char('q') => return Ok(()),
            KeyCode::Char('?') => app.help_open = true,
            KeyCode::Up | KeyCode::Char('k') => app.select_prev(),
            KeyCode::Down | KeyCode::Char('j') => app.select_next(),
            KeyCode::PageUp => app.select_page_up(),
            KeyCode::PageDown => app.select_page_down(),
            KeyCode::Home | KeyCode::Char('g') => app.select_first(),
            KeyCode::End | KeyCode::Char('G') => app.select_last(),
            KeyCode::Tab => {
                app.view = app.view.next();
                app.refilter_from_top();
            }
            KeyCode::Char('/') => app.filter_mode = true,
            KeyCode::Esc => {
                if !app.filter.is_empty() {
                    app.filter.clear();
                    app.refilter_from_top();
                }
            }
            KeyCode::Char('i') => {
                if let Some(pkg) = app.selected_package().cloned() {
                    app.pending_confirm = Some(PendingAction::Install(pkg));
                }
            }
            KeyCode::Char('u') => {
                if let Some(pkg) = app.selected_package().cloned() {
                    if app.db.is_installed(&pkg.name) {
                        app.pending_confirm = Some(PendingAction::Uninstall(pkg));
                    }
                }
            }
            _ => {}
        }
    }
}

/// Leaves the TUI, runs the real `tsi install` code path with normal
/// streaming output, waits for the user to acknowledge, then resumes the TUI.
fn run_install(
    terminal: &mut Terminal<CrosstermBackend<Stdout>>,
    prefix: &std::path::Path,
    spec: &str,
) -> Result<()> {
    suspend_terminal()?;
    let install_args = InstallArgs {
        packages: vec![spec.to_string()],
        force: false,
        prefix: Some(prefix.to_string_lossy().to_string()),
        verbose: false,
    };
    if let Err(e) = install::run(install_args) {
        term::output::error(format!("Install failed: {e}"));
    }
    wait_for_enter();
    resume_terminal()?;
    terminal.clear()?;
    Ok(())
}

/// Same suspend/run/resume dance as `run_install`, but for uninstall.
fn run_uninstall(
    terminal: &mut Terminal<CrosstermBackend<Stdout>>,
    prefix: &std::path::Path,
    name: &str,
) -> Result<()> {
    suspend_terminal()?;
    let uninstall_args = UninstallArgs {
        packages: vec![name.to_string()],
        prefix: Some(prefix.to_string_lossy().to_string()),
    };
    if let Err(e) = uninstall::run(uninstall_args) {
        term::output::error(format!("Uninstall failed: {e}"));
    }
    wait_for_enter();
    resume_terminal()?;
    terminal.clear()?;
    Ok(())
}

fn wait_for_enter() {
    use std::io::Write;
    println!("\nPress Enter to return to tsi ui...");
    let _ = std::io::stdout().flush();
    let mut buf = String::new();
    let _ = std::io::stdin().read_line(&mut buf);
}

fn render(f: &mut Frame, app: &mut App) {
    let root = Layout::default()
        .direction(Direction::Vertical)
        .constraints([Constraint::Min(0), Constraint::Length(3)])
        .split(f.area());

    let main = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([Constraint::Percentage(40), Constraint::Percentage(60)])
        .split(root[0]);

    render_list(f, app, main[0]);
    render_details(f, app, main[1]);
    render_bottom_bar(f, app, root[1]);

    if app.help_open {
        render_help(f, f.area());
    }
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

fn render_help(f: &mut Frame, area: Rect) {
    let popup = centered_rect(60, 60, area);
    let lines = vec![
        Line::from(Span::styled(
            "Keybindings",
            Style::default().add_modifier(Modifier::BOLD),
        )),
        Line::from(""),
        Line::from("Up/Down, j/k      Move selection"),
        Line::from("PageUp/PageDown   Move selection by 10"),
        Line::from("Home/g, End/G     Jump to first/last"),
        Line::from("Tab               Cycle view (All/Installed/Available)"),
        Line::from("/                 Filter packages"),
        Line::from("i                 Install selected package"),
        Line::from("u                 Uninstall selected package"),
        Line::from("Esc               Cancel filter/confirm"),
        Line::from("q                 Quit"),
        Line::from("?                 Toggle this help"),
    ];
    let paragraph = Paragraph::new(lines)
        .block(Block::default().borders(Borders::ALL).title("Help"))
        .wrap(Wrap { trim: false });
    f.render_widget(Clear, popup);
    f.render_widget(paragraph, popup);
}

fn render_list(f: &mut Frame, app: &App, area: Rect) {
    let items: Vec<ListItem> = app
        .filtered
        .iter()
        .map(|&i| {
            let pkg = &app.packages[i];
            let installed = app.db.is_installed(&pkg.name);
            let marker = if installed { "*" } else { " " };
            let line = format!("{} {:<24} {}", marker, pkg.name, pkg.version);
            let style = if installed {
                Style::default().fg(Color::Green)
            } else {
                Style::default()
            };
            ListItem::new(Line::from(Span::styled(line, style)))
        })
        .collect();

    let title = format!(
        "Packages — {} ({}/{})",
        app.view.label(),
        app.filtered.len(),
        app.packages.len()
    );
    let list = List::new(items)
        .block(Block::default().borders(Borders::ALL).title(title))
        .highlight_style(Style::default().add_modifier(Modifier::REVERSED))
        .highlight_symbol("> ");

    let mut state = ListState::default();
    if !app.filtered.is_empty() {
        state.select(Some(app.selected));
    }
    f.render_stateful_widget(list, area, &mut state);
}

fn render_details(f: &mut Frame, app: &App, area: Rect) {
    let text: Vec<Line> = if let Some(pkg) = app.selected_package() {
        let mut lines = vec![
            Line::from(Span::styled(
                format!("{} {}", pkg.name, pkg.version),
                Style::default().add_modifier(Modifier::BOLD),
            )),
            Line::from(""),
        ];
        if !pkg.description.is_empty() {
            lines.push(Line::from(pkg.description.clone()));
            lines.push(Line::from(""));
        }
        if !pkg.build_system.is_empty() {
            lines.push(Line::from(format!("Build system: {}", pkg.build_system)));
        }
        if let Some(url) = &pkg.source.url {
            lines.push(Line::from(format!("Source: {}", url)));
        }
        if !pkg.dependencies.is_empty() {
            lines.push(Line::from(""));
            lines.push(Line::from(format!(
                "Dependencies: {}",
                pkg.dependencies.join(", ")
            )));
        }
        if !pkg.build_dependencies.is_empty() {
            lines.push(Line::from(format!(
                "Build dependencies: {}",
                pkg.build_dependencies.join(", ")
            )));
        }
        lines.push(Line::from(""));
        match app.db.get(&pkg.name) {
            Some(info) => {
                lines.push(Line::from(Span::styled(
                    format!("Installed: yes ({})", info.version),
                    Style::default().fg(Color::Green),
                )));
                lines.push(Line::from(format!("Path: {}", info.install_path)));
            }
            None => {
                lines.push(Line::from(Span::styled(
                    "Installed: no",
                    Style::default().fg(Color::DarkGray),
                )));
            }
        }
        lines
    } else {
        vec![Line::from("No package matches the current filter")]
    };

    let paragraph = Paragraph::new(text)
        .block(Block::default().borders(Borders::ALL).title("Details"))
        .wrap(Wrap { trim: false });
    f.render_widget(paragraph, area);
}

fn render_bottom_bar(f: &mut Frame, app: &App, area: Rect) {
    let line1 = if app.filter_mode {
        format!("/{}\u{2588}", app.filter)
    } else if let Some(action) = &app.pending_confirm {
        action.prompt()
    } else if !app.filter.is_empty() {
        format!("Filter: {} (Esc to clear)", app.filter)
    } else {
        String::new()
    };
    let line2 = match app.selected_package() {
        Some(pkg) if app.db.is_installed(&pkg.name) => {
            "Up/Down, PgUp/PgDn, Home/g, End/G: navigate   Tab: view   /: filter   u: uninstall   q: quit   ?: help"
        }
        Some(_) => {
            "Up/Down, PgUp/PgDn, Home/g, End/G: navigate   Tab: view   /: filter   i: install   q: quit   ?: help"
        }
        None => {
            "Up/Down, PgUp/PgDn, Home/g, End/G: navigate   Tab: view   /: filter   q: quit   ?: help"
        }
    };
    let paragraph = Paragraph::new(vec![Line::from(line1), Line::from(line2)])
        .block(Block::default().borders(Borders::ALL));
    f.render_widget(paragraph, area);
}

#[cfg(test)]
mod tests {
    use super::*;

    fn make_package(name: &str, description: &str) -> Package {
        let version: crate::core::package::PackageVersion = serde_json::from_value(serde_json::json!({
            "version": "1.0.0",
            "description": description,
            "source": { "type": "archive", "url": "https://example.com/pkg.tar.gz" },
        }))
        .expect("valid PackageVersion json");
        Package::from_version(name, &version)
    }

    fn test_app(names: &[&str]) -> App {
        let dir = tempfile::tempdir().expect("tempdir");
        let db = Database::new(&dir.path().join("db")).expect("db");
        let packages: Vec<Package> = names.iter().map(|n| make_package(n, "")).collect();
        App::new(PathBuf::from("/tmp"), packages, db)
    }

    #[test]
    fn compute_filtered_all_view_matches_everything() {
        let packages = vec![make_package("zlib", "compression"), make_package("curl", "http client")];
        let idx = compute_filtered(&packages, "", View::All, |_| false);
        assert_eq!(idx, vec![0, 1]);
    }

    #[test]
    fn compute_filtered_by_name_and_description() {
        let packages = vec![make_package("zlib", "compression"), make_package("curl", "http client")];
        let idx = compute_filtered(&packages, "http", View::All, |_| false);
        assert_eq!(idx, vec![1]);
    }

    #[test]
    fn compute_filtered_installed_view() {
        let packages = vec![make_package("zlib", ""), make_package("curl", "")];
        let idx = compute_filtered(&packages, "", View::Installed, |n| n == "zlib");
        assert_eq!(idx, vec![0]);
    }

    #[test]
    fn compute_filtered_available_view() {
        let packages = vec![make_package("zlib", ""), make_package("curl", "")];
        let idx = compute_filtered(&packages, "", View::Available, |n| n == "zlib");
        assert_eq!(idx, vec![1]);
    }

    #[test]
    fn compute_filtered_view_and_filter_combine() {
        let packages = vec![
            make_package("zlib", ""),
            make_package("curl", ""),
            make_package("curl-extra", ""),
        ];
        let idx = compute_filtered(&packages, "curl", View::Available, |n| n == "zlib");
        assert_eq!(idx, vec![1, 2]);
    }

    #[test]
    fn selection_clamps_when_filtered_shrinks() {
        let mut app = test_app(&["a", "b", "c"]);
        app.selected = 2;
        app.filter = "nomatch".to_string();
        app.apply_filter();
        assert!(app.filtered.is_empty());
        assert_eq!(app.selected, 0);
    }

    #[test]
    fn refilter_from_top_resets_selection() {
        let mut app = test_app(&["a", "b", "c"]);
        app.selected = 2;
        app.filter = "b".to_string();
        app.refilter_from_top();
        assert_eq!(app.selected, 0);
        assert_eq!(app.filtered, vec![1]);
    }

    #[test]
    fn navigation_bounds_are_respected() {
        let mut app = test_app(&["a", "b", "c"]);

        app.select_prev();
        assert_eq!(app.selected, 0, "select_prev at top stays at 0");

        app.select_next();
        app.select_next();
        app.select_next();
        assert_eq!(app.selected, 2, "select_next clamps to last index");

        app.select_first();
        assert_eq!(app.selected, 0);

        app.select_last();
        assert_eq!(app.selected, 2);

        app.select_page_up();
        assert_eq!(app.selected, 0, "page up saturates at 0");

        app.select_page_down();
        assert_eq!(app.selected, 2, "page down clamps to last index");
    }

    #[test]
    fn view_cycles_through_all_variants() {
        assert_eq!(View::All.next(), View::Installed);
        assert_eq!(View::Installed.next(), View::Available);
        assert_eq!(View::Available.next(), View::All);
    }
}
