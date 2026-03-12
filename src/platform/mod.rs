use std::path::PathBuf;

#[cfg(unix)]
mod unix;

#[cfg(windows)]
mod windows;

#[cfg(unix)]
pub use unix::*;

#[cfg(windows)]
pub use windows::*;

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
