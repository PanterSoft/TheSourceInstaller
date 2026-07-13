use crate::platform;
use anyhow::{Context, Result};
use std::fs;
use std::path::Path;

pub fn create_symlinks(package_install_dir: &Path, main_install_dir: &Path) -> Result<()> {
    for subdir in ["bin", "lib", "include", "share"] {
        let src_dir = package_install_dir.join(subdir);
        if !src_dir.is_dir() {
            continue;
        }
        let dst_dir = main_install_dir.join(subdir);
        fs::create_dir_all(&dst_dir).context("Create link dir")?;
        let check_exec = subdir == "bin";
        link_dir_contents(&src_dir, &dst_dir, check_exec)?;
    }
    // Autotools often install pkg-config files under lib/pkgconfig/; the top-level lib/ pass only
    // symlinks immediate files in lib/, so merge .pc files into the shared prefix for PKG_CONFIG_PATH.
    let pkgconfig_src = package_install_dir.join("lib").join("pkgconfig");
    if pkgconfig_src.is_dir() {
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
