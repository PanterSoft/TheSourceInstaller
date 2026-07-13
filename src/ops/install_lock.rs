//! Per-prefix lock so only one install or upgrade runs at a time.

use anyhow::{anyhow, Context, Result};
use fs4::FileExt;
use std::fs::{File, OpenOptions};
use std::path::Path;

const LOCK_FILE_NAME: &str = ".tsi-install.lock";

/// Guard that holds the install lock. Dropping it releases the lock.
pub struct InstallLockGuard {
    _file: File,
}

/// Acquires an exclusive lock for install/upgrade on the given prefix.
/// If another tsi command already holds the lock for this prefix, returns an error
/// with a clear message instead of blocking.
pub fn acquire_install_lock(prefix: &Path) -> Result<InstallLockGuard> {
    std::fs::create_dir_all(prefix).context("Create prefix dir for lock")?;
    let lock_path = prefix.join(LOCK_FILE_NAME);
    let file = OpenOptions::new()
        .create(true)
        .truncate(false)
        .read(true)
        .write(true)
        .open(&lock_path)
        .with_context(|| format!("Open lock file: {}", lock_path.display()))?;

    // Non-blocking: if another process holds the lock, fail fast with a friendly message.
    match file.try_lock_exclusive() {
        Ok(()) => {}
        Err(_) => {
            return Err(anyhow!(
                "Another tsi command is already running for this prefix.\n\
                 Wait for it to finish, or if you're sure no other tsi process is running,\n\
                 remove the stale lock file: {}",
                lock_path.display()
            ));
        }
    }

    Ok(InstallLockGuard { _file: file })
}
