//! End-to-end CLI runs against a throwaway prefix. These cover the states a real
//! machine can be in — empty prefix, no package definitions, populated database —
//! without building anything from source.

use std::path::Path;
use std::process::{Command, Output};

fn tsi(prefix: &Path, args: &[&str]) -> Output {
    Command::new(env!("CARGO_BIN_EXE_tsi"))
        .args(args)
        .args(["--prefix", prefix.to_str().unwrap()])
        .output()
        .unwrap()
}

fn stdout(o: &Output) -> String {
    String::from_utf8_lossy(&o.stdout).to_string()
}

fn combined(o: &Output) -> String {
    format!("{}{}", stdout(o), String::from_utf8_lossy(&o.stderr))
}

/// Seeds `prefix/db/installed.json` with the given (name, deps) records.
fn seed_db(prefix: &Path, pkgs: &[(&str, &[&str])]) {
    let db_dir = prefix.join("db");
    std::fs::create_dir_all(&db_dir).unwrap();
    let installed: Vec<serde_json::Value> = pkgs
        .iter()
        .map(|(name, deps)| {
            serde_json::json!({
                "name": name,
                "version": "1.0.0",
                "install_path": prefix.join("install").join(name).to_string_lossy(),
                "installed_at": 0,
                "dependencies": deps,
            })
        })
        .collect();
    let body = serde_json::json!({ "schema_version": 1, "installed": installed });
    std::fs::write(
        db_dir.join("installed.json"),
        serde_json::to_string_pretty(&body).unwrap(),
    )
    .unwrap();
}

#[test]
fn list_on_a_fresh_prefix_reports_nothing_installed() {
    let temp = tempfile::tempdir().unwrap();
    let out = tsi(temp.path(), &["list"]);
    assert!(out.status.success(), "{}", combined(&out));
    assert!(combined(&out).contains("No packages installed"));
}

#[test]
fn list_json_on_a_fresh_prefix_is_an_empty_array() {
    let temp = tempfile::tempdir().unwrap();
    let out = tsi(temp.path(), &["list", "--json"]);
    assert!(out.status.success(), "{}", combined(&out));
    let v: serde_json::Value = serde_json::from_str(stdout(&out).trim()).unwrap();
    assert_eq!(v.as_array().unwrap().len(), 0);
}

#[test]
fn list_json_is_machine_readable() {
    let temp = tempfile::tempdir().unwrap();
    seed_db(temp.path(), &[("zlib", &[]), ("curl", &["zlib"])]);

    let out = tsi(temp.path(), &["list", "--json"]);
    assert!(out.status.success(), "{}", combined(&out));
    let v: serde_json::Value = serde_json::from_str(stdout(&out).trim()).unwrap();
    let names: Vec<&str> = v
        .as_array()
        .unwrap()
        .iter()
        .map(|p| p["name"].as_str().unwrap())
        .collect();
    assert_eq!(names, vec!["zlib", "curl"]);
    assert_eq!(v[1]["dependencies"][0], "zlib");
}

#[test]
fn uninstall_refuses_to_break_a_dependent_package() {
    let temp = tempfile::tempdir().unwrap();
    seed_db(temp.path(), &[("zlib", &[]), ("curl", &["zlib"])]);

    let out = tsi(temp.path(), &["uninstall", "zlib"]);
    let text = combined(&out);
    assert!(
        !out.status.success(),
        "a refused uninstall must exit non-zero"
    );
    assert!(text.contains("required by"), "{text}");
    assert!(text.contains("curl"), "{text}");

    // Still installed.
    let listed = stdout(&tsi(temp.path(), &["list", "--json"]));
    assert!(listed.contains("zlib"), "{listed}");
}

#[test]
fn uninstall_force_overrides_the_dependency_guard() {
    let temp = tempfile::tempdir().unwrap();
    seed_db(temp.path(), &[("zlib", &[]), ("curl", &["zlib"])]);

    let out = tsi(temp.path(), &["uninstall", "zlib", "--force"]);
    assert!(out.status.success(), "{}", combined(&out));

    let v: serde_json::Value =
        serde_json::from_str(stdout(&tsi(temp.path(), &["list", "--json"])).trim()).unwrap();
    let names: Vec<&str> = v
        .as_array()
        .unwrap()
        .iter()
        .map(|p| p["name"].as_str().unwrap())
        .collect();
    assert_eq!(names, vec!["curl"]);
}

#[test]
fn uninstall_a_leaf_package_needs_no_force() {
    let temp = tempfile::tempdir().unwrap();
    seed_db(temp.path(), &[("zlib", &[]), ("curl", &["zlib"])]);

    let out = tsi(temp.path(), &["uninstall", "curl"]);
    assert!(out.status.success(), "{}", combined(&out));
    let listed = stdout(&tsi(temp.path(), &["list", "--json"]));
    assert!(!listed.contains("curl"), "{listed}");
}

#[test]
fn uninstall_with_no_arguments_fails_with_usage() {
    let temp = tempfile::tempdir().unwrap();
    let out = tsi(temp.path(), &["uninstall"]);
    assert!(!out.status.success());
    assert!(combined(&out).contains("No packages specified"));
}

#[test]
fn install_without_package_definitions_tells_you_to_update() {
    let temp = tempfile::tempdir().unwrap();
    let out = tsi(temp.path(), &["install", "curl"]);
    assert!(!out.status.success());
    assert!(combined(&out).contains("tsi update"), "{}", combined(&out));
}

#[test]
fn search_without_package_definitions_tells_you_to_update() {
    let temp = tempfile::tempdir().unwrap();
    let out = tsi(temp.path(), &["search", "ssl"]);
    assert!(!out.status.success());
    assert!(combined(&out).contains("tsi update"));
}

#[test]
fn update_from_a_local_directory_populates_the_prefix() {
    let temp = tempfile::tempdir().unwrap();
    let fixtures = Path::new(env!("CARGO_MANIFEST_DIR")).join("tests/fixtures/packages");

    let out = tsi(
        temp.path(),
        &["update", "--local", fixtures.to_str().unwrap()],
    );
    assert!(out.status.success(), "{}", combined(&out));
    assert!(temp.path().join("packages/curl.json").is_file());

    // With definitions in place, search and info now work offline.
    let search = tsi(temp.path(), &["search", "curl"]);
    assert!(search.status.success(), "{}", combined(&search));
    assert!(combined(&search).contains("curl"));

    let info = tsi(temp.path(), &["info", "curl"]);
    assert!(info.status.success(), "{}", combined(&info));
    assert!(combined(&info).contains("zlib"), "{}", combined(&info));
}

#[test]
fn doctor_runs_on_a_fresh_prefix_without_failing() {
    let temp = tempfile::tempdir().unwrap();
    let out = tsi(temp.path(), &["doctor"]);
    assert!(out.status.success(), "{}", combined(&out));
    assert!(combined(&out).contains("tsi update"), "{}", combined(&out));
}

#[test]
fn a_held_install_lock_is_reported_not_silently_ignored() {
    let temp = tempfile::tempdir().unwrap();
    seed_db(temp.path(), &[("zlib", &[])]);

    // Simulate a concurrent tsi by holding the lock ourselves.
    let guard = tsi::ops::install_lock::acquire_install_lock(temp.path()).unwrap();
    let out = tsi(temp.path(), &["uninstall", "zlib"]);
    assert!(!out.status.success());
    assert!(
        combined(&out).contains("already running"),
        "{}",
        combined(&out)
    );
    drop(guard);

    // Lock released: the same command now goes through.
    let out = tsi(temp.path(), &["uninstall", "zlib"]);
    assert!(out.status.success(), "{}", combined(&out));
}

/// Writes a minimal package definition into `dir`, returning nothing.
/// `extra` is spliced in as raw JSON fields (e.g. `"platforms": ["linux"]`).
fn write_pkg(dir: &Path, name: &str, deps: &[&str], extra: &str) {
    std::fs::create_dir_all(dir).unwrap();
    let deps_json = serde_json::to_string(deps).unwrap();
    let body = format!(
        r#"{{
            "name": "{name}",
            "version": "1.0.0",
            "description": "fixture",
            "source": {{
                "type": "tarball",
                "url": "https://example.invalid/{name}-1.0.0.tar.gz"
            }},
            "dependencies": {deps_json},
            "build_system": "make"{extra}
        }}"#
    );
    std::fs::write(dir.join(format!("{name}.json")), body).unwrap();
}

#[test]
fn installing_a_package_unsupported_here_fails_before_any_download() {
    let temp = tempfile::tempdir().unwrap();
    let defs = temp.path().join("defs");
    write_pkg(&defs, "elsewhere", &[], r#", "platforms": ["nosuchos"]"#);

    let prefix = temp.path().join("prefix");
    let up = tsi(&prefix, &["update", "--local", defs.to_str().unwrap()]);
    assert!(up.status.success(), "{}", combined(&up));

    let out = tsi(&prefix, &["install", "elsewhere"]);
    assert!(!out.status.success(), "{}", combined(&out));
    assert!(
        combined(&out).contains("Unsupported on platform"),
        "{}",
        combined(&out)
    );
    // The source URL is unresolvable on purpose: reaching the fetch at all
    // would mean the platform gate ran too late to save a full build.
    assert!(
        !prefix.join("sources").exists(),
        "nothing should have been fetched"
    );
}

#[test]
fn an_unsupported_dependency_blocks_the_install_that_needs_it() {
    let temp = tempfile::tempdir().unwrap();
    let defs = temp.path().join("defs");
    write_pkg(&defs, "kernelish", &[], r#", "platforms": ["nosuchos"]"#);
    write_pkg(&defs, "dependent", &["kernelish"], "");

    let prefix = temp.path().join("prefix");
    tsi(&prefix, &["update", "--local", defs.to_str().unwrap()]);

    let out = tsi(&prefix, &["install", "dependent"]);
    assert!(!out.status.success(), "{}", combined(&out));
    // Names the dependency, not the package that was asked for.
    assert!(combined(&out).contains("kernelish"), "{}", combined(&out));
}

#[test]
fn a_package_with_no_platform_restriction_is_not_blocked() {
    let temp = tempfile::tempdir().unwrap();
    let defs = temp.path().join("defs");
    write_pkg(&defs, "portable", &[], "");

    let prefix = temp.path().join("prefix");
    tsi(&prefix, &["update", "--local", defs.to_str().unwrap()]);

    // The install still fails -- the URL is unresolvable -- but it must get
    // past the platform gate to a fetch error, not stop at "Unsupported".
    let out = tsi(&prefix, &["install", "portable"]);
    assert!(
        !combined(&out).contains("Unsupported on platform"),
        "{}",
        combined(&out)
    );
}

#[test]
fn update_local_pointed_at_its_own_packages_dir_does_not_empty_it() {
    let temp = tempfile::tempdir().unwrap();
    let prefix = temp.path().to_path_buf();
    let packages = prefix.join("packages");
    write_pkg(&packages, "zlib", &[], "");
    let before = std::fs::read_to_string(packages.join("zlib.json")).unwrap();
    assert!(!before.is_empty());

    let out = tsi(&prefix, &["update", "--local", packages.to_str().unwrap()]);
    assert!(!out.status.success(), "{}", combined(&out));

    let after = std::fs::read_to_string(packages.join("zlib.json")).unwrap();
    assert_eq!(after, before, "the registry must survive the refusal");
}

#[test]
fn a_metapackage_installs_its_dependencies_and_fetches_nothing() {
    let temp = tempfile::tempdir().unwrap();
    let defs = temp.path().join("defs");
    // A real dependency would need a real build; the point here is that the
    // metapackage itself never reaches the fetcher.
    std::fs::create_dir_all(&defs).unwrap();
    std::fs::write(
        defs.join("meta.json"),
        r#"{
            "name": "meta",
            "version": "1.0",
            "description": "metapackage",
            "dependencies": [],
            "build_system": "meta"
        }"#,
    )
    .unwrap();

    let prefix = temp.path().join("prefix");
    let up = tsi(&prefix, &["update", "--local", defs.to_str().unwrap()]);
    assert!(up.status.success(), "{}", combined(&up));

    let out = tsi(&prefix, &["install", "meta"]);
    assert!(out.status.success(), "{}", combined(&out));
    // No source was declared, so nothing may have been downloaded or unpacked.
    assert!(
        !prefix.join("sources").exists(),
        "a metapackage must not fetch: {}",
        combined(&out)
    );
    let listed = combined(&tsi(&prefix, &["list"]));
    assert!(
        listed.contains("meta"),
        "metapackage should be recorded as installed, got: {listed}"
    );
}

#[test]
fn a_metapackage_needs_no_source_field_to_parse() {
    let json = r#"{
        "name": "m",
        "version": "1.0",
        "source": { "type": "tarball", "url": "https://example.com/x.tar.gz" },
        "build_system": "meta"
    }"#;
    // A source is harmless to parse; the validator is what rejects declaring one.
    let pkg = &tsi::core::package::parse_package_file(json).unwrap()[0];
    assert_eq!(pkg.build_system, "meta");
}

#[test]
fn a_build_that_installs_nothing_is_not_a_success() {
    let temp = tempfile::tempdir().unwrap();
    let defs = temp.path().join("defs");
    let src = temp.path().join("src");
    std::fs::create_dir_all(&src).unwrap();
    std::fs::write(src.join("README"), "nothing to build").unwrap();

    // A custom build whose commands succeed but write nothing to the prefix --
    // the shape of liburing configuring without --prefix and installing to /usr.
    std::fs::create_dir_all(&defs).unwrap();
    std::fs::write(
        defs.join("hollow.json"),
        format!(
            r#"{{
                "name": "hollow",
                "version": "1.0",
                "description": "builds fine, installs nowhere",
                "source": {{ "type": "local", "path": "{}" }},
                "build_system": "custom",
                "build_commands": ["true"]
            }}"#,
            src.to_str().unwrap()
        ),
    )
    .unwrap();

    let prefix = temp.path().join("prefix");
    let up = tsi(&prefix, &["update", "--local", defs.to_str().unwrap()]);
    assert!(up.status.success(), "{}", combined(&up));

    let out = tsi(&prefix, &["install", "hollow"]);
    assert!(!out.status.success(), "{}", combined(&out));
    assert!(
        combined(&out).contains("installed no files"),
        "{}",
        combined(&out)
    );
}
