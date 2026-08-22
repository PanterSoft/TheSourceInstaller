use crate::core::database::Database;
use crate::core::package::Package;
use crate::ops::build;
use crate::ops::fetch;
use crate::ops::link;
use anyhow::{Context, Result};
use std::path::Path;

fn is_dir_empty(dir: &Path) -> Result<bool> {
    let mut entries =
        std::fs::read_dir(dir).with_context(|| format!("Read install dir {}", dir.display()))?;
    Ok(entries.next().is_none())
}

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

    // A build that exits zero having installed nothing is not a success. It is
    // usually a package whose install step ignored the prefix it was given:
    // liburing ran `./configure` with no --prefix and then `make install
    // prefix=...`, which cannot retarget the paths configure had already baked
    // in, so the files landed in /usr and TSI reported OK over an empty tree.
    if is_dir_empty(&install_dir)? {
        anyhow::bail!(
            "{} installed no files into {}. Its build succeeded, so the install step \
             most likely wrote somewhere else -- check that the package passes the \
             prefix to configure, not only to `make install`.",
            pkg.name,
            install_dir.display()
        );
    }

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
