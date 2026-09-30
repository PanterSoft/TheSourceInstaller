use std::path::{Path, PathBuf};

#[cfg(unix)]
mod unix;

#[cfg(windows)]
mod windows;

#[cfg(unix)]
pub use unix::*;

#[cfg(windows)]
pub use windows::*;

pub fn arch_name() -> &'static str {
    #[cfg(target_arch = "x86_64")]
    return "x86_64";

    #[cfg(target_arch = "aarch64")]
    return "aarch64";

    #[cfg(target_arch = "x86")]
    return "x86";

    #[cfg(target_arch = "arm")]
    return "arm";

    #[cfg(not(any(
        target_arch = "x86_64",
        target_arch = "aarch64",
        target_arch = "x86",
        target_arch = "arm"
    )))]
    return "unknown";
}

/// The `<os>-<arch>` suffix of this build's release asset (`tsi-<suffix>`), matching
/// the names `.github/workflows/build-binaries.yml` publishes and `tsi-bootstrap.sh`
/// downloads. Unlike `arch_name()`, it tells apart targets that share a
/// `target_arch` (armv6 vs armv7) and uses the conventional names for the rest.
pub fn release_platform() -> String {
    format!(
        "{}-{}",
        os_name(),
        release_arch(env!("TSI_BUILD_TARGET"), arch_name())
    )
}

/// Maps a Rust target triple to the arch part of a release asset name, falling back
/// to `fallback` for triples that have no dedicated asset.
pub fn release_arch<'a>(target: &str, fallback: &'a str) -> &'a str {
    let arch = target.split('-').next().unwrap_or("");
    match arch {
        "x86_64" => "x86_64",
        "aarch64" => "aarch64",
        "i686" | "i586" => "i686",
        // The ARM assets are hard-float; a soft-float build has no asset to take.
        a if a.starts_with("armv7") && target.ends_with("hf") => "armv7",
        "arm" | "armv6" if target.ends_with("hf") => "armv6",
        a if a.starts_with("riscv64") => "riscv64",
        "powerpc64le" => "ppc64le",
        _ => fallback,
    }
}

pub fn os_name() -> &'static str {
    #[cfg(target_os = "macos")]
    return "darwin";

    #[cfg(target_os = "linux")]
    return "linux";

    #[cfg(target_os = "windows")]
    return "windows";

    #[cfg(target_os = "freebsd")]
    return "freebsd";

    #[cfg(target_os = "openbsd")]
    return "openbsd";

    #[cfg(target_os = "netbsd")]
    return "netbsd";

    #[cfg(not(any(
        target_os = "macos",
        target_os = "linux",
        target_os = "windows",
        target_os = "freebsd",
        target_os = "openbsd",
        target_os = "netbsd"
    )))]
    return "unknown";
}

pub fn default_prefix() -> PathBuf {
    dirs::home_dir()
        .map(|h| h.join(".tsi"))
        .unwrap_or_else(|| PathBuf::from(".tsi"))
}

/// File that marks a directory as a TSI prefix. `tsi-bootstrap.sh` and the first
/// command that writes to a prefix create it.
pub const PREFIX_MARKER: &str = ".tsi-prefix";

/// System-wide config that can point TSI at its data directory, e.g. for a distro
/// package that ships the binary as `/usr/bin/tsi`.
#[cfg(unix)]
pub const SYSTEM_CONFIG: &str = "/etc/tsi.conf";

/// Where TSI keeps its data, in order of precedence: `--prefix`, the `TSI_PREFIX`
/// environment variable, the prefix the running binary sits in (`<prefix>/bin/tsi`,
/// only when that is a real TSI prefix), `prefix` in `/etc/tsi.conf`, `~/.tsi`.
pub fn resolve_prefix(user_prefix: Option<&str>) -> PathBuf {
    if let Some(p) = user_prefix {
        return PathBuf::from(p);
    }
    if let Some(p) = std::env::var_os("TSI_PREFIX").filter(|p| !p.is_empty()) {
        return PathBuf::from(p);
    }
    if let Some(p) = detect_prefix_from_binary() {
        return p;
    }
    if let Some(p) = prefix_from_system_config() {
        return p;
    }
    default_prefix()
}

fn detect_prefix_from_binary() -> Option<PathBuf> {
    let exe = std::env::current_exe().ok()?;
    prefix_for_binary(&exe)
}

/// The prefix a binary at `exe` belongs to: the parent of its `bin/` directory,
/// provided that directory is a TSI prefix. A directory with the marker file always
/// is; one without it (installs from before the marker existed) is accepted unless
/// it is a system directory, so `/usr/bin/tsi` never makes `/usr` the prefix.
pub fn prefix_for_binary(exe: &Path) -> Option<PathBuf> {
    let file = exe.file_name()?.to_str()?;
    if file != "tsi" && file != "tsi.exe" {
        return None;
    }
    let bin = exe.parent()?;
    if bin.file_name()? != "bin" {
        return None;
    }
    let prefix = bin.parent()?;
    if prefix.as_os_str().is_empty() {
        return None;
    }
    if prefix.join(PREFIX_MARKER).is_file() {
        return Some(prefix.to_path_buf());
    }
    if is_system_dir(prefix) {
        return None;
    }
    Some(prefix.to_path_buf())
}

#[cfg(unix)]
fn prefix_from_system_config() -> Option<PathBuf> {
    let text = std::fs::read_to_string(SYSTEM_CONFIG).ok()?;
    parse_system_config(&text)
}

#[cfg(not(unix))]
fn prefix_from_system_config() -> Option<PathBuf> {
    None
}

/// Reads `prefix = "/some/dir"` from the TOML text of `/etc/tsi.conf`.
pub fn parse_system_config(text: &str) -> Option<PathBuf> {
    #[derive(serde::Deserialize)]
    struct SystemConfig {
        prefix: Option<String>,
    }
    let cfg: SystemConfig = toml::from_str(text).ok()?;
    cfg.prefix.filter(|p| !p.is_empty()).map(PathBuf::from)
}

/// True for directories that are never a TSI prefix: the filesystem root, the
/// standard system hierarchies and the user's home directory. TSI refuses to
/// detect one of these as its prefix or to remove one.
pub fn is_system_dir(path: &Path) -> bool {
    let path = normalize(path);
    if path.parent().is_none() {
        return true; // `/`, `C:\`
    }
    if let Some(home) = dirs::home_dir() {
        if path == normalize(&home) {
            return true;
        }
    }
    #[cfg(unix)]
    const SYSTEM: &[&str] = &[
        "/usr",
        "/usr/local",
        "/opt",
        "/bin",
        "/sbin",
        "/lib",
        "/lib64",
        "/etc",
        "/var",
        "/var/lib",
        "/home",
        "/root",
        "/tmp",
        "/srv",
        "/boot",
        "/dev",
        "/proc",
        "/sys",
        "/run",
        "/mnt",
        "/media",
        "/snap",
        "/nix",
        "/Applications",
        "/Library",
        "/System",
        "/Users",
        "/private",
        "/opt/homebrew",
        "/usr/pkg",
    ];
    #[cfg(not(unix))]
    const SYSTEM: &[&str] = &[];
    if SYSTEM
        .iter()
        .any(|s| path == Path::new(s) || path == normalize(Path::new(s)))
    {
        return true;
    }
    #[cfg(windows)]
    for var in [
        "SystemRoot",
        "ProgramFiles",
        "ProgramFiles(x86)",
        "ProgramData",
    ] {
        if let Some(dir) = std::env::var_os(var) {
            if path == normalize(Path::new(&dir)) {
                return true;
            }
        }
    }
    false
}

/// Resolves symlinks when the path exists (so `/usr/../usr` or a symlinked home
/// compare equal), and otherwise drops trailing separators and `.` components.
fn normalize(path: &Path) -> PathBuf {
    std::fs::canonicalize(path).unwrap_or_else(|_| path.components().collect())
}

/// Writes the prefix marker file, creating the prefix if needed. Errors are
/// ignored: the marker only helps detection.
pub fn mark_prefix(prefix: &Path) {
    let marker = prefix.join(PREFIX_MARKER);
    if marker.exists() {
        return;
    }
    if std::fs::create_dir_all(prefix).is_ok() {
        let _ = std::fs::write(
            &marker,
            "This directory is a TSI (The Source Installer) prefix.\n",
        );
    }
}
