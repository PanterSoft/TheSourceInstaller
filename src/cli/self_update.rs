use crate::cli::update::{extract_tar_gz, is_git_available};
use crate::platform;
use crate::ui;
use anyhow::{Context, Result};
use clap::Args;
use std::path::{Path, PathBuf};

const DEFAULT_REPO: &str = "https://github.com/PanterSoft/tsi.git";

#[derive(Args)]
pub struct SelfUpdateArgs {
    #[arg(long)]
    pub repo: Option<String>,
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
    // `new_bin` lives in the prefix, which may be on another filesystem than `target`, and
    // rename() fails across devices. Stage a copy next to the target so the final rename is
    // always same-directory.
    let staged = target.with_extension("new");
    let _ = std::fs::remove_file(&staged);
    std::fs::copy(new_bin, &staged).context("Stage updated binary next to the installed one")?;
    let new_bin = staged.as_path();
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

/// Why a downloaded release asset failed verification against `SHA256SUMS`.
fn check_sums(sums: &str, asset: &str, actual_hex: &str) -> Result<()> {
    let expected = sums.lines().find_map(|line| {
        let (hash, name) = line.trim().split_once(char::is_whitespace)?;
        let name = name.trim().trim_start_matches('*');
        (name == asset).then(|| hash.to_ascii_lowercase())
    });
    match expected {
        None => anyhow::bail!("SHA256SUMS has no entry for {asset}; refusing to install"),
        Some(h) if h == actual_hex.to_ascii_lowercase() => Ok(()),
        Some(h) => anyhow::bail!(
            "checksum mismatch for {asset}: SHA256SUMS says {h}, downloaded file is {actual_hex}"
        ),
    }
}

/// Downloads `SHA256SUMS` from the latest release and verifies `bin` against it.
fn verify_download(bin: &Path, asset: &str, tmp: &Path) -> Result<()> {
    let sums_path = tmp.join("SHA256SUMS");
    let url = "https://github.com/PanterSoft/tsi/releases/latest/download/SHA256SUMS";
    crate::ops::fetch::download_file(url, &sums_path)
        .context("SHA256SUMS is missing from the release; refusing to install")?;
    let sums = std::fs::read_to_string(&sums_path).context("Read SHA256SUMS")?;
    let actual = crate::util::sha256::sha256_file(bin).context("Hash downloaded binary")?;
    check_sums(&sums, asset, &actual)
}

/// Tries to download a pre-built binary for this platform from the latest GitHub release.
/// Returns `Ok(None)` if no matching release asset exists. A binary that fails checksum
/// verification is deleted and reported as an error (no fallback to a source build).
fn try_prebuilt(tmp: &Path) -> Result<Option<PathBuf>> {
    let asset = format!("tsi-{}-{}", platform::os_name(), platform::arch_name());
    let url = format!("https://github.com/PanterSoft/tsi/releases/latest/download/{asset}");
    let dest = tmp.join("tsi-new");
    match crate::ops::fetch::download_file(&url, &dest) {
        Ok(()) if dest.metadata().is_ok_and(|m| m.len() > 0) => {
            if let Err(e) = verify_download(&dest, &asset, tmp) {
                let _ = std::fs::remove_file(&dest);
                return Err(e);
            }
            Ok(Some(dest))
        }
        _ => {
            let _ = std::fs::remove_file(&dest);
            Ok(None)
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
        let rest = repo
            .strip_prefix("https://github.com/")
            .or_else(|| repo.strip_prefix("http://github.com/"))
            .map(|r| r.trim_end_matches('/').trim_end_matches(".git"))
            .ok_or_else(|| {
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
    let tmp = prefix.join("tmp-self-update");
    if tmp.exists() {
        std::fs::remove_dir_all(&tmp).context("Remove stale tmp dir")?;
    }
    std::fs::create_dir_all(&tmp).context("Create tmp dir")?;

    ui::output::section("Checking for a pre-built binary...");
    let new_bin = match try_prebuilt(&tmp)? {
        Some(p) => p,
        None => {
            ui::output::detail("No pre-built binary available; building from source");
            let repo = args.repo.as_deref().unwrap_or(DEFAULT_REPO);
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
    use super::check_sums;

    const H: &str = "aaaa";

    #[test]
    fn match_ok() {
        let sums = format!("{H}  tsi-linux-x86_64\nbbbb  tsi-darwin-aarch64\n");
        assert!(check_sums(&sums, "tsi-linux-x86_64", H).is_ok());
    }

    #[test]
    fn mismatch() {
        let e = check_sums("bbbb  tsi-linux-x86_64\n", "tsi-linux-x86_64", H).unwrap_err();
        assert!(e.to_string().contains("mismatch"));
    }

    #[test]
    fn not_listed() {
        let e = check_sums(&format!("{H}  tsi-other\n"), "tsi-linux-x86_64", H).unwrap_err();
        assert!(e.to_string().contains("no entry"));
    }

    #[test]
    fn crlf_and_whitespace() {
        let sums = format!("  {H}   tsi-linux-x86_64  \r\nbbbb  x\r\n");
        assert!(check_sums(&sums, "tsi-linux-x86_64", H).is_ok());
    }
}
