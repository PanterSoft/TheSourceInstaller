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

const SUBCOMMANDS: &[&str] = &[
    "install",
    "list",
    "uninstall",
    "search",
    "info",
    "update",
    "doctor",
    "upgrade",
];

#[test]
fn test_tsi_subcommand_help() {
    for subcmd in SUBCOMMANDS {
        let (output, success) = tsi_cmd(&[subcmd, "--help"]);
        assert!(
            success,
            "tsi {} --help should succeed: {}",
            subcmd,
            String::from_utf8_lossy(&output.stderr)
        );
    }
}
