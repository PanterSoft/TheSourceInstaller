use crate::core::database::Database;
use crate::core::package::Package;
use crate::ops::build;
use crate::ops::fetch;
use crate::ops::link;
use anyhow::{Context, Result};
use std::path::Path;

/// Fetches, builds, links, and records a package under prefix.
pub fn install_package(
    pkg: &Package,
    prefix: &Path,
    db: &mut Database,
    force: bool,
    verbose: bool,
) -> Result<()> {
    let sources_dir = prefix.join("sources");
    let build_dir = prefix
        .join("build")
        .join(format!("{}-{}", pkg.name, pkg.version));
    let install_dir = prefix
        .join("install")
        .join(format!("{}-{}", pkg.name, pkg.version));
    let main_install = prefix.join("install");

    std::fs::create_dir_all(&sources_dir).context("Create sources dir")?;

    let source_dir = fetch::fetch(pkg, &sources_dir, force)?;

    build::build(pkg, &source_dir, &build_dir, &install_dir, &main_install, verbose)?;

    link::create_symlinks(&install_dir, &main_install)?;

    let deps: Vec<String> = pkg
        .dependencies
        .iter()
        .chain(pkg.build_dependencies.iter())
        .cloned()
        .collect();
    db.add(&pkg.name, &pkg.version, &install_dir, &deps)?;

    Ok(())
}
