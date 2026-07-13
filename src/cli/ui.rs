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
use ratatui::widgets::{Block, Borders, List, ListItem, ListState, Paragraph, Wrap};
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

struct App {
    prefix: PathBuf,
    packages: Vec<Package>,
    db: Database,
    filtered: Vec<usize>,
    selected: usize,
    filter: String,
    filter_mode: bool,
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
        }
    }

    fn apply_filter(&mut self) {
        let q = self.filter.to_lowercase();
        self.filtered = self
            .packages
            .iter()
            .enumerate()
            .filter(|(_, p)| {
                q.is_empty()
                    || p.name.to_lowercase().contains(&q)
                    || p.description.to_lowercase().contains(&q)
            })
            .map(|(i, _)| i)
            .collect();
        if self.selected >= self.filtered.len() {
            self.selected = self.filtered.len().saturating_sub(1);
        }
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

        if app.filter_mode {
            match key.code {
                KeyCode::Esc => {
                    app.filter.clear();
                    app.filter_mode = false;
                    app.apply_filter();
                }
                KeyCode::Enter => app.filter_mode = false,
                KeyCode::Backspace => {
                    app.filter.pop();
                    app.apply_filter();
                }
                KeyCode::Char(c) => {
                    app.filter.push(c);
                    app.apply_filter();
                }
                _ => {}
            }
            continue;
        }

        match key.code {
            KeyCode::Char('q') => return Ok(()),
            KeyCode::Up | KeyCode::Char('k') => app.select_prev(),
            KeyCode::Down | KeyCode::Char('j') => app.select_next(),
            KeyCode::Char('/') => app.filter_mode = true,
            KeyCode::Esc => {
                if !app.filter.is_empty() {
                    app.filter.clear();
                    app.apply_filter();
                }
            }
            KeyCode::Char('i') => {
                if let Some(pkg) = app.selected_package().cloned() {
                    run_install(terminal, &app.prefix, &pkg.spec())?;
                    app.refresh_db()?;
                }
            }
            KeyCode::Char('u') => {
                if let Some(pkg) = app.selected_package().cloned() {
                    run_uninstall(terminal, &app.prefix, &pkg.name)?;
                    app.refresh_db()?;
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

    let title = format!("Packages ({}/{})", app.filtered.len(), app.packages.len());
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
        format!("/{}", app.filter)
    } else if !app.filter.is_empty() {
        format!("Filter: {} (Esc to clear)", app.filter)
    } else {
        String::new()
    };
    let line2 = "Up/Down or j/k: navigate   /: filter   i: install   u: uninstall   q: quit";
    let paragraph = Paragraph::new(vec![Line::from(line1), Line::from(line2)])
        .block(Block::default().borders(Borders::ALL));
    f.render_widget(paragraph, area);
}
