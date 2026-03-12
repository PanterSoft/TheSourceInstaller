use crate::platform;
use crate::ui;
use anyhow::{Context, Result};
use clap::Args;
use std::io::{self, Write};
use std::path::Path;

#[derive(Args)]
pub struct RemoveArgs {
    /// Installation prefix to remove (default: detected from binary location)
    #[arg(long)]
    pub prefix: Option<String>,
    /// Skip confirmation prompt
    #[arg(long)]
    pub yes: bool,
}

fn is_tsi_install(prefix: &Path) -> bool {
    let bin_tsi = prefix.join("bin").join("tsi");
    let bin_tsi_exe = prefix.join("bin").join("tsi.exe");
    (bin_tsi.exists() && bin_tsi.is_file()) || (bin_tsi_exe.exists() && bin_tsi_exe.is_file())
}

fn confirm_remove(prefix: &Path) -> Result<bool> {
    let prompt = format!(
        "Do you really want to uninstall TSI? This will remove {} and all installed packages. [y/N]: ",
        prefix.display()
    );
    let _ = io::stderr().lock().write_all(prompt.as_bytes());
    let _ = io::stderr().lock().flush();
    let mut line = String::new();
    io::stdin().read_line(&mut line).context("Read confirmation")?;
    let trimmed = line.trim().to_lowercase();
    Ok(trimmed == "y" || trimmed == "yes")
}

pub fn run(args: RemoveArgs) -> Result<()> {
    let prefix = platform::resolve_prefix(args.prefix.as_deref());
    if !prefix.exists() {
        ui::output::error(format!("No TSI installation found at {}", prefix.display()));
        return Err(anyhow::anyhow!("Prefix does not exist: {}", prefix.display()));
    }
    if !is_tsi_install(&prefix) {
        ui::output::error(format!(
            "{} does not look like a TSI installation (no bin/tsi). Refusing to remove.",
            prefix.display()
        ));
        return Err(anyhow::anyhow!("Not a TSI installation: {}", prefix.display()));
    }
    if !args.yes && !confirm_remove(&prefix)? {
        ui::output::info("Cancelled.");
        return Ok(());
    }
    ui::output::step(format!("Removing {}...", prefix.display()));
    std::fs::remove_dir_all(&prefix).context("Remove installation directory")?;
    ui::output::success("TSI uninstalled.");
    ui::output::info("Remove the TSI bin directory from your PATH in your shell profile if present.");
    Ok(())
}
