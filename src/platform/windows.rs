use std::fs;
use std::path::Path;

use anyhow::{Context, Result};

/// Remove a path whether it is a file symlink or a directory symlink.
/// On Windows, `fs::remove_file` fails with ERROR_ACCESS_DENIED on directory symlinks,
/// so we must dispatch based on the symlink metadata.
fn remove_any(path: &Path) -> Result<()> {
    let meta = fs::symlink_metadata(path).context("Failed to read symlink metadata")?;
    if meta.is_dir() {
        fs::remove_dir(path).context("Failed to remove directory symlink")?;
    } else {
        fs::remove_file(path).context("Failed to remove file symlink")?;
    }
    Ok(())
}

pub fn create_symlink(src: &Path, dst: &Path) -> Result<()> {
    if dst.exists() || fs::symlink_metadata(dst).is_ok() {
        remove_any(dst)?;
    }
    if let Some(parent) = dst.parent() {
        fs::create_dir_all(parent).context("Failed to create parent directory")?;
    }
    std::os::windows::fs::symlink_file(src, dst).context("Failed to create symlink")?;
    Ok(())
}

pub fn create_dir_symlink(src: &Path, dst: &Path) -> Result<()> {
    if dst.exists() || fs::symlink_metadata(dst).is_ok() {
        remove_any(dst)?;
    }
    if let Some(parent) = dst.parent() {
        fs::create_dir_all(parent).context("Failed to create parent directory")?;
    }
    std::os::windows::fs::symlink_dir(src, dst).context("Failed to create directory symlink")?;
    Ok(())
}
