use crate::core::database::Database;

/// Ordered list of packages that form the self-hosting toolchain.
///
/// These are allowed to use system tools during the bootstrap phase. Once all of
/// them are installed, strict isolation can rely solely on TSI-provided tools.
pub const BOOTSTRAP_PACKAGES: &[&str] = &[
    "m4",
    "gmp",
    "mpfr",
    "mpc",
    "isl",
    "binutils",
    "gcc",
    "make",
    "patch",
    "tar",
    "gzip",
    "xz",
    "bzip2",
    "coreutils",
    "diffutils",
    "findutils",
    "sed",
    "grep",
    "gawk",
    "pkg-config",
];

/// Returns true if all bootstrap packages are installed.
pub fn is_bootstrap_complete(db: &Database) -> bool {
    BOOTSTRAP_PACKAGES
        .iter()
        .all(|name| db.is_installed(name))
}

/// Returns true if the given package is part of the bootstrap toolchain.
pub fn is_bootstrap_package(name: &str) -> bool {
    BOOTSTRAP_PACKAGES.contains(&name)
}

