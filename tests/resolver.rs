use std::collections::HashSet;
use std::path::Path;

#[test]
fn test_resolve_curl_dependencies() {
    let packages_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures/packages");
    let registry = tsi::core::registry::Registry::load_from_dir(&packages_dir).unwrap();
    let installed = HashSet::new();
    let packages = tsi::core::resolver::resolve(&registry, "curl", &installed).unwrap();
    let order = tsi::core::resolver::get_build_order(&packages);
    assert!(!order.is_empty());
    let names: Vec<&str> = order.iter().map(|p| p.name.as_str()).collect();
    assert!(names.contains(&"zlib"));
    assert!(names.contains(&"openssl"));
    assert!(names.contains(&"curl"));
}
