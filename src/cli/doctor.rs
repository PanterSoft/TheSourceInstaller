use crate::core::registry::Registry;
use crate::platform;
use crate::ui;
use anyhow::Result;
use clap::Args;
#[derive(Args)]
pub struct DoctorArgs {
    #[arg(long)]
    pub prefix: Option<String>,
}

pub fn run(args: DoctorArgs) -> Result<()> {
    let prefix = platform::resolve_prefix(args.prefix.as_deref());
    let packages_dir = prefix.join("packages");

    ui::output::section("Checking system...");

    let mut warnings = 0;

    let cc = if cfg!(windows) { "cl" } else { "cc" };
    if std::process::Command::new(cc)
        .arg("--version")
        .output()
        .is_ok()
    {
        ui::output::success("C compiler found");
    } else {
        ui::output::warning("C compiler not found -- required for building");
        warnings += 1;
    }

    if std::process::Command::new("make")
        .arg("--version")
        .output()
        .is_ok()
    {
        ui::output::success("make found");
    } else {
        ui::output::warning("make not found -- required for most packages");
        warnings += 1;
    }

    if packages_dir.exists() {
        let registry = Registry::load_from_dir(&packages_dir).unwrap_or_else(|_| Registry::new());
        ui::output::success(format!(
            "Package definitions: {} packages available",
            registry.count()
        ));
    } else {
        ui::output::warning("No package definitions -- run 'tsi update'");
        warnings += 1;
    }

    if prefix.exists() {
        if let Ok(meta) = std::fs::metadata(&prefix) {
            if meta.is_dir() {
                ui::output::success(format!("Install prefix: {} (writable)", prefix.display()));
            }
        }
    } else {
        if std::fs::create_dir_all(&prefix).is_ok() {
            ui::output::success(format!("Install prefix: {} (created)", prefix.display()));
        } else {
            ui::output::warning(format!("Cannot create prefix: {}", prefix.display()));
            warnings += 1;
        }
    }

    if std::process::Command::new("git")
        .arg("--version")
        .output()
        .is_ok()
    {
        ui::output::success("git found");
    } else {
        ui::output::warning("git not found -- some packages require git sources");
        warnings += 1;
    }

    if warnings > 0 {
        ui::output::section(format!("{} warning(s) found.", warnings));
    } else {
        ui::output::section("All checks passed.");
    }

    Ok(())
}
