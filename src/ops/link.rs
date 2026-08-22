use crate::platform;
use anyhow::{Context, Result};
use std::fs;
use std::path::Path;

/// Where a package's directories land in the shared prefix.
///
/// `lib64` folds into `lib`. On x86_64 Linux plenty of projects install there
/// instead -- openssl does -- and while it went unlinked, nothing in it reached
/// the prefix: not the libraries, not the .pc files, and not the `-L`/rpath
/// that point at `<prefix>/lib`. openssl's own binary then resolved libssl from
/// the host instead of from the openssl TSI had just built, and reported 35
/// undefined symbols while its package showed as installed. aarch64 never saw
/// it, because there the same package installs into `lib`.
const LINKED_DIRS: &[(&str, &str)] = &[
    ("bin", "bin"),
    ("lib", "lib"),
    ("lib64", "lib"),
    ("include", "include"),
    ("share", "share"),
];

pub fn create_symlinks(package_install_dir: &Path, main_install_dir: &Path) -> Result<()> {
    for (src_name, dst_name) in LINKED_DIRS {
        let src_dir = package_install_dir.join(src_name);
        if !src_dir.is_dir() {
            continue;
        }
        let dst_dir = main_install_dir.join(dst_name);
        fs::create_dir_all(&dst_dir).context("Create link dir")?;
        let check_exec = *src_name == "bin";
        link_dir_contents(&src_dir, &dst_dir, check_exec)?;
    }
    // Autotools often install pkg-config files under lib/pkgconfig/; the top-level lib/ pass only
    // symlinks immediate files in lib/, so merge .pc files into the shared prefix for PKG_CONFIG_PATH.
    for libdir in ["lib", "lib64"] {
        let pkgconfig_src = package_install_dir.join(libdir).join("pkgconfig");
        if !pkgconfig_src.is_dir() {
            continue;
        }
        let pkgconfig_dst = main_install_dir.join("lib").join("pkgconfig");
        fs::create_dir_all(&pkgconfig_dst).context("Create pkgconfig dir")?;
        link_dir_contents(&pkgconfig_src, &pkgconfig_dst, false)?;
    }
    Ok(())
}

fn link_dir_contents(src_dir: &Path, dst_dir: &Path, check_executable: bool) -> Result<()> {
    for entry in fs::read_dir(src_dir).context("Read src dir")? {
        let entry = entry?;
        let src_path = entry.path();
        let name = entry.file_name();
        let dst_path = dst_dir.join(&name);

        if src_path.is_dir() {
            fs::create_dir_all(&dst_path).context("Create link subdir")?;
            link_dir_contents(&src_path, &dst_path, check_executable)?;
            continue;
        }
        if check_executable {
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                let meta = fs::metadata(&src_path).context("Stat file")?;
                if meta.permissions().mode() & 0o111 == 0 {
                    continue;
                }
            }
        }

        platform::create_symlink(&src_path, &dst_path)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn touch(path: &Path) {
        fs::create_dir_all(path.parent().unwrap()).unwrap();
        fs::write(path, b"x").unwrap();
    }

    #[test]
    fn lib64_lands_in_the_shared_lib_dir() {
        // openssl on x86_64 Linux installs here, and nothing that looks for a
        // library looks in <prefix>/lib64 -- not the rpath TSI records, not the
        // -L it passes. Left unlinked, the package installs and does not load.
        let tmp = tempfile::tempdir().unwrap();
        let pkg = tmp.path().join("openssl-3.2.1");
        let prefix = tmp.path().join("prefix");
        touch(&pkg.join("lib64/libssl.so.3"));
        touch(&pkg.join("lib64/pkgconfig/libssl.pc"));
        touch(&pkg.join("bin/openssl"));

        create_symlinks(&pkg, &prefix).unwrap();

        assert!(
            prefix.join("lib/libssl.so.3").exists(),
            "lib64 library not linked into lib/"
        );
        assert!(
            prefix.join("lib/pkgconfig/libssl.pc").exists(),
            "lib64 .pc not merged"
        );
        assert!(
            !prefix.join("lib64").exists(),
            "should fold into lib/, not recreate lib64/"
        );
    }

    #[test]
    fn a_package_using_plain_lib_is_unaffected() {
        let tmp = tempfile::tempdir().unwrap();
        let pkg = tmp.path().join("zlib-1.3.1");
        let prefix = tmp.path().join("prefix");
        touch(&pkg.join("lib/libz.so.1"));
        touch(&pkg.join("include/zlib.h"));

        create_symlinks(&pkg, &prefix).unwrap();

        assert!(prefix.join("lib/libz.so.1").exists());
        assert!(prefix.join("include/zlib.h").exists());
    }
}
