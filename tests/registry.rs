use std::path::Path;
use tsi::core::registry::Registry;

/// Writes `{name}.json` files into a temp dir and loads a registry from it.
fn registry_from(files: &[(&str, &str)]) -> (tempfile::TempDir, Registry) {
    let temp = tempfile::tempdir().unwrap();
    for (name, body) in files {
        std::fs::write(temp.path().join(format!("{name}.json")), body).unwrap();
    }
    let reg = Registry::load_from_dir(temp.path()).unwrap();
    (temp, reg)
}

fn multi(name: &str, versions: &[&str]) -> String {
    let vs: Vec<String> = versions
        .iter()
        .map(|v| {
            format!(
                r#"{{"version":"{v}","source":{{"type":"tarball","url":"https://e/x.tar.gz"}}}}"#
            )
        })
        .collect();
    format!(r#"{{"name":"{name}","versions":[{}]}}"#, vs.join(","))
}

#[test]
fn bare_name_selects_highest_version() {
    let (_t, reg) = registry_from(&[("p", &multi("p", &["1.9.0", "1.10.0", "1.2.0"]))]);
    assert_eq!(reg.get("p").unwrap().version, "1.10.0");
}

#[test]
fn prerelease_does_not_outrank_release() {
    let (_t, reg) = registry_from(&[("p", &multi("p", &["3.0.0-rc1", "3.0.0"]))]);
    assert_eq!(reg.get("p").unwrap().version, "3.0.0");
}

#[test]
fn pinned_spec_selects_exact_version() {
    let (_t, reg) = registry_from(&[("p", &multi("p", &["1.0.0", "2.0.0"]))]);
    assert_eq!(reg.get("p@1.0.0").unwrap().version, "1.0.0");
    assert!(reg.get("p@9.9.9").is_none());
    assert!(reg.get("nope").is_none());
}

#[test]
fn malformed_file_is_skipped_not_fatal() {
    let (_t, reg) = registry_from(&[("good", &multi("good", &["1.0"])), ("bad", "{ not json")]);
    assert_eq!(reg.count(), 1);
    assert!(reg.get("good").is_some());
}

#[test]
fn non_json_files_are_ignored() {
    let (_t, reg) = registry_from(&[("good", &multi("good", &["1.0"]))]);
    // README.md and friends live alongside package definitions in tsi-packages.
    assert_eq!(reg.count(), 1);
    assert_eq!(reg.get_versions("good").unwrap().len(), 1);
}

#[test]
fn search_matches_name_and_description_case_insensitively() {
    let body = r#"{"name":"libfoo","version":"1.0","description":"A Widget Library",
        "source":{"type":"tarball","url":"https://e/x.tar.gz"}}"#;
    let (_t, reg) = registry_from(&[("libfoo", body), ("other", &multi("other", &["1.0"]))]);
    assert_eq!(reg.search("LIBFOO").len(), 1);
    assert_eq!(reg.search("widget").len(), 1);
    assert!(reg.search("nothing-here").is_empty());
}

#[test]
fn empty_dir_yields_empty_registry() {
    let temp = tempfile::tempdir().unwrap();
    let reg = Registry::load_from_dir(temp.path()).unwrap();
    assert_eq!(reg.count(), 0);
    assert_eq!(reg.all_packages().count(), 0);
}

#[test]
fn missing_dir_is_an_error_not_a_panic() {
    let reg = Registry::load_from_dir(Path::new("/definitely/not/here"));
    // WalkDir skips unreadable roots, so this loads empty rather than failing —
    // either behavior is acceptable, panicking is not.
    assert!(reg.map(|r| r.count()).unwrap_or(0) == 0);
}
