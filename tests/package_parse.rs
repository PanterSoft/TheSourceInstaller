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
