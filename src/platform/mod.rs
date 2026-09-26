use std::path::PathBuf;

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

pub fn resolve_prefix(user_prefix: Option<&str>) -> PathBuf {
    if let Some(p) = user_prefix {
        return PathBuf::from(p);
    }
    if let Some(p) = detect_prefix_from_binary() {
        return p;
    }
    default_prefix()
}

fn detect_prefix_from_binary() -> Option<PathBuf> {
    let exe = std::env::current_exe().ok()?;
    let exe_str = exe.to_string_lossy();
    let bin_tsi = if cfg!(windows) {
        r"\bin\tsi.exe"
    } else {
        "/bin/tsi"
    };
    if let Some(pos) = exe_str.find(bin_tsi) {
        let prefix = exe_str[..pos].to_string();
        if !prefix.is_empty() {
            return Some(PathBuf::from(prefix));
        }
    }
    None
}
