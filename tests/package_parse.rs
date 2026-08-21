use std::path::Path;

use tsi::core::package::parse_package_file;

#[test]
fn test_cmake_args_falls_back_to_base_without_os_override() {
    let json = r#"{
        "name": "p",
        "version": "1",
        "source": { "type": "tarball", "url": "https://example.com/x.tar.gz" },
        "build_system": "cmake",
        "cmake_args": ["-DBASE=1"]
    }"#;
    let pkg = &parse_package_file(json).unwrap()[0];
    assert_eq!(pkg.cmake_args, vec!["-DBASE=1"]);
}

#[test]
fn test_cmake_args_os_override_replaces_base() {
    let json = r#"{
        "name": "p",
        "version": "1",
        "source": { "type": "tarball", "url": "https://example.com/x.tar.gz" },
        "build_system": "cmake",
        "cmake_args": ["-DBASE=1"],
        "cmake_args_darwin": ["-DDARWIN=1"],
        "cmake_args_linux": ["-DLINUX=1"],
        "cmake_args_windows": ["-DWINDOWS=1"]
    }"#;
    let pkg = &parse_package_file(json).unwrap()[0];
    #[cfg(target_os = "macos")]
    assert_eq!(pkg.cmake_args, vec!["-DDARWIN=1"]);
    #[cfg(target_os = "linux")]
    assert_eq!(pkg.cmake_args, vec!["-DLINUX=1"]);
    #[cfg(target_os = "windows")]
    assert_eq!(pkg.cmake_args, vec!["-DWINDOWS=1"]);
    #[cfg(not(any(target_os = "macos", target_os = "linux", target_os = "windows")))]
    assert_eq!(pkg.cmake_args, vec!["-DBASE=1"]);
}

#[test]
fn test_parse_all_packages() {
    let packages_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures/packages");
    let registry = tsi::core::registry::Registry::load_from_dir(&packages_dir).unwrap();
    assert!(
        registry.count() > 0,
        "Should have parsed at least one package"
    );
}

#[test]
fn test_platforms_absent_means_every_host() {
    let json = r#"{
        "name": "p",
        "version": "1",
        "source": { "type": "tarball", "url": "https://example.com/x.tar.gz" }
    }"#;
    let pkg = &parse_package_file(json).unwrap()[0];
    assert!(pkg.platforms.is_empty());
    assert!(pkg.supports_host());
}

#[test]
fn test_platforms_matches_bare_os_and_os_arch() {
    let host_os = tsi::platform::os_name();
    let host_arch = tsi::platform::arch_name();

    let mk = |platforms: &str| {
        let json = format!(
            r#"{{
                "name": "p",
                "version": "1",
                "source": {{ "type": "tarball", "url": "https://example.com/x.tar.gz" }},
                "platforms": {}
            }}"#,
            platforms
        );
        parse_package_file(&json).unwrap().remove(0)
    };

    assert!(mk(&format!(r#"["{}"]"#, host_os)).supports_host());
    assert!(mk(&format!(r#"["{}-{}"]"#, host_os, host_arch)).supports_host());
    // Right OS, wrong arch: os-arch entries must not match on OS alone.
    assert!(!mk(&format!(r#"["{}-nosucharch"]"#, host_os)).supports_host());
    assert!(!mk(r#"["nosuchos"]"#).supports_host());
}
