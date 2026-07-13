//! btop-inspired visual style: rounded thin borders, a restrained palette,
//! and helpers so every panel in the TUI looks consistent.

use ratatui::style::{Color, Modifier, Style};
use ratatui::text::{Line, Span};
use ratatui::widgets::{Block, BorderType, Borders};

/// Accent color for the focused panel border, selection, active tab, keys.
pub const ACCENT: Color = Color::Cyan;
/// De-emphasized elements: inactive borders, separators, hints.
pub const MUTED: Color = Color::DarkGray;
/// Installed packages / successful operations.
pub const OK: Color = Color::Green;
/// Errors and failed operations only.
pub const ERR: Color = Color::Red;

pub fn dim() -> Style {
    Style::default().fg(MUTED)
}

pub fn accent() -> Style {
    Style::default().fg(ACCENT)
}

pub fn accent_bold() -> Style {
    Style::default().fg(ACCENT).add_modifier(Modifier::BOLD)
}

pub fn ok() -> Style {
    Style::default().fg(OK)
}

pub fn err() -> Style {
    Style::default().fg(ERR)
}

/// Style for the selected list row.
pub fn selection() -> Style {
    Style::default()
        .fg(ACCENT)
        .add_modifier(Modifier::REVERSED)
}

/// Rounded panel with the border in accent when focused, dim otherwise.
/// `title` should come from [`panel_title`] (or a custom `Line`).
pub fn panel(title: Line<'static>, focused: bool) -> Block<'static> {
    let border = if focused { accent() } else { dim() };
    Block::default()
        .borders(Borders::ALL)
        .border_type(BorderType::Rounded)
        .border_style(border)
        .title(title)
}

/// `┈ text ┈` title line embedded in a panel border (dim separators).
pub fn panel_title(text: impl Into<String>) -> Line<'static> {
    Line::from(vec![
        Span::styled("┈ ", dim()),
        Span::raw(text.into()),
        Span::styled(" ┈", dim()),
    ])
}

/// Extra spans appended to a panel title, e.g. a spinner or status marker.
pub fn panel_title_with(text: impl Into<String>, extra: Vec<Span<'static>>) -> Line<'static> {
    let mut spans = vec![Span::styled("┈ ", dim()), Span::raw(text.into())];
    spans.extend(extra);
    spans.push(Span::styled(" ┈", dim()));
    Line::from(spans)
}

/// `┈ i install ┈ r remove ┈` hint line for a panel's bottom border:
/// keys in accent, descriptions dim.
pub fn hint_line(hints: &[(&str, &str)]) -> Line<'static> {
    let mut spans = Vec::with_capacity(hints.len() * 4 + 1);
    spans.push(Span::styled("┈ ", dim()));
    for (i, (key, desc)) in hints.iter().enumerate() {
        if i > 0 {
            spans.push(Span::styled(" ┈ ", dim()));
        }
        spans.push(Span::styled((*key).to_string(), accent()));
        spans.push(Span::styled(format!(" {desc}"), dim()));
    }
    spans.push(Span::styled(" ┈", dim()));
    Line::from(spans)
}

/// Footer hint spans (no border decorations): `key desc · key desc`.
pub fn footer_hints(hints: &[(&str, &str)]) -> Vec<Span<'static>> {
    let mut spans = Vec::with_capacity(hints.len() * 3);
    for (i, (key, desc)) in hints.iter().enumerate() {
        if i > 0 {
            spans.push(Span::styled(" · ", dim()));
        }
        spans.push(Span::styled((*key).to_string(), accent()));
        spans.push(Span::styled(format!(" {desc}"), dim()));
    }
    spans
}
