//! Packages tab: filterable package list (top) + details panel (bottom),
//! with install/remove/upgrade actions running through the op runner.

use crate::cli::ui::{theme, App};
use crate::core::package::Package;
use crossterm::event::KeyCode;
use ratatui::layout::{Constraint, Direction, Layout, Rect};
use ratatui::style::{Modifier, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{List, ListItem, ListState, Paragraph, Wrap};
use ratatui::Frame;
use std::path::Path;

/// Which subset of packages the list shows (cycled with the Tab key).
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum View {
    #[default]
    All,
    Installed,
    Available,
}

impl View {
    pub fn next(self) -> Self {
        match self {
            View::All => View::Installed,
            View::Installed => View::Available,
            View::Available => View::All,
        }
    }

    pub fn label(self) -> &'static str {
        match self {
            View::All => "all",
            View::Installed => "installed",
            View::Available => "available",
        }
    }
}

/// A package action awaiting inline y/N confirmation.
#[derive(Clone)]
pub enum PendingAction {
    Install(Package),
    Remove(Package),
    Upgrade { pkg: Package, from: String },
}

impl PendingAction {
    pub fn prompt(&self) -> String {
        match self {
            PendingAction::Install(pkg) => format!("Install {} {}? y/N", pkg.name, pkg.version),
            PendingAction::Remove(pkg) => format!("Remove {} {}? y/N", pkg.name, pkg.version),
            PendingAction::Upgrade { pkg, from } => {
                format!("Upgrade {} {} → {}? y/N", pkg.name, from, pkg.version)
            }
        }
    }

    /// Op-runner label and `tsi` subprocess arguments for this action.
    pub fn label_args(&self, prefix: &Path) -> (String, Vec<String>) {
        let prefix = prefix.to_string_lossy().to_string();
        match self {
            PendingAction::Install(pkg) => (
                format!("install {}", pkg.name),
                vec![
                    "install".into(),
                    pkg.spec(),
                    "--prefix".into(),
                    prefix,
                ],
            ),
            PendingAction::Remove(pkg) => (
                format!("remove {}", pkg.name),
                vec![
                    "uninstall".into(),
                    pkg.name.clone(),
                    "--prefix".into(),
                    prefix,
                ],
            ),
            PendingAction::Upgrade { pkg, .. } => (
                format!("upgrade {}", pkg.name),
                vec![
                    "upgrade".into(),
                    pkg.name.clone(),
                    "--prefix".into(),
                    prefix,
                ],
            ),
        }
    }
}

/// A package is upgradable when its installed version differs from the
/// latest registry version.
pub fn can_upgrade(installed: &str, latest: &str) -> bool {
    installed != latest
}

/// Pure filtering logic: which package indices match the current filter text
/// and view, given an `is_installed` predicate. Kept free of `App`/`Database`
/// so it can be unit tested directly.
pub fn compute_filtered(
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

/// Packages-tab state and navigation, kept as `App` methods so the rest of
/// the module (op completion refresh, footer) can reach it.
impl App {
    pub(crate) fn apply_filter(&mut self) {
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

    pub(crate) fn selected_package(&self) -> Option<&Package> {
        self.filtered.get(self.selected).map(|&i| &self.packages[i])
    }

    /// The selected package is installed at a different version than the
    /// latest registry version.
    pub(crate) fn selected_upgradable(&self) -> bool {
        self.selected_package()
            .and_then(|pkg| self.db.get(&pkg.name).map(|i| (i, pkg)))
            .is_some_and(|(info, pkg)| can_upgrade(&info.version, &pkg.version))
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
}

/// Handles a key press for the packages tab. Returns `Ok(true)` when the key
/// was consumed (global handling in `run_app` is skipped).
pub fn handle_key(app: &mut App, code: KeyCode) -> anyhow::Result<bool> {
    if let Some(action) = app.pending_confirm.take() {
        if matches!(code, KeyCode::Char('y') | KeyCode::Char('Y')) {
            let (label, args) = action.label_args(&app.prefix);
            app.start_op(label, args)?;
        }
        return Ok(true);
    }

    if app.filter_mode {
        match code {
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
        return Ok(true);
    }

    match code {
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
            if app.filter.is_empty() {
                return Ok(false);
            }
            app.filter.clear();
            app.refilter_from_top();
        }
        KeyCode::Char('i') => {
            if app.op_running() {
                return Ok(true);
            }
            if let Some(pkg) = app.selected_package().cloned() {
                if !app.db.is_installed(&pkg.name) {
                    app.pending_confirm = Some(PendingAction::Install(pkg));
                }
            }
        }
        KeyCode::Char('r') => {
            if app.op_running() {
                return Ok(true);
            }
            if let Some(pkg) = app.selected_package().cloned() {
                if app.db.is_installed(&pkg.name) {
                    app.pending_confirm = Some(PendingAction::Remove(pkg));
                }
            }
        }
        KeyCode::Char('u') => {
            if app.op_running() {
                return Ok(true);
            }
            if let Some(pkg) = app.selected_package().cloned() {
                if let Some(info) = app.db.get(&pkg.name) {
                    if can_upgrade(&info.version, &pkg.version) {
                        let from = info.version.clone();
                        app.pending_confirm = Some(PendingAction::Upgrade { pkg, from });
                    }
                }
            }
        }
        _ => return Ok(false),
    }
    Ok(true)
}

/// Renders the packages tab: list panel on top (~60%), details below (~40%).
pub fn render(f: &mut Frame, app: &App, area: Rect) {
    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([Constraint::Percentage(60), Constraint::Percentage(40)])
        .split(area);
    render_list(f, app, chunks[0]);
    render_details(f, app, chunks[1]);
}

fn render_list(f: &mut Frame, app: &App, area: Rect) {
    let items: Vec<ListItem> = app
        .filtered
        .iter()
        .map(|&i| {
            let pkg = &app.packages[i];
            let installed = app.db.get(&pkg.name);
            let (dot, dot_style) = match installed {
                Some(_) => ("●", theme::ok()),
                None => ("○", theme::dim()),
            };
            let version = match installed {
                Some(info) if can_upgrade(&info.version, &pkg.version) => {
                    format!("{} → {}", info.version, pkg.version)
                }
                _ => pkg.version.clone(),
            };
            ListItem::new(Line::from(vec![
                Span::styled(dot, dot_style),
                Span::raw(format!(" {:<24} ", pkg.name)),
                Span::styled(version, Style::default()),
            ]))
        })
        .collect();

    let mut title = format!(
        "packages · {} ({}/{})",
        app.view.label(),
        app.filtered.len(),
        app.packages.len()
    );
    if app.filter_mode {
        title.push_str(&format!(" · /{}\u{2588}", app.filter));
    } else if !app.filter.is_empty() {
        title.push_str(&format!(" · /{}", app.filter));
    }

    let mut hints: Vec<(&str, &str)> = Vec::new();
    match app.selected_package() {
        Some(pkg) if app.db.is_installed(&pkg.name) => {
            hints.push(("r", "remove"));
            if app.selected_upgradable() {
                hints.push(("u", "upgrade"));
            }
        }
        Some(_) => hints.push(("i", "install")),
        None => {}
    }
    hints.push(("/", "filter"));
    hints.push(("tab", "view"));

    let block = theme::panel(theme::panel_title(title), true)
        .title_bottom(theme::hint_line(&hints));
    let list = List::new(items)
        .block(block)
        .highlight_style(theme::selection())
        .highlight_symbol("▸ ");

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
        let field = |label: &str, value: String| {
            Line::from(vec![
                Span::styled(format!("{label}: "), theme::dim()),
                Span::raw(value),
            ])
        };
        if !pkg.build_system.is_empty() {
            lines.push(field("build system", pkg.build_system.clone()));
        }
        if let Some(url) = &pkg.source.url {
            lines.push(field("source", url.clone()));
        }
        if !pkg.dependencies.is_empty() {
            lines.push(field("dependencies", pkg.dependencies.join(", ")));
        }
        if !pkg.build_dependencies.is_empty() {
            lines.push(field("build deps", pkg.build_dependencies.join(", ")));
        }
        lines.push(Line::from(""));
        match app.db.get(&pkg.name) {
            Some(info) => {
                lines.push(Line::from(Span::styled(
                    format!("● installed ({})", info.version),
                    theme::ok(),
                )));
                lines.push(field("path", info.install_path.clone()));
            }
            None => {
                lines.push(Line::from(Span::styled("○ not installed", theme::dim())));
            }
        }
        lines
    } else {
        vec![Line::from(Span::styled(
            "no package matches the current filter",
            theme::dim(),
        ))]
    };

    let paragraph = Paragraph::new(text)
        .block(theme::panel(theme::panel_title("details"), false))
        .wrap(Wrap { trim: false });
    f.render_widget(paragraph, area);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::core::database::Database;
    use std::path::PathBuf;

    fn make_package(name: &str, description: &str) -> Package {
        let version: crate::core::package::PackageVersion =
            serde_json::from_value(serde_json::json!({
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
        let packages = vec![
            make_package("zlib", "compression"),
            make_package("curl", "http client"),
        ];
        let idx = compute_filtered(&packages, "", View::All, |_| false);
        assert_eq!(idx, vec![0, 1]);
    }

    #[test]
    fn compute_filtered_by_name_and_description() {
        let packages = vec![
            make_package("zlib", "compression"),
            make_package("curl", "http client"),
        ];
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

    #[test]
    fn can_upgrade_only_when_versions_differ() {
        assert!(can_upgrade("1.0.0", "1.0.1"));
        assert!(!can_upgrade("1.0.0", "1.0.0"));
    }

    #[test]
    fn pending_action_builds_subprocess_args() {
        let pkg = make_package("curl", "");
        let prefix = PathBuf::from("/opt/tsi");

        let (label, args) = PendingAction::Install(pkg.clone()).label_args(&prefix);
        assert_eq!(label, "install curl");
        assert_eq!(args, vec!["install", "curl@1.0.0", "--prefix", "/opt/tsi"]);

        let (label, args) = PendingAction::Remove(pkg.clone()).label_args(&prefix);
        assert_eq!(label, "remove curl");
        assert_eq!(args, vec!["uninstall", "curl", "--prefix", "/opt/tsi"]);

        let (label, args) = PendingAction::Upgrade {
            pkg,
            from: "0.9.0".into(),
        }
        .label_args(&prefix);
        assert_eq!(label, "upgrade curl");
        assert_eq!(args, vec!["upgrade", "curl", "--prefix", "/opt/tsi"]);
    }
}
