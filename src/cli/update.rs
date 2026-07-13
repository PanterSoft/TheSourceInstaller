use crate::platform;
use crate::ui;
use anyhow::{Context, Result};
use clap::Args;
use std::path::{Path, PathBuf};

const DEFAULT_REPO: &str = "https://github.com/PanterSoft/tsi-packages.git";

fn copy_package_jsons(from_dir: &Path, packages_dir: &Path) -> Result<()> {
    for entry in std::fs::read_dir(from_dir).context("Read package source dir")? {
        let entry = entry?;
        let path = entry.path();
        if path.extension().is_some_and(|e| e == "json") {
            let dest = packages_dir.join(entry.file_name());
            std::fs::copy(&path, &dest).context("Copy package file")?;
        }
    }
    Ok(())
}

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
        copy_package_jsons(&src, &packages_dir)?;
        ui::output::detail("Package definitions updated");
        return Ok(());
    }

    let repo = args.repo.as_deref().unwrap_or(DEFAULT_REPO);
    ui::output::section("Syncing repository...");
    let tmp = prefix.join("tmp-repo-update");
    if tmp.exists() {
        let remote_matches = std::process::Command::new("git")
            .args(["-C"])
            .arg(&tmp)
            .args(["remote", "get-url", "origin"])
            .output()
            .is_ok_and(|out| out.status.success() && String::from_utf8_lossy(&out.stdout).trim() == repo);
        if !remote_matches {
            std::fs::remove_dir_all(&tmp)?;
        }
    }
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
            .args(["clone", "--depth", "1", repo])
            .arg(&tmp)
            .status()?;
        if !status.success() {
            return Err(anyhow::anyhow!("git clone failed"));
        }
    }

    let src_packages = tmp.join("packages");
    if src_packages.exists() {
        copy_package_jsons(&src_packages, &packages_dir)?;
    }
    ui::output::detail("Package definitions updated");
    Ok(())
}
