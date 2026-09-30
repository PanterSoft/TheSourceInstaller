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

/// What TSI creates directly under its prefix. `tsi remove` deletes these and
/// nothing else, so a prefix that holds other files keeps them.
const TSI_DIRS: &[&str] = &[
    "packages",
    "sources",
    "build",
    "db",
    "install",
    "tmp-repo-update",
    "tmp-self-update",
];
const TSI_FILES: &[&str] = &[
    "bin/tsi",
    "bin/tsi.exe",
    "share/completions/tsi.bash",
    "share/completions/tsi.zsh",
    "tsi.toml",
    ".tsi-install.lock",
    platform::PREFIX_MARKER,
];
/// Removed only when empty once the files above are gone.
const TSI_PARENTS: &[&str] = &["share/completions", "share", "bin"];

fn is_tsi_install(prefix: &Path) -> bool {
    let bin_tsi = prefix.join("bin").join("tsi");
    let bin_tsi_exe = prefix.join("bin").join("tsi.exe");
    prefix.join(platform::PREFIX_MARKER).is_file() || bin_tsi.is_file() || bin_tsi_exe.is_file()
}

fn confirm_remove(prefix: &Path) -> Result<bool> {
    let prompt = format!(
        "Do you really want to uninstall TSI? This will remove TSI and all installed packages from {}. [y/N]: ",
        prefix.display()
    );
    let _ = io::stderr().lock().write_all(prompt.as_bytes());
    let _ = io::stderr().lock().flush();
    let mut line = String::new();
    io::stdin()
        .read_line(&mut line)
        .context("Read confirmation")?;
    let trimmed = line.trim().to_lowercase();
    Ok(trimmed == "y" || trimmed == "yes")
}

/// Deletes what TSI created under `prefix`, then `prefix` itself if that leaves it
/// empty. Never deletes anything else.
fn remove_tsi_files(prefix: &Path) -> Result<()> {
    for dir in TSI_DIRS {
        let path = prefix.join(dir);
        if std::fs::symlink_metadata(&path).is_ok() {
            std::fs::remove_dir_all(&path).with_context(|| format!("Remove {}", path.display()))?;
        }
    }
    for file in TSI_FILES {
        let path = prefix.join(file);
        if std::fs::symlink_metadata(&path).is_ok() {
            std::fs::remove_file(&path).with_context(|| format!("Remove {}", path.display()))?;
        }
    }
    for dir in TSI_PARENTS {
        let _ = std::fs::remove_dir(prefix.join(dir)); // only succeeds when empty
    }
    let _ = std::fs::remove_dir(prefix);
    Ok(())
}

pub fn run(args: RemoveArgs) -> Result<()> {
    let prefix = platform::resolve_prefix(args.prefix.as_deref());
    if !prefix.exists() {
        ui::output::error(format!("No TSI installation found at {}", prefix.display()));
        return Err(anyhow::anyhow!(
            "Prefix does not exist: {}",
            prefix.display()
        ));
    }
    if platform::is_system_dir(&prefix) {
        ui::output::error(format!(
            "{} is a system directory, not a TSI prefix. Refusing to remove.",
            prefix.display()
        ));
        return Err(anyhow::anyhow!(
            "Refusing to remove system directory: {}",
            prefix.display()
        ));
    }
    if !is_tsi_install(&prefix) {
        ui::output::error(format!(
            "{} does not look like a TSI installation (no bin/tsi). Refusing to remove.",
            prefix.display()
        ));
        return Err(anyhow::anyhow!(
            "Not a TSI installation: {}",
            prefix.display()
        ));
    }
    if !args.yes && !confirm_remove(&prefix)? {
        ui::output::info("Cancelled.");
        return Ok(());
    }
    ui::output::step(format!("Removing TSI from {}...", prefix.display()));
    remove_tsi_files(&prefix)?;
    ui::output::success("TSI uninstalled.");
    if prefix.exists() {
        ui::output::info(format!(
            "Kept {}: it holds files TSI did not create.",
            prefix.display()
        ));
    }
    ui::output::info(
        "Remove the TSI bin directory from your PATH in your shell profile if present.",
    );
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn removes_only_what_tsi_created() {
        let tmp = tempfile::tempdir().unwrap();
        let prefix = tmp.path().join("prefix");
        for dir in ["bin", "packages", "db", "install/bin", "share/completions"] {
            std::fs::create_dir_all(prefix.join(dir)).unwrap();
        }
        std::fs::write(prefix.join("bin/tsi"), "").unwrap();
        std::fs::write(prefix.join("bin/other-tool"), "").unwrap();
        std::fs::write(prefix.join("share/completions/tsi.bash"), "").unwrap();
        std::fs::write(prefix.join("notes.txt"), "").unwrap();

        remove_tsi_files(&prefix).unwrap();

        assert!(prefix.join("bin/other-tool").exists());
        assert!(prefix.join("notes.txt").exists());
        assert!(!prefix.join("bin/tsi").exists());
        assert!(!prefix.join("packages").exists());
        assert!(!prefix.join("install").exists());
        assert!(!prefix.join("share").exists());
    }

    #[test]
    fn removes_a_prefix_left_empty() {
        let tmp = tempfile::tempdir().unwrap();
        let prefix = tmp.path().join("prefix");
        std::fs::create_dir_all(prefix.join("bin")).unwrap();
        std::fs::write(prefix.join("bin/tsi"), "").unwrap();
        platform::mark_prefix(&prefix);
        remove_tsi_files(&prefix).unwrap();
        assert!(!prefix.exists());
    }
}
