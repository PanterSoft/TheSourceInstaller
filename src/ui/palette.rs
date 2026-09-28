//! TSI's terminal color identity, shared by the CLI output and the TUI.
//!
//! Colors are xterm-256 indices rather than 24-bit RGB: they render the same on
//! every terminal TSI targets, including ones without truecolor. See
//! `docs/brand.md` for the full palette and the rules behind these choices.

/// Terminal Red (`#D75F5F`): the brand color in the terminal. Section arrows,
/// the TUI accent (focused borders, selection, active tab, keys), spinners.
/// Chosen over the web's Forge Red because it stays readable on both dark
/// (5.7:1 on black) and light backgrounds.
pub const BRAND: u8 = 167;

/// Muted Terminal Red (`#AF5F5F`): the filled part of progress bars.
pub const BRAND_DEEP: u8 = 131;

/// Unfilled part of progress bars (`#444444`).
pub const TRACK: u8 = 238;
