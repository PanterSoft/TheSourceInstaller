use tsi::core::database::Database;

#[test]
fn test_database_create_and_add() {
    let temp = tempfile::tempdir().unwrap();
    let db_dir = temp.path();

    let mut db = Database::new(db_dir).unwrap();
    assert!(!db.is_installed("foo"));

    db.add("foo", "1.0.0", &db_dir.join("install/foo"), &[])
        .unwrap();
    assert!(db.is_installed("foo"));

    let pkg = db.get("foo").unwrap();
    assert_eq!(pkg.name, "foo");
    assert_eq!(pkg.version, "1.0.0");
}

#[test]
fn test_database_remove() {
    let temp = tempfile::tempdir().unwrap();
    let db_dir = temp.path();

    let mut db = Database::new(db_dir).unwrap();
    db.add(
        "bar",
        "2.0.0",
        &db_dir.join("install/bar"),
        &["dep1".into()],
    )
    .unwrap();
    assert!(db.is_installed("bar"));

    let removed = db.remove("bar").unwrap();
    assert!(removed);
    assert!(!db.is_installed("bar"));

    let removed_again = db.remove("bar").unwrap();
    assert!(!removed_again);
}

#[test]
fn test_database_persistence() {
    let temp = tempfile::tempdir().unwrap();
    let db_dir = temp.path();

    {
        let mut db = Database::new(db_dir).unwrap();
        db.add("baz", "3.0.0", &db_dir.join("install/baz"), &[])
            .unwrap();
    }

    let db = Database::new(db_dir).unwrap();
    assert!(db.is_installed("baz"));
    let pkg = db.get("baz").unwrap();
    assert_eq!(pkg.version, "3.0.0");
}

#[test]
fn test_reverse_dependencies() {
    let temp = tempfile::tempdir().unwrap();
    let db_dir = temp.path();

    let mut db = Database::new(db_dir).unwrap();
    db.add("zlib", "1.3", &db_dir.join("zlib"), &[]).unwrap();
    db.add("curl", "8.0", &db_dir.join("curl"), &["zlib".into()])
        .unwrap();
    db.add("git", "2.45", &db_dir.join("git"), &["zlib".into()])
        .unwrap();

    assert_eq!(db.reverse_dependencies("zlib"), vec!["curl", "git"]);
    assert!(db.reverse_dependencies("curl").is_empty());
    assert!(db.reverse_dependencies("not-installed").is_empty());

    // Once the dependents are gone, the leaf is free to remove.
    db.remove("curl").unwrap();
    db.remove("git").unwrap();
    assert!(db.reverse_dependencies("zlib").is_empty());
}

#[test]
fn test_save_leaves_no_temp_file_and_stays_parseable() {
    let temp = tempfile::tempdir().unwrap();
    let db_dir = temp.path();

    let mut db = Database::new(db_dir).unwrap();
    db.add("foo", "1.0", &db_dir.join("foo"), &[]).unwrap();

    assert!(!db_dir.join("installed.json.tmp").exists());
    let json = std::fs::read_to_string(db_dir.join("installed.json")).unwrap();
    let parsed: serde_json::Value = serde_json::from_str(&json).unwrap();
    assert_eq!(parsed["schema_version"], 1);
    assert_eq!(parsed["installed"][0]["name"], "foo");
}

#[test]
fn test_corrupt_database_reports_error_instead_of_silently_resetting() {
    let temp = tempfile::tempdir().unwrap();
    std::fs::write(temp.path().join("installed.json"), "{ truncated").unwrap();

    let err = Database::new(temp.path()).unwrap_err();
    assert!(err.to_string().contains("corrupted"), "got: {err}");
}

#[test]
fn test_add_existing_package_updates_in_place() {
    let temp = tempfile::tempdir().unwrap();
    let db_dir = temp.path();

    let mut db = Database::new(db_dir).unwrap();
    db.add("foo", "1.0", &db_dir.join("foo-1.0"), &[]).unwrap();
    db.add("foo", "2.0", &db_dir.join("foo-2.0"), &["bar".into()])
        .unwrap();

    assert_eq!(db.list().len(), 1);
    let pkg = db.get("foo").unwrap();
    assert_eq!(pkg.version, "2.0");
    assert_eq!(pkg.dependencies, vec!["bar"]);
}

#[test]
fn test_load_picks_up_changes_written_by_another_process() {
    let temp = tempfile::tempdir().unwrap();
    let db_dir = temp.path();

    let mut db = Database::new(db_dir).unwrap();
    {
        let mut other = Database::new(db_dir).unwrap();
        other.add("foo", "1.0", &db_dir.join("foo"), &[]).unwrap();
    }
    assert!(!db.is_installed("foo"));
    db.load().unwrap();
    assert!(db.is_installed("foo"));
}

#[test]
fn test_database_list_and_installed_set() {
    let temp = tempfile::tempdir().unwrap();
    let db_dir = temp.path();

    let mut db = Database::new(db_dir).unwrap();
    db.add("a", "1.0", &db_dir.join("a"), &[]).unwrap();
    db.add("b", "2.0", &db_dir.join("b"), &[]).unwrap();

    let list = db.list();
    assert_eq!(list.len(), 2);

    let set = db.installed_set();
    assert!(set.contains("a"));
    assert!(set.contains("b"));
}
