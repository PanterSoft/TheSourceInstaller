use crate::core::database::Database;
use anyhow::{Context, Result};
use std::fs;
use std::path::Path;

pub fn uninstall_package(name: &str, prefix: &Path, db: &mut Database) -> Result<bool> {
    let pkg = match db.get(name) {
        Some(p) => p,
        None => return Ok(false),
    };

    let install_path = Path::new(&pkg.install_path);
    if install_path.exists() {
        remove_symlinks_to_package(install_path, &prefix.join("install"))?;
        fs::remove_dir_all(install_path).context("Remove install dir")?;
    }

    db.remove(name)?;
    Ok(true)
}

fn remove_symlinks_to_package(package_install_dir: &Path, main_install_dir: &Path) -> Result<()> {
    for subdir in ["bin", "lib", "include", "share"] {
        let pkg_subdir = package_install_dir.join(subdir);
        if !pkg_subdir.is_dir() {
            continue;
        }
        let main_subdir = main_install_dir.join(subdir);
        if !main_subdir.is_dir() {
            continue;
        }
        for entry in fs::read_dir(&main_subdir).context("Read main dir")? {
            let entry = entry?;
            let path = entry.path();
            if path.is_symlink() {
                if let Ok(target) = fs::read_link(&path) {
                    if target.starts_with(package_install_dir) {
                        fs::remove_file(&path).context("Remove symlink")?;
                    }
                }
            }
        }
    }
    Ok(())
}
