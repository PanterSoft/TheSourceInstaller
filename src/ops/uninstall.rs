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

/// Removes a replaced version's links and files. The database already points at the
/// new version, so this works from the old install path instead of a package name.
pub fn remove_old_install(old_install_path: &Path, prefix: &Path) -> Result<()> {
    if old_install_path.exists() {
        remove_symlinks_to_package(old_install_path, &prefix.join("install"))?;
        fs::remove_dir_all(old_install_path).context("Remove old install dir")?;
    }
    Ok(())
}

/// Undoes `link::create_symlinks`: it links `lib64` into `lib` and recurses into
/// subdirectories, so the shared dirs are walked recursively rather than one level.
fn remove_symlinks_to_package(package_install_dir: &Path, main_install_dir: &Path) -> Result<()> {
    for subdir in ["bin", "lib", "include", "share"] {
        let main_subdir = main_install_dir.join(subdir);
        if main_subdir.is_dir() {
            remove_links_in(&main_subdir, package_install_dir)?;
        }
    }
    Ok(())
}

/// Returns true when a link was removed from `dir` or below. Subdirectories that
/// this leaves empty are removed too (link.rs creates them); `dir` itself stays.
fn remove_links_in(dir: &Path, package_install_dir: &Path) -> Result<bool> {
    let mut removed = false;
    for entry in fs::read_dir(dir).context("Read main dir")? {
        let entry = entry?;
        let path = entry.path();
        let file_type = entry.file_type()?;
        if file_type.is_symlink() {
            if let Ok(target) = fs::read_link(&path) {
                if target.starts_with(package_install_dir) {
                    fs::remove_file(&path).context("Remove symlink")?;
                    removed = true;
                }
            }
        } else if file_type.is_dir()
            && remove_links_in(&path, package_install_dir)?
            && fs::read_dir(&path)?.next().is_none()
        {
            fs::remove_dir(&path).context("Remove empty link dir")?;
            removed = true;
        }
    }
    Ok(removed)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ops::link;

    fn touch(path: &Path) {
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        fs::write(path, b"x").unwrap();
    }

    #[test]
    fn removes_lib64_and_nested_links_and_keeps_others() {
        let tmp = tempfile::tempdir().unwrap();
        let main = tmp.path().join("install");
        let pkg = main.join("openssl-3.2.1");
        let other = main.join("zlib-1.3.1");
        touch(&pkg.join("lib64/libssl.so.3"));
        touch(&pkg.join("lib64/pkgconfig/libssl.pc"));
        touch(&pkg.join("include/openssl/ssl.h"));
        touch(&pkg.join("share/man/man1/openssl.1"));
        touch(&other.join("lib/libz.so.1"));
        touch(&other.join("lib/pkgconfig/zlib.pc"));
        link::create_symlinks(&pkg, &main).unwrap();
        link::create_symlinks(&other, &main).unwrap();
        assert!(main.join("lib/libssl.so.3").is_symlink());

        remove_symlinks_to_package(&pkg, &main).unwrap();

        assert!(!main.join("lib/libssl.so.3").is_symlink());
        assert!(!main.join("lib/pkgconfig/libssl.pc").is_symlink());
        assert!(!main.join("include/openssl").exists());
        assert!(!main.join("share/man").exists());
        assert!(main.join("lib/libz.so.1").exists());
        assert!(main.join("lib/pkgconfig/zlib.pc").exists());
    }
}
