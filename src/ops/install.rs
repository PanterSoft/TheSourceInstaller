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
    isolated: bool,
    verbose: bool,
) -> Result<()> {
    // A metapackage has nothing of its own to fetch, build or link: recording it
    // (with its dependencies, which the resolver has already installed) is the
    // whole job. `autotools` previously had to name *some* source to satisfy the
    // schema and downloaded GNU hello on every install.
    if pkg.build_system == "meta" {
        let deps: Vec<String> = pkg
            .dependencies
            .iter()
            .chain(pkg.build_dependencies.iter())
            .cloned()
            .collect();
        let marker = prefix
            .join("install")
            .join(format!("{}-{}", pkg.name, pkg.version));
        std::fs::create_dir_all(&marker).context("Create metapackage record dir")?;
        db.add(&pkg.name, &pkg.version, &marker, &deps)?;
        return Ok(());
    }

    let sources_dir = prefix.join("sources");
    let build_dir = prefix
        .join("build")
        .join(format!("{}-{}", pkg.name, pkg.version));
    let install_dir = prefix
        .join("install")
        .join(format!("{}-{}", pkg.name, pkg.version));
    let main_install = prefix.join("install");

    std::fs::create_dir_all(&sources_dir).context("Create sources dir")?;

    let fetched_dir = fetch::fetch(pkg, &sources_dir, force)?;
    let source_dir = pkg
        .source_dir
        .as_deref()
        .map(|sub| fetched_dir.join(sub))
        .unwrap_or(fetched_dir);

    if !source_dir.is_dir() {
        anyhow::bail!(
            "source_dir {:?} does not exist or is not a directory (check package source_dir)",
            source_dir
        );
    }

    if force && build_dir.exists() {
        std::fs::remove_dir_all(&build_dir).context("Remove build dir for --force")?;
    }

    build::build(
        pkg,
        &source_dir,
        &build_dir,
        &install_dir,
        &main_install,
        isolated,
        verbose,
    )?;

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
