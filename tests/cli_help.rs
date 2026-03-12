use std::process::Command;

fn tsi_cmd(args: &[&str]) -> (std::process::Output, bool) {
    let exe = env!("CARGO_BIN_EXE_tsi");
    let output = Command::new(exe).args(args).output().unwrap();
    let success = output.status.success();
    (output, success)
}

#[test]
fn test_tsi_help() {
    let (output, success) = tsi_cmd(&["--help"]);
    assert!(
        success,
        "tsi --help should succeed: {}",
        String::from_utf8_lossy(&output.stderr)
    );
    assert!(String::from_utf8_lossy(&output.stdout).contains("TSI"));
    assert!(String::from_utf8_lossy(&output.stdout).contains("install"));
}

#[test]
fn test_tsi_version() {
    let (output, success) = tsi_cmd(&["--version"]);
    assert!(success, "tsi --version should succeed");
    assert!(String::from_utf8_lossy(&output.stdout).contains("tsi"));
}

#[test]
fn test_tsi_install_help() {
    let (output, success) = tsi_cmd(&["install", "--help"]);
    assert!(success, "tsi install --help should succeed");
    assert!(String::from_utf8_lossy(&output.stdout).contains("install"));
}

#[test]
fn test_tsi_list_help() {
    let (_, success) = tsi_cmd(&["list", "--help"]);
    assert!(success, "tsi list --help should succeed");
}

#[test]
fn test_tsi_uninstall_help() {
    let (_, success) = tsi_cmd(&["uninstall", "--help"]);
    assert!(success, "tsi uninstall --help should succeed");
}

#[test]
fn test_tsi_search_help() {
    let (_, success) = tsi_cmd(&["search", "--help"]);
    assert!(success, "tsi search --help should succeed");
}

#[test]
fn test_tsi_info_help() {
    let (_, success) = tsi_cmd(&["info", "--help"]);
    assert!(success, "tsi info --help should succeed");
}

#[test]
fn test_tsi_update_help() {
    let (_, success) = tsi_cmd(&["update", "--help"]);
    assert!(success, "tsi update --help should succeed");
}

#[test]
fn test_tsi_doctor_help() {
    let (_, success) = tsi_cmd(&["doctor", "--help"]);
    assert!(success, "tsi doctor --help should succeed");
}

#[test]
fn test_tsi_upgrade_help() {
    let (_, success) = tsi_cmd(&["upgrade", "--help"]);
    assert!(success, "tsi upgrade --help should succeed");
}
