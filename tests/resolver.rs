use std::collections::HashSet;
use std::path::Path;
use tsi::core::registry::Registry;
use tsi::core::resolver::{get_build_order, resolve};

#[test]
fn test_resolve_curl_dependencies() {
    let packages_dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures/packages");
    let registry = Registry::load_from_dir(&packages_dir).unwrap();
    let installed = HashSet::new();
    let packages = resolve(&registry, "curl", &installed).unwrap();
    let order = get_build_order(&packages);
    assert!(!order.is_empty());
    let names: Vec<&str> = order.iter().map(|p| p.name.as_str()).collect();
    assert!(names.contains(&"zlib"));
    assert!(names.contains(&"openssl"));
    assert!(names.contains(&"curl"));
}

/// Writes package definitions into a temp dir and loads a registry from it.
/// Each entry is (name, dependencies, build_dependencies).
fn registry_of(pkgs: &[(&str, &[&str], &[&str])]) -> (tempfile::TempDir, Registry) {
    let temp = tempfile::tempdir().unwrap();
    for (name, deps, build_deps) in pkgs {
        let json = format!(
            r#"{{"name":"{name}","version":"1.0",
                 "source":{{"type":"tarball","url":"https://e/x.tar.gz"}},
                 "dependencies":{},"build_dependencies":{}}}"#,
            serde_json::to_string(deps).unwrap(),
            serde_json::to_string(build_deps).unwrap()
        );
        std::fs::write(temp.path().join(format!("{name}.json")), json).unwrap();
    }
    let reg = Registry::load_from_dir(temp.path()).unwrap();
    (temp, reg)
}

fn position(order: &[tsi::core::package::Package], name: &str) -> usize {
    order.iter().position(|p| p.name == name).unwrap()
}

#[test]
fn dep_listed_in_both_lists_still_builds_before_its_dependent() {
    // Regression: a dep named in both `dependencies` and `build_dependencies` was counted
    // twice but decremented once, so the dependent never reached in-degree 0 and fell into
    // the unordered tail — where it kept its input position, ahead of its own dependency.
    // Input is deliberately unsorted, which is the whole point of get_build_order.
    let (_t, reg) = registry_of(&[
        ("app", &["zlib"], &["zlib"]),
        ("zlib", &["base"], &["base"]),
        ("base", &[], &[]),
    ]);
    let unsorted: Vec<_> = ["app", "zlib", "base"]
        .iter()
        .map(|n| reg.get(n).unwrap().clone())
        .collect();
    let order = get_build_order(&unsorted);
    assert_eq!(order.len(), 3);
    assert!(position(&order, "base") < position(&order, "zlib"));
    assert!(position(&order, "zlib") < position(&order, "app"));
}

#[test]
fn build_order_is_topological_for_a_chain() {
    let (_t, reg) = registry_of(&[
        ("a", &["b"], &[]),
        ("b", &["c"], &[]),
        ("c", &[], &["d"]),
        ("d", &[], &[]),
    ]);
    let order = get_build_order(&resolve(&reg, "a", &HashSet::new()).unwrap());
    let names: Vec<&str> = order.iter().map(|p| p.name.as_str()).collect();
    assert_eq!(names, vec!["d", "c", "b", "a"]);
}

#[test]
fn diamond_dependency_is_built_once() {
    let (_t, reg) = registry_of(&[
        ("top", &["left", "right"], &[]),
        ("left", &["base"], &[]),
        ("right", &["base"], &[]),
        ("base", &[], &[]),
    ]);
    let order = get_build_order(&resolve(&reg, "top", &HashSet::new()).unwrap());
    assert_eq!(order.len(), 4);
    assert_eq!(position(&order, "base"), 0);
    assert_eq!(position(&order, "top"), 3);
}

#[test]
fn circular_dependency_is_rejected() {
    let (_t, reg) = registry_of(&[("a", &["b"], &[]), ("b", &["a"], &[])]);
    let err = resolve(&reg, "a", &HashSet::new()).unwrap_err();
    assert!(err.to_string().contains("Circular"), "got: {err}");
}

#[test]
fn self_dependency_does_not_hang_or_drop_the_package() {
    // A package listing itself is malformed input; resolve reports the cycle rather than looping.
    let (_t, reg) = registry_of(&[("solo", &["solo"], &[])]);
    assert!(resolve(&reg, "solo", &HashSet::new()).is_err());
}

#[test]
fn missing_package_is_an_error() {
    let (_t, reg) = registry_of(&[("a", &["ghost"], &[])]);
    let err = resolve(&reg, "a", &HashSet::new()).unwrap_err();
    assert!(err.to_string().contains("not found"), "got: {err}");
}

#[test]
fn already_installed_deps_are_skipped() {
    let (_t, reg) = registry_of(&[("app", &["zlib"], &[]), ("zlib", &[], &[])]);
    let installed: HashSet<String> = ["zlib".to_string()].into_iter().collect();
    let order = get_build_order(&resolve(&reg, "app", &installed).unwrap());
    let names: Vec<&str> = order.iter().map(|p| p.name.as_str()).collect();
    assert_eq!(names, vec!["app"]);
}

#[test]
fn installed_root_resolves_to_nothing_to_do() {
    let (_t, reg) = registry_of(&[("app", &[], &[])]);
    let installed: HashSet<String> = ["app".to_string()].into_iter().collect();
    assert!(resolve(&reg, "app", &installed).unwrap().is_empty());
}
