use std::path::Path;

#[test]
fn test_parse_all_packages() {
    let packages_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("packages");
    if !packages_dir.exists() {
        return;
    }
    let registry = tsi::core::registry::Registry::load_from_dir(&packages_dir).unwrap();
    assert!(
        registry.count() > 0,
        "Should have parsed at least one package"
    );
}
