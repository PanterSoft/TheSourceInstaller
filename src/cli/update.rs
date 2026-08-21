use crate::platform;
use crate::ui;
use anyhow::{Context, Result};
use clap::Args;
use std::path::{Path, PathBuf};

const DEFAULT_REPO: &str = "https://github.com/PanterSoft/tsi-packages.git";

fn copy_package_jsons(from_dir: &Path, packages_dir: &Path) -> Result<()> {
    // std::fs::copy truncates the destination before reading the source, so
    // copying a directory onto itself empties every package file in it. Refuse
    // instead: `tsi update --local ~/.tsi/packages` used to wipe the registry.
    if same_dir(from_dir, packages_dir) {
        anyhow::bail!(
            "Source and destination are the same directory ({}); \
             nothing to copy, and copying would empty the package files.",
            packages_dir.display()
        );
    }
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

/// Whether two paths name the same directory, following symlinks and `..`.
/// Falls back to a plain comparison when either path cannot be canonicalized.
fn same_dir(a: &Path, b: &Path) -> bool {
    match (a.canonicalize(), b.canonicalize()) {
        (Ok(a), Ok(b)) => a == b,
        _ => a == b,
    }
}

/// Returns true if a `git` binary is available on this system.
pub(crate) fn is_git_available() -> bool {
    std::process::Command::new("git")
        .arg("--version")
        .output()
        .is_ok()
}

/// Derives a GitHub archive-tarball URL (main branch) from a GitHub repository URL of the
/// form `https://github.com/OWNER/REPO` or `https://github.com/OWNER/REPO.git`.
/// Returns `None` if `repo` doesn't look like a plain GitHub repository URL.
fn github_tarball_url(repo: &str) -> Option<String> {
    let rest = repo
        .strip_prefix("https://github.com/")
        .or_else(|| repo.strip_prefix("http://github.com/"))?;
    let rest = rest.trim_end_matches('/');
    let rest = rest.strip_suffix(".git").unwrap_or(rest);

    let (owner, name) = rest.split_once('/')?;
    if owner.is_empty() || name.is_empty() || name.contains('/') {
        return None;
    }
    Some(format!(
        "https://github.com/{owner}/{name}/archive/refs/heads/main.tar.gz"
    ))
}

pub(crate) fn extract_tar_gz(archive: &Path, dest: &Path) -> Result<()> {
    let file = std::fs::File::open(archive).context("Open downloaded archive")?;
    let dec = flate2::read::GzDecoder::new(std::io::BufReader::new(file));
    tar::Archive::new(dec)
        .unpack(dest)
        .context("Extract repository tarball")?;
    Ok(())
}

/// No-git fallback for `tsi update`: downloads the repository as a GitHub tarball via the
/// built-in HTTP client, extracts it with the built-in tar support, and returns the path to
/// its `packages/` directory.
fn fetch_repo_via_tarball(repo: &str, tmp: &Path) -> Result<PathBuf> {
    let url = github_tarball_url(repo).ok_or_else(|| {
        anyhow::anyhow!(
            "git is not installed, and '{repo}' is not a GitHub repository URL, so package \
             definitions can't be downloaded automatically. Install git, pass --repo with a \
             GitHub URL (https://github.com/OWNER/REPO), or use --local instead."
        )
    })?;

    if tmp.exists() {
        std::fs::remove_dir_all(tmp).context("Remove stale tmp dir")?;
    }
    std::fs::create_dir_all(tmp).context("Create tmp dir")?;

    let archive_path = tmp.join("repo.tar.gz");
    crate::ops::fetch::download_file(&url, &archive_path)
        .with_context(|| format!("Download repository tarball from {url}"))?;

    extract_tar_gz(&archive_path, tmp)?;
    let _ = std::fs::remove_file(&archive_path);

    // GitHub archive tarballs extract into a single "REPO-main" directory.
    let extracted = std::fs::read_dir(tmp)
        .context("Read extracted tmp dir")?
        .filter_map(|e| e.ok())
        .find(|e| e.path().is_dir() && e.file_name().to_string_lossy().ends_with("-main"))
        .map(|e| e.path())
        .ok_or_else(|| {
            anyhow::anyhow!(
                "Could not find extracted repository directory under {}",
                tmp.display()
            )
        })?;

    Ok(extracted.join("packages"))
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

    let src_packages = if is_git_available() {
        if tmp.exists() {
            let remote_matches = std::process::Command::new("git")
                .args(["-C"])
                .arg(&tmp)
                .args(["remote", "get-url", "origin"])
                .output()
                .is_ok_and(|out| {
                    out.status.success() && String::from_utf8_lossy(&out.stdout).trim() == repo
                });
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
        tmp.join("packages")
    } else {
        ui::output::detail("git not found; downloading package definitions via HTTP instead");
        fetch_repo_via_tarball(repo, &tmp)?
    };

    if src_packages.exists() {
        copy_package_jsons(&src_packages, &packages_dir)?;
    }
    ui::output::detail("Package definitions updated");
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::{copy_package_jsons, github_tarball_url};

    #[test]
    fn github_tarball_url_derivation() {
        assert_eq!(
            github_tarball_url("https://github.com/PanterSoft/tsi-packages.git"),
            Some(
                "https://github.com/PanterSoft/tsi-packages/archive/refs/heads/main.tar.gz"
                    .to_string()
            )
        );
        // No .git suffix.
        assert_eq!(
            github_tarball_url("https://github.com/PanterSoft/tsi-packages"),
            Some(
                "https://github.com/PanterSoft/tsi-packages/archive/refs/heads/main.tar.gz"
                    .to_string()
            )
        );
        // Trailing slash.
        assert_eq!(
            github_tarball_url("https://github.com/PanterSoft/tsi-packages/"),
            Some(
                "https://github.com/PanterSoft/tsi-packages/archive/refs/heads/main.tar.gz"
                    .to_string()
            )
        );
        // http (non-https) still recognized.
        assert_eq!(
            github_tarball_url("http://github.com/owner/repo"),
            Some("https://github.com/owner/repo/archive/refs/heads/main.tar.gz".to_string())
        );
        // Non-GitHub URL is rejected.
        assert_eq!(
            github_tarball_url("https://gitlab.com/owner/repo.git"),
            None
        );
        // Missing repo segment is rejected.
        assert_eq!(github_tarball_url("https://github.com/owner"), None);
        // Extra path segments beyond owner/repo are rejected.
        assert_eq!(
            github_tarball_url("https://github.com/owner/repo/tree/main"),
            None
        );
    }
    #[test]
    fn copying_a_directory_onto_itself_is_refused_and_leaves_files_intact() {
        let dir = std::env::temp_dir().join(format!("tsi-selfcopy-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(&dir).unwrap();
        let pkg = dir.join("zlib.json");
        std::fs::write(&pkg, r#"{"name":"zlib"}"#).unwrap();

        let err = copy_package_jsons(&dir, &dir).unwrap_err();
        assert!(
            err.to_string().contains("same directory"),
            "unexpected error: {err}"
        );
        // The whole point: fs::copy would have truncated this to zero bytes.
        assert_eq!(std::fs::read_to_string(&pkg).unwrap(), r#"{"name":"zlib"}"#);

        // A genuinely different directory still copies.
        let other = dir.join("out");
        std::fs::create_dir_all(&other).unwrap();
        copy_package_jsons(&dir, &other).unwrap();
        assert_eq!(
            std::fs::read_to_string(other.join("zlib.json")).unwrap(),
            r#"{"name":"zlib"}"#
        );

        let _ = std::fs::remove_dir_all(&dir);
    }
}
