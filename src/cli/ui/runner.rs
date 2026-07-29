//! Subprocess operation runner: spawns `tsi <args>` as a child of the current
//! binary, streams its stdout/stderr into an in-TUI log pane, and tracks the
//! exit status. Only one operation runs at a time (enforced by `App`).

use super::theme;
use anyhow::{Context, Result};
use ratatui::layout::Rect;
use ratatui::text::{Line, Span};
use ratatui::widgets::{Paragraph, Wrap};
use ratatui::Frame;
use std::io::{BufRead, BufReader, Read};
use std::process::{Child, Command, Stdio};
use std::sync::mpsc::{Receiver, Sender};
use std::thread;

/// Keep at most this many log lines in memory (oldest are dropped).
const MAX_LOG_LINES: usize = 5000;

const SPINNER: &[&str] = &["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"];

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OpStatus {
    Running,
    Done,
    /// Exit code, or -1 when terminated by a signal.
    Failed(i32),
}

/// A running (or finished) `tsi` subprocess with its captured log.
pub struct OpRunner {
    /// Human-readable label, e.g. `install curl`.
    pub label: String,
    child: Child,
    rx: Receiver<String>,
    pub lines: Vec<String>,
    pub status: OpStatus,
    /// `None` = auto-follow the tail; `Some(offset)` = manual scroll position.
    scroll: Option<usize>,
    /// Log pane inner height, recorded during render for page scrolling.
    viewport: usize,
}

impl OpRunner {
    /// Spawns `current_exe() <args>` with stdout+stderr piped; two reader
    /// threads forward lines over a channel drained by [`OpRunner::poll`].
    pub fn spawn(label: String, args: Vec<String>) -> Result<Self> {
        let exe = std::env::current_exe().context("cannot locate the tsi binary")?;
        let mut child = Command::new(exe)
            .args(&args)
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped())
            .spawn()
            .context("failed to spawn tsi subprocess")?;

        let (tx, rx) = std::sync::mpsc::channel();
        if let Some(out) = child.stdout.take() {
            spawn_reader(out, tx.clone());
        }
        if let Some(err) = child.stderr.take() {
            spawn_reader(err, tx);
        }

        Ok(Self {
            label,
            child,
            rx,
            lines: Vec::new(),
            status: OpStatus::Running,
            scroll: None,
            viewport: 1,
        })
    }

    /// Drains pending output lines and updates the exit status.
    /// Call once per event-loop iteration.
    pub fn poll(&mut self) {
        while let Ok(line) = self.rx.try_recv() {
            self.lines.push(clean_line(&line));
        }
        if self.lines.len() > MAX_LOG_LINES {
            let excess = self.lines.len() - MAX_LOG_LINES;
            self.lines.drain(..excess);
            if let Some(s) = self.scroll.as_mut() {
                *s = s.saturating_sub(excess);
            }
        }
        if self.status == OpStatus::Running {
            if let Ok(Some(status)) = self.child.try_wait() {
                self.status = if status.success() {
                    OpStatus::Done
                } else {
                    OpStatus::Failed(status.code().unwrap_or(-1))
                };
            }
        }
    }

    pub fn running(&self) -> bool {
        self.status == OpStatus::Running
    }

    pub fn finished(&self) -> bool {
        !self.running()
    }

    /// Kills the child process and reaps it. Safe to call when already exited.
    pub fn kill(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }

    fn max_scroll(&self) -> usize {
        self.lines.len().saturating_sub(self.viewport)
    }

    pub fn scroll_page_up(&mut self) {
        let cur = self.scroll.unwrap_or_else(|| self.max_scroll());
        self.scroll = Some(cur.saturating_sub(self.viewport.max(1)));
    }

    pub fn scroll_page_down(&mut self) {
        let cur = self.scroll.unwrap_or_else(|| self.max_scroll());
        let next = cur + self.viewport.max(1);
        self.scroll = if next >= self.max_scroll() {
            None // back to auto-follow
        } else {
            Some(next)
        };
    }
}

impl Drop for OpRunner {
    fn drop(&mut self) {
        if self.running() {
            self.kill();
        }
    }
}

fn spawn_reader(stream: impl Read + Send + 'static, tx: Sender<String>) {
    thread::spawn(move || {
        for line in BufReader::new(stream).lines().map_while(|l| l.ok()) {
            if tx.send(line).is_err() {
                break;
            }
        }
    });
}

/// Strips ANSI escapes and collapses `\r` progress redraws to the final frame.
fn clean_line(line: &str) -> String {
    let stripped = strip_ansi(line);
    match stripped.rfind('\r') {
        Some(pos) => stripped[pos + 1..].to_string(),
        None => stripped,
    }
}

/// Removes ANSI escape sequences (CSI `ESC[...X` and OSC `ESC]...BEL/ST`)
/// with a tiny state machine — no dependencies.
pub fn strip_ansi(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    let mut chars = s.chars().peekable();
    while let Some(c) = chars.next() {
        if c != '\u{1b}' {
            out.push(c);
            continue;
        }
        match chars.peek() {
            // CSI: ESC [ <params/intermediates> <final byte @..~>
            Some('[') => {
                chars.next();
                for n in chars.by_ref() {
                    if ('\u{40}'..='\u{7e}').contains(&n) {
                        break;
                    }
                }
            }
            // OSC: ESC ] ... terminated by BEL or ST (ESC \)
            Some(']') => {
                chars.next();
                while let Some(n) = chars.next() {
                    if n == '\u{07}' {
                        break;
                    }
                    if n == '\u{1b}' {
                        if chars.peek() == Some(&'\\') {
                            chars.next();
                        }
                        break;
                    }
                }
            }
            // Two-character escapes (ESC c, ESC 7, ...)
            Some(_) => {
                chars.next();
            }
            None => {}
        }
    }
    out
}

/// Renders the log pane for `op` into `area` (records the viewport height
/// for page scrolling as a side effect).
pub fn render_log(f: &mut Frame, op: &mut OpRunner, tick: usize, area: Rect) {
    let status_spans: Vec<Span<'static>> = match op.status {
        OpStatus::Running => vec![
            Span::styled(" · ", theme::dim()),
            Span::styled(SPINNER[tick % SPINNER.len()].to_string(), theme::accent()),
        ],
        OpStatus::Done => vec![
            Span::styled(" · ", theme::dim()),
            Span::styled("✔ done", theme::ok()),
        ],
        OpStatus::Failed(code) => vec![
            Span::styled(" · ", theme::dim()),
            Span::styled(format!("✖ failed (exit {code})"), theme::err()),
        ],
    };
    let title = theme::panel_title_with(format!("log · {}", op.label), status_spans);
    let mut block = theme::panel(title, op.running());
    if op.finished() {
        block = block.title_bottom(theme::hint_line(&[
            ("pgup/pgdn", "scroll"),
            ("esc", "close"),
        ]));
    }

    let inner_height = area.height.saturating_sub(2) as usize;
    op.viewport = inner_height.max(1);
    let offset = op.scroll.unwrap_or_else(|| op.max_scroll());

    let text: Vec<Line> = op
        .lines
        .iter()
        .skip(offset)
        .take(inner_height)
        .map(|l| Line::from(l.as_str()))
        .collect();
    let paragraph = Paragraph::new(text).block(block).wrap(Wrap { trim: false });
    f.render_widget(paragraph, area);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn strip_ansi_removes_color_codes() {
        assert_eq!(strip_ansi("\u{1b}[32mgreen\u{1b}[0m text"), "green text");
    }

    #[test]
    fn strip_ansi_removes_multi_param_csi() {
        assert_eq!(strip_ansi("\u{1b}[1;38;5;208mbold\u{1b}[0m"), "bold");
    }

    #[test]
    fn strip_ansi_removes_osc_sequences() {
        assert_eq!(strip_ansi("\u{1b}]0;title\u{07}rest"), "rest");
        assert_eq!(strip_ansi("\u{1b}]8;;url\u{1b}\\link"), "link");
    }

    #[test]
    fn strip_ansi_passes_plain_text_through() {
        assert_eq!(strip_ansi("plain → text ● dots"), "plain → text ● dots");
    }

    #[test]
    fn strip_ansi_handles_truncated_escape() {
        assert_eq!(strip_ansi("text\u{1b}"), "text");
        assert_eq!(strip_ansi("text\u{1b}[31"), "text");
    }

    #[test]
    fn clean_line_keeps_last_carriage_return_frame() {
        assert_eq!(clean_line("10%\r50%\r100% done"), "100% done");
        assert_eq!(clean_line("no progress"), "no progress");
    }
}
