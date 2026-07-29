//! TSI tab: a menu of maintenance actions (update definitions, self-update,
//! bootstrap, remove) that each run as a `tsi` subprocess in the log pane.
//! The destructive "remove TSI" action is gated behind a typed `yes`.

use crate::cli::ui::{theme, App};
use crossterm::event::KeyCode;
use ratatui::layout::{Alignment, Constraint, Direction, Layout, Rect};
use ratatui::style::{Modifier, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Clear, List, ListItem, ListState, Paragraph, Wrap};
use ratatui::Frame;
use std::path::Path;

/// A maintenance action offered by the TSI tab.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Action {
    Update,
    SelfUpdate,
    Bootstrap,
    Remove,
}

impl Action {
    const ALL: [Action; 4] = [
        Action::Update,
        Action::SelfUpdate,
        Action::Bootstrap,
        Action::Remove,
    ];

    fn title(self) -> &'static str {
        match self {
            Action::Update => "Update package definitions",
            Action::SelfUpdate => "Self-update the tsi binary",
            Action::Bootstrap => "Install / repair bootstrap toolchain",
            Action::Remove => "Remove TSI (wipes the whole prefix)",
        }
    }

    fn hint(self) -> &'static str {
        match self {
            Action::Update => "fetch the latest package definitions",
            Action::SelfUpdate => "download and replace the running binary",
            Action::Bootstrap => "build the isolated gcc/make toolchain",
            Action::Remove => "delete the prefix and every installed package",
        }
    }

    fn destructive(self) -> bool {
        matches!(self, Action::Remove)
    }

    /// Op-runner label and `tsi` subprocess arguments.
    fn label_args(self, prefix: &Path) -> (String, Vec<String>) {
        let p = prefix.to_string_lossy().to_string();
        match self {
            Action::Update => (
                "update definitions".into(),
                vec!["update".into(), "--prefix".into(), p],
            ),
            Action::SelfUpdate => (
                "self-update".into(),
                vec!["self-update".into(), "--prefix".into(), p],
            ),
            Action::Bootstrap => (
                "bootstrap".into(),
                vec!["bootstrap".into(), "--prefix".into(), p],
            ),
            // --yes: the subprocess has no stdin, so it can't prompt; our own
            // typed-"yes" gate stands in for the interactive confirmation.
            Action::Remove => (
                "remove tsi".into(),
                vec!["remove".into(), "--prefix".into(), p, "--yes".into()],
            ),
        }
    }
}

/// A pending TSI action awaiting confirmation. Destructive actions require the
/// user to type `yes`; the rest are a simple y/N.
pub struct TsiConfirm {
    action: Action,
    /// `Some(buffer)` when a typed `yes` is required; `None` for y/N.
    typed: Option<String>,
}

impl TsiConfirm {
    /// A destructive confirmation (typed-`yes` gate), for render tests.
    #[cfg(test)]
    pub fn for_test() -> Self {
        Self {
            action: Action::Remove,
            typed: Some(String::new()),
        }
    }

    fn prompt(&self) -> Line<'static> {
        match &self.typed {
            Some(buf) => Line::from(vec![
                Span::styled("Type ", Style::default()),
                Span::styled("yes", theme::err()),
                Span::styled(" to confirm: ", Style::default()),
                Span::styled(format!("{buf}\u{2588}"), theme::accent()),
            ]),
            None => Line::from(vec![
                Span::styled(format!("{}? ", self.action.title()), Style::default()),
                Span::styled("y/N", theme::accent()),
            ]),
        }
    }
}

/// Handles a key press for the TSI tab. Returns `Ok(true)` when consumed.
pub fn handle_key(app: &mut App, code: KeyCode) -> anyhow::Result<bool> {
    // A confirmation is open: route input to it.
    if let Some(mut confirm) = app.tsi_confirm.take() {
        match &mut confirm.typed {
            // Typed-"yes" gate for destructive actions.
            Some(buf) => match code {
                KeyCode::Esc => {}
                KeyCode::Enter => {
                    if buf.trim() == "yes" {
                        let (label, args) = confirm.action.label_args(&app.prefix);
                        app.start_op(label, args)?;
                    }
                }
                KeyCode::Backspace => {
                    buf.pop();
                    app.tsi_confirm = Some(confirm);
                }
                KeyCode::Char(c) => {
                    buf.push(c);
                    app.tsi_confirm = Some(confirm);
                }
                _ => app.tsi_confirm = Some(confirm),
            },
            // Simple y/N.
            None => {
                if matches!(code, KeyCode::Char('y') | KeyCode::Char('Y')) {
                    let (label, args) = confirm.action.label_args(&app.prefix);
                    app.start_op(label, args)?;
                }
            }
        }
        return Ok(true);
    }

    match code {
        KeyCode::Up | KeyCode::Char('k') => {
            app.tsi_selected = app.tsi_selected.saturating_sub(1);
        }
        KeyCode::Down | KeyCode::Char('j') => {
            app.tsi_selected = (app.tsi_selected + 1).min(Action::ALL.len() - 1);
        }
        KeyCode::Enter => {
            if app.op_running() {
                return Ok(true);
            }
            let action = Action::ALL[app.tsi_selected];
            app.tsi_confirm = Some(TsiConfirm {
                action,
                typed: if action.destructive() {
                    Some(String::new())
                } else {
                    None
                },
            });
        }
        _ => return Ok(false),
    }
    Ok(true)
}

/// Renders the TSI action menu (top) and the highlighted action's detail
/// (bottom); overlays the confirmation prompt when one is pending.
pub fn render(f: &mut Frame, app: &App, area: Rect) {
    let chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([Constraint::Min(0), Constraint::Length(6)])
        .split(area);

    let items: Vec<ListItem> = Action::ALL
        .iter()
        .map(|a| {
            let style = if a.destructive() {
                theme::err()
            } else {
                Style::default()
            };
            ListItem::new(Line::from(Span::styled(format!("  {}", a.title()), style)))
        })
        .collect();

    let block = theme::panel(theme::panel_title("tsi · maintenance"), true)
        .title_bottom(theme::hint_line(&[("↑↓", "select"), ("enter", "run")]));
    let list = List::new(items)
        .block(block)
        .highlight_style(theme::selection())
        .highlight_symbol("▸ ");
    let mut state = ListState::default();
    state.select(Some(app.tsi_selected));
    f.render_stateful_widget(list, chunks[0], &mut state);

    let action = Action::ALL[app.tsi_selected];
    let detail = vec![
        Line::from(Span::styled(
            action.title().to_string(),
            Style::default().add_modifier(Modifier::BOLD),
        )),
        Line::from(""),
        Line::from(Span::styled(action.hint().to_string(), theme::dim())),
    ];
    let detail_block = theme::panel(theme::panel_title("action"), false);
    f.render_widget(
        Paragraph::new(detail)
            .block(detail_block)
            .wrap(Wrap { trim: true }),
        chunks[1],
    );

    if let Some(confirm) = &app.tsi_confirm {
        render_confirm(f, confirm, area);
    }
}

fn render_confirm(f: &mut Frame, confirm: &TsiConfirm, area: Rect) {
    let popup = super::super::centered_rect(60, 24, area);
    let focused = confirm.action.destructive();
    let block = theme::panel(theme::panel_title("confirm"), true);
    let text = vec![
        Line::from(""),
        confirm.prompt(),
        Line::from(""),
        Line::from(Span::styled(
            if focused {
                "Esc to cancel · Enter to submit"
            } else {
                "any other key cancels"
            },
            theme::dim(),
        )),
    ];
    f.render_widget(Clear, popup);
    f.render_widget(
        Paragraph::new(text)
            .block(block)
            .alignment(Alignment::Center)
            .wrap(Wrap { trim: true }),
        popup,
    );
}
