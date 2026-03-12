use crate::platform;
use crate::ui;
use anyhow::Result;
use clap::Args;
use std::path::PathBuf;

const DEFAULT_REPO: &str = "https://github.com/PanterSoft/tsi.git";

#[derive(Args)]
pub struct UpdateArgs {
    #[arg(long)]
    pub repo: Option<String>,
    #[arg(long)]
    pub local: Option<String>,
    #[arg(long)]
    pub prefix: Option<String>,
}

pub fn run(args: UpdateArgs) -> Result<()> {
    let prefix = platform::resolve_prefix(args.prefix.as_deref());
    let packages_dir = prefix.join("packages");
    std::fs::create_dir_all(&packages_dir)?;

    if let Some(local) = &args.local {
        ui::output::section("Copying packages from local path...");
        let src = PathBuf::from(local);
        if !src.is_dir() {
            return Err(anyhow::anyhow!("Local path is not a directory: {}", local));
        }
        for entry in std::fs::read_dir(&src)? {
            let entry = entry?;
            let path = entry.path();
            if path.extension().is_some_and(|e| e == "json") {
                let dest = packages_dir.join(entry.file_name());
                std::fs::copy(&path, &dest)?;
            }
        }
        ui::output::detail("Package definitions updated");
        return Ok(());
    }

    let repo = args.repo.as_deref().unwrap_or(DEFAULT_REPO);
    ui::output::section("Syncing repository...");
    let tmp = prefix.join("tmp-repo-update");
    if tmp.exists() {
        let status = std::process::Command::new("git")
            .args(["pull"])
            .current_dir(&tmp)
            .status()?;
        if !status.success() {
            std::fs::remove_dir_all(&tmp)?;
        }
    }
    if !tmp.exists() {
        let status = std::process::Command::new("git")
            .args(["clone", "--depth", "1", repo, tmp.to_str().unwrap()])
            .status()?;
        if !status.success() {
            return Err(anyhow::anyhow!("git clone failed"));
        }
    }

    let src_packages = tmp.join("packages");
    if src_packages.exists() {
        for entry in std::fs::read_dir(&src_packages)? {
            let entry = entry?;
            let path = entry.path();
            if path.extension().is_some_and(|e| e == "json") {
                let dest = packages_dir.join(entry.file_name());
                std::fs::copy(&path, &dest)?;
            }
        }
    }
    ui::output::detail("Package definitions updated");
    Ok(())
}
