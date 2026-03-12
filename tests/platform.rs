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
    assert!(s.contains(".tsi"), "default_prefix should contain .tsi: {}", s);
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
