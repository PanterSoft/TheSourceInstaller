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

/// White (`#FFFFFF`) for text set on a Terminal Red background, e.g. the TUI's
/// ` tsi ` badge.
pub const ON_BRAND: u8 = 231;

/// Muted Terminal Red (`#AF5F5F`): the filled part of progress bars.
pub const BRAND_DEEP: u8 = 131;

/// Unfilled part of progress bars (`#444444`).
pub const TRACK: u8 = 238;

#[cfg(test)]
mod tests {
    use super::*;

    /// `docs/assets/brand/tokens.json` is the palette a GUI or any other tool
    /// reads; it must not drift from what the terminal actually uses.
    #[test]
    fn matches_the_published_design_tokens() {
        let path = concat!(env!("CARGO_MANIFEST_DIR"), "/docs/assets/brand/tokens.json");
        let tokens: serde_json::Value =
            serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
        let term = |name: &str| tokens["terminal"][name]["$value"].as_u64();

        assert_eq!(term("brand"), Some(BRAND.into()));
        assert_eq!(term("on-brand"), Some(ON_BRAND.into()));
        assert_eq!(term("brand-deep"), Some(BRAND_DEEP.into()));
        assert_eq!(term("track"), Some(TRACK.into()));
    }
}
