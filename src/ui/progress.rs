use super::palette::{BRAND, BRAND_DEEP, TRACK};
use indicatif::{ProgressBar, ProgressStyle};

/// Spinner and bar colors in indicatif's dotted-style syntax: xterm-256 indices
/// from the TSI palette (see `palette.rs`).
fn spinner_style() -> String {
    format!("{{spinner:.{BRAND}}}")
}

fn bar(width: u16) -> String {
    format!("{{bar:{width}.{BRAND_DEEP}/{TRACK}}}")
}

pub fn create_download_progress(total: u64) -> ProgressBar {
    let pb = ProgressBar::new(total);
    pb.set_style(
        ProgressStyle::default_bar()
            .template(&format!(
                "{} [{}] {{bytes}}/{{total_bytes}} ({{eta}})",
                spinner_style(),
                bar(40)
            ))
            .expect("valid progress template")
            .progress_chars("##-"),
    );
    pb
}

pub fn create_spinner(message: &str) -> ProgressBar {
    let pb = ProgressBar::new_spinner();
    pb.set_message(message.to_string());
    pb.set_style(
        ProgressStyle::default_spinner()
            .template(&format!("{} {{msg}}", spinner_style()))
            .expect("valid progress template"),
    );
    pb
}

pub fn create_simple_progress_bar(total: u64) -> ProgressBar {
    let pb = ProgressBar::new(total);
    pb.set_style(
        ProgressStyle::default_bar()
            .template(&format!(
                "{} [{}] {{pos}}/{{len}} ({{per_sec}})",
                spinner_style(),
                bar(60)
            ))
            .expect("valid progress template")
            .progress_chars("##-"),
    );
    pb
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn templates_carry_palette_colors() {
        assert_eq!(spinner_style(), "{spinner:.167}");
        assert_eq!(bar(40), "{bar:40.131/238}");
    }
}
