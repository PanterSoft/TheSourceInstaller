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
