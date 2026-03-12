use std::fs;
use std::path::Path;

use anyhow::{Context, Result};

pub fn create_symlink(src: &Path, dst: &Path) -> Result<()> {
    if dst.exists() {
        fs::remove_file(dst).context("Failed to remove existing symlink target")?;
    }
    if let Some(parent) = dst.parent() {
        fs::create_dir_all(parent).context("Failed to create parent directory")?;
    }
    std::os::windows::fs::symlink_file(src, dst).context("Failed to create symlink")?;
    Ok(())
}

pub fn create_dir_symlink(src: &Path, dst: &Path) -> Result<()> {
    if dst.exists() {
        fs::remove_file(dst).context("Failed to remove existing symlink target")?;
    }
    if let Some(parent) = dst.parent() {
        fs::create_dir_all(parent).context("Failed to create parent directory")?;
    }
    std::os::windows::fs::symlink_dir(src, dst).context("Failed to create directory symlink")?;
    Ok(())
}
