//! System tab: an at-a-glance overview (prefix, package counts, toolchain
//! version) plus a `d` action that runs the full `tsi doctor` health check in
//! the log pane.

use super::packages::can_upgrade;
use crate::cli::ui::{theme, App};
use crossterm::event::KeyCode;
use ratatui::layout::Rect;
use ratatui::style::{Modifier, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Paragraph, Wrap};
use ratatui::Frame;
use std::collections::HashMap;

/// Counts of installed and upgradable packages, derived from the DB and the
/// registry snapshot. Pure so it can be unit tested.
pub fn package_stats(
    installed: &[(String, String)],
    latest: &HashMap<String, String>,
) -> (usize, usize) {
    let upgradable = installed
        .iter()
        .filter(|(name, ver)| {
            latest
                .get(name)
                .is_some_and(|latest| can_upgrade(ver, latest))
        })
        .count();
    (installed.len(), upgradable)
}

/// Handles a key press for the System tab. Returns `Ok(true)` when consumed.
pub fn handle_key(app: &mut App, code: KeyCode) -> anyhow::Result<bool> {
    match code {
        KeyCode::Char('d') => {
            if app.op_running() {
                return Ok(true);
            }
            let prefix = app.prefix.to_string_lossy().to_string();
            app.start_op(
                "doctor".into(),
                vec!["doctor".into(), "--prefix".into(), prefix],
            )?;
            Ok(true)
        }
        _ => Ok(false),
    }
}

/// Renders the system overview panel.
pub fn render(f: &mut Frame, app: &App, area: Rect) {
    let latest: HashMap<String, String> = app
        .packages
        .iter()
        .map(|p| (p.name.clone(), p.version.clone()))
        .collect();
    let installed: Vec<(String, String)> = app
        .db
        .list()
        .iter()
        .map(|p| (p.name.clone(), p.version.clone()))
        .collect();
    let (installed_count, upgradable) = package_stats(&installed, &latest);

    let field = |label: &str, value: String| {
        Line::from(vec![
            Span::styled(format!("  {label:<16}"), theme::dim()),
            Span::raw(value),
        ])
    };

    let upgradable_line = if upgradable > 0 {
        Line::from(vec![
            Span::styled(format!("  {:<16}", "upgradable"), theme::dim()),
            Span::styled(upgradable.to_string(), theme::accent()),
        ])
    } else {
        field("upgradable", "0".into())
    };

    let lines = vec![
        Line::from(Span::styled(
            "  overview",
            Style::default().add_modifier(Modifier::BOLD),
        )),
        Line::from(""),
        field("prefix", app.prefix.display().to_string()),
        field("tsi version", env!("CARGO_PKG_VERSION").to_string()),
        field("packages", format!("{} available", app.packages.len())),
        field("installed", installed_count.to_string()),
        upgradable_line,
        Line::from(""),
        Line::from(Span::styled(
            "  run a full health check for compiler, toolchain and paths",
            theme::dim(),
        )),
    ];

    let block = theme::panel(theme::panel_title("system"), true)
        .title_bottom(theme::hint_line(&[("d", "doctor")]));
    let paragraph = Paragraph::new(lines).block(block).wrap(Wrap { trim: false });
    f.render_widget(paragraph, area);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn package_stats_counts_installed_and_upgradable() {
        let installed = vec![
            ("zlib".to_string(), "1.3.0".to_string()),
            ("curl".to_string(), "8.9.0".to_string()),
            ("jq".to_string(), "1.7.1".to_string()),
        ];
        let latest: HashMap<String, String> = [
            ("zlib", "1.3.1"), // upgradable
            ("curl", "8.9.0"), // up to date
            ("jq", "1.7.1"),   // up to date
        ]
        .iter()
        .map(|(k, v)| (k.to_string(), v.to_string()))
        .collect();

        let (count, upgradable) = package_stats(&installed, &latest);
        assert_eq!(count, 3);
        assert_eq!(upgradable, 1);
    }

    #[test]
    fn package_stats_ignores_packages_absent_from_registry() {
        let installed = vec![("orphan".to_string(), "1.0.0".to_string())];
        let latest = HashMap::new();
        let (count, upgradable) = package_stats(&installed, &latest);
        assert_eq!(count, 1);
        assert_eq!(upgradable, 0);
    }
}
