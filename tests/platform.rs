use tsi::platform;

#[test]
fn test_os_name_is_non_empty() {
    let name = platform::os_name();
    assert!(!name.is_empty());
    assert!(
        name == "darwin"
            || name == "linux"
            || name == "windows"
            || name == "freebsd"
            || name == "openbsd"
            || name == "netbsd"
            || name == "unknown"
    );
}

#[test]
fn test_default_prefix_contains_tsi() {
    let prefix = platform::default_prefix();
    let s = prefix.to_string_lossy();
    assert!(
        s.contains(".tsi"),
        "default_prefix should contain .tsi: {}",
        s
    );
}

#[test]
fn test_resolve_prefix_with_user_override() {
    let prefix = platform::resolve_prefix(Some("/custom/path"));
    assert_eq!(prefix.to_string_lossy(), "/custom/path");
}

#[test]
fn test_resolve_prefix_with_none_uses_default_or_detected() {
    let prefix = platform::resolve_prefix(None);
    let s = prefix.to_string_lossy();
    assert!(!s.is_empty());
}

#[test]
fn test_release_arch_matches_published_asset_names() {
    let cases = [
        ("x86_64-unknown-linux-musl", "x86_64"),
        ("aarch64-apple-darwin", "aarch64"),
        ("i686-unknown-linux-musl", "i686"),
        ("armv7-unknown-linux-musleabihf", "armv7"),
        ("arm-unknown-linux-musleabihf", "armv6"),
        ("riscv64gc-unknown-linux-musl", "riscv64"),
        ("powerpc64le-unknown-linux-musl", "ppc64le"),
        ("x86_64-pc-windows-msvc", "x86_64"),
    ];
    for (target, want) in cases {
        assert_eq!(platform::release_arch(target, "fallback"), want, "{target}");
    }
}

#[test]
fn test_release_arch_falls_back_without_an_asset() {
    assert_eq!(
        platform::release_arch("arm-unknown-linux-musleabi", "arm"),
        "arm"
    );
    assert_eq!(
        platform::release_arch("s390x-unknown-linux-gnu", "unknown"),
        "unknown"
    );
}

#[test]
fn test_release_platform_is_os_dash_arch() {
    let plat = platform::release_platform();
    assert!(
        plat.starts_with(&format!("{}-", platform::os_name())),
        "{plat}"
    );
}

#[cfg(unix)]
#[test]
fn test_system_dirs_are_never_detected_as_prefix() {
    for exe in [
        "/usr/bin/tsi",
        "/usr/local/bin/tsi",
        "/bin/tsi",
        "/opt/bin/tsi",
    ] {
        assert_eq!(
            platform::prefix_for_binary(std::path::Path::new(exe)),
            None,
            "{exe}"
        );
    }
    assert!(platform::is_system_dir(std::path::Path::new("/")));
    assert!(platform::is_system_dir(std::path::Path::new("/usr/")));
    if let Some(home) = dirs::home_dir() {
        assert!(platform::is_system_dir(&home));
        assert_eq!(platform::prefix_for_binary(&home.join("bin/tsi")), None);
    }
}

#[test]
fn test_prefix_for_binary_accepts_a_dedicated_prefix() {
    let tmp = tempfile::tempdir().unwrap();
    let prefix = tmp.path().join("tsi");
    let exe = prefix.join("bin").join("tsi");
    assert_eq!(platform::prefix_for_binary(&exe), Some(prefix.clone()));
    assert!(!platform::is_system_dir(&prefix));
    assert_eq!(platform::prefix_for_binary(&prefix.join("bin/other")), None);
    assert_eq!(
        platform::prefix_for_binary(&prefix.join("libexec/tsi")),
        None
    );
}

#[test]
fn test_mark_prefix_writes_the_marker() {
    let tmp = tempfile::tempdir().unwrap();
    let prefix = tmp.path().join("p");
    platform::mark_prefix(&prefix);
    assert!(prefix.join(platform::PREFIX_MARKER).is_file());
}

#[test]
fn test_parse_system_config() {
    assert_eq!(
        platform::parse_system_config("prefix = \"/var/lib/tsi\"\n"),
        Some(std::path::PathBuf::from("/var/lib/tsi"))
    );
    assert_eq!(platform::parse_system_config("prefix = \"\"\n"), None);
    assert_eq!(platform::parse_system_config("other = 1\n"), None);
    assert_eq!(platform::parse_system_config("not toml ["), None);
}
