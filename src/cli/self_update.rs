use crate::cli::update::{extract_tar_gz, is_git_available};
use crate::platform;
use crate::ui;
use anyhow::{Context, Result};
use clap::Args;
use std::path::{Path, PathBuf};

use crate::repos::TSI_REPO as DEFAULT_REPO;

#[derive(Args)]
pub struct SelfUpdateArgs {
    /// Repository to update from (default: TSI's GitHub repository)
    #[arg(long)]
    pub repo: Option<String>,
    /// Update even when the running version is already the latest release
    #[arg(long)]
    pub force: bool,
    #[arg(long, default_value = "main")]
    pub branch: String,
    #[arg(long)]
    pub prefix: Option<String>,
}

/// Replaces the running `tsi` binary with `new_bin`.
///
/// On Unix, `rename()` swaps the directory entry atomically; a process that's currently
/// executing the old file keeps its inode open, so overwriting it in place is safe. Windows
/// refuses to overwrite an executable that's running, so there we move the old one aside first.
fn replace_binary(new_bin: &Path, target: &Path) -> Result<()> {
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        std::fs::set_permissions(new_bin, std::fs::Permissions::from_mode(0o755))
            .context("Make updated binary executable")?;
        std::fs::rename(new_bin, target).context("Install updated binary")?;
    }
    #[cfg(windows)]
    {
        let old = target.with_extension("exe.old");
        let _ = std::fs::remove_file(&old);
        std::fs::rename(target, &old).context("Move aside running binary")?;
        std::fs::rename(new_bin, target).context("Install updated binary")?;
    }
    Ok(())
}

/// The version of `slug`'s latest GitHub release (its tag without a leading `v`), or
/// `None` when it can't be found out (no release, offline, rate-limited).
fn latest_release_version(slug: &str) -> Option<String> {
    #[derive(serde::Deserialize)]
    struct Release {
        tag_name: String,
    }
    let url = format!("https://api.github.com/repos/{slug}/releases/latest");
    let agent = ureq::AgentBuilder::new()
        .timeout(std::time::Duration::from_secs(15))
        .build();
    let body = agent
        .get(&url)
        .set("User-Agent", &format!("tsi/{}", env!("CARGO_PKG_VERSION")))
        .set("Accept", "application/vnd.github+json")
        .call()
        .ok()?
        .into_string()
        .ok()?;
    let release: Release = serde_json::from_str(&body).ok()?;
    Some(release.tag_name.trim_start_matches('v').to_string())
}

/// True when version `current` is the same as or newer than `latest`, comparing the
/// numeric dot-separated parts (`0.2.10` is newer than `0.2.9`). Pre-release or build
/// suffixes are ignored; a version that doesn't parse is never up to date.
fn is_up_to_date(current: &str, latest: &str) -> bool {
    fn parse(v: &str) -> Option<Vec<u64>> {
        let core = v.trim_start_matches('v').split(['-', '+']).next()?;
        core.split('.').map(|p| p.parse().ok()).collect()
    }
    match (parse(current), parse(latest)) {
        (Some(mut c), Some(mut l)) => {
            let n = c.len().max(l.len());
            c.resize(n, 0);
            l.resize(n, 0);
            c >= l
        }
        _ => false,
    }
}

/// Tries to download a pre-built binary for this platform from `slug`'s latest GitHub
/// release. Returns `None` (not an error) if no matching release asset exists.
fn try_prebuilt(slug: &str, tmp: &Path) -> Option<PathBuf> {
    let plat = platform::release_platform();
    let url = format!("https://github.com/{slug}/releases/latest/download/tsi-{plat}");
    let dest = tmp.join("tsi-new");
    match crate::ops::fetch::download_file(&url, &dest) {
        Ok(()) if dest.metadata().is_ok_and(|m| m.len() > 0) => Some(dest),
        _ => {
            let _ = std::fs::remove_file(&dest);
            None
        }
    }
}

/// Fetches source for `repo`@`branch` into `tmp/src` (via git if available, else a GitHub
/// tarball) and builds it with cargo. Returns the path to the resulting release binary.
fn build_from_source(repo: &str, branch: &str, tmp: &Path) -> Result<PathBuf> {
    let src = tmp.join("src");

    if is_git_available() {
        let status = std::process::Command::new("git")
            .args(["clone", "--depth", "1", "--branch", branch, repo])
            .arg(&src)
            .status()
            .context("Run git clone")?;
        if !status.success() {
            anyhow::bail!("git clone of {repo} ({branch}) failed");
        }
    } else {
        let rest = crate::repos::github_slug(repo).ok_or_else(|| {
            anyhow::anyhow!(
                "git is not installed, and '{repo}' is not a GitHub repository URL, so \
                     source can't be downloaded automatically. Install git or pass --repo."
            )
        })?;
        let url = format!("https://github.com/{rest}/archive/refs/heads/{branch}.tar.gz");
        let archive = tmp.join("src.tar.gz");
        crate::ops::fetch::download_file(&url, &archive)
            .with_context(|| format!("Download source tarball from {url}"))?;
        extract_tar_gz(&archive, tmp)?;
        let _ = std::fs::remove_file(&archive);
        let extracted = std::fs::read_dir(tmp)
            .context("Read extracted tmp dir")?
            .filter_map(|e| e.ok())
            .find(|e| e.path().is_dir() && e.file_name() != "src")
            .map(|e| e.path())
            .ok_or_else(|| anyhow::anyhow!("Could not find extracted source directory"))?;
        std::fs::rename(&extracted, &src).context("Move extracted source into place")?;
    }

    if std::process::Command::new("cargo")
        .arg("--version")
        .output()
        .is_err()
    {
        anyhow::bail!(
            "No pre-built binary available for this platform, and Rust/cargo isn't installed \
             to build from source. Install Rust from https://rustup.rs and try again."
        );
    }

    let status = std::process::Command::new("cargo")
        .args(["build", "--release"])
        .current_dir(&src)
        .status()
        .context("Run cargo build")?;
    if !status.success() {
        anyhow::bail!("cargo build --release failed");
    }

    let bin_name = if cfg!(windows) { "tsi.exe" } else { "tsi" };
    Ok(src.join("target").join("release").join(bin_name))
}

pub fn run(args: SelfUpdateArgs) -> Result<()> {
    let prefix = platform::resolve_prefix(args.prefix.as_deref());
    let exe = std::env::current_exe().context("Locate running tsi binary")?;
    let repo = args.repo.as_deref().unwrap_or(DEFAULT_REPO);
    let slug = crate::repos::github_slug(repo);

    let current = env!("CARGO_PKG_VERSION");
    if !args.force {
        if let Some(latest) = slug.as_deref().and_then(latest_release_version) {
            if is_up_to_date(current, &latest) {
                ui::output::success(format!(
                    "TSI is already up to date ({current}; latest release {latest})."
                ));
                return Ok(());
            }
            ui::output::detail(format!("Updating TSI {current} -> {latest}"));
        }
    }

    let tmp = prefix.join("tmp-self-update");
    if tmp.exists() {
        std::fs::remove_dir_all(&tmp).context("Remove stale tmp dir")?;
    }
    std::fs::create_dir_all(&tmp).context("Create tmp dir")?;

    ui::output::section("Checking for a pre-built binary...");
    let new_bin = match slug.as_deref().and_then(|s| try_prebuilt(s, &tmp)) {
        Some(p) => p,
        None => {
            ui::output::detail("No pre-built binary available; building from source");
            build_from_source(repo, &args.branch, &tmp)?
        }
    };

    ui::output::section("Installing updated binary...");
    replace_binary(&new_bin, &exe)?;
    let _ = std::fs::remove_dir_all(&tmp);
    ui::output::detail(format!("TSI updated: {}", exe.display()));
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::is_up_to_date;

    #[test]
    fn compares_versions_numerically() {
        assert!(is_up_to_date("0.2.2", "0.2.2"));
        assert!(is_up_to_date("0.2.2", "v0.2.2"));
        assert!(is_up_to_date("0.2.10", "0.2.9"));
        assert!(is_up_to_date("0.3", "0.2.9"));
        assert!(!is_up_to_date("0.2.2", "0.2.3"));
        assert!(!is_up_to_date("0.2.9", "0.2.10"));
        assert!(!is_up_to_date("0.2.2", "1.0.0-rc.1"));
        assert!(!is_up_to_date("0.2.2", "garbage"));
    }
}
