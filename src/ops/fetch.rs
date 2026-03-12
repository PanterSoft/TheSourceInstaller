use crate::core::package::{Package, PackageSource};
use anyhow::{Context, Result};
use std::fs::File;
use std::io::{BufReader, Read, Write};
use std::path::Path;

pub fn fetch(pkg: &Package, dest_dir: &Path, force: bool) -> Result<std::path::PathBuf> {
    let source_dir = dest_dir.join(format!("{}-{}", pkg.name, pkg.version));
    if source_dir.exists() && !force {
        return Ok(source_dir);
    }

    match pkg.source.source_type.as_str() {
        "tarball" | "zip" => fetch_archive(pkg, dest_dir, force),
        "git" => fetch_git(pkg, dest_dir, force),
        "local" => fetch_local(pkg, dest_dir),
        _ => Err(anyhow::anyhow!("Unknown source type: {}", pkg.source.source_type)),
    }
}

fn fetch_archive(pkg: &Package, dest_dir: &Path, force: bool) -> Result<std::path::PathBuf> {
    let url = pkg
        .source
        .url
        .as_ref()
        .ok_or_else(|| anyhow::anyhow!("Missing URL for tarball source"))?;

    std::fs::create_dir_all(dest_dir).context("Failed to create dest dir")?;

    let filename = url.rsplit('/').next().unwrap_or("archive").to_string();
    let archive_path = dest_dir.join(&filename);

    if !archive_path.exists() || force {
        download_file(url, &archive_path)?;
    }

    let target_dir = dest_dir.join(format!("{}-{}", pkg.name, pkg.version));
    if target_dir.exists() {
        std::fs::remove_dir_all(&target_dir).context("Failed to remove existing source dir")?;
    }

    extract_archive(&archive_path, dest_dir)?;

    let entries: Vec<_> = std::fs::read_dir(dest_dir)?
        .filter_map(|e| e.ok())
        .filter(|e| {
            e.file_name()
                .to_str()
                .map_or(false, |n| !n.starts_with('.') && n != filename)
        })
        .collect();

    if entries.len() == 1 {
        let single = entries[0].path();
        if single.is_dir() && single != target_dir {
            std::fs::rename(&single, &target_dir).context("Rename extracted dir")?;
        }
    } else if !target_dir.exists() {
        std::fs::create_dir_all(&target_dir)?;
        for e in entries {
            let p = e.path();
            if p.is_dir() {
                let dest = target_dir.join(e.file_name());
                std::fs::rename(&p, &dest).context("Move extracted dir")?;
            } else {
                std::fs::rename(&p, target_dir.join(e.file_name())).context("Move extracted file")?;
            }
        }
    }

    Ok(target_dir)
}

pub fn download_file(url: &str, dest: &Path) -> Result<()> {
    let agent = ureq::Agent::new();
    let response = agent
        .get(url)
        .call()
        .map_err(|e| anyhow::anyhow!("Download failed: {}", e))?;

    let len = response
        .header("Content-Length")
        .and_then(|h| h.parse::<u64>().ok())
        .unwrap_or(0);

    let mut reader = response.into_reader();
    let mut file = File::create(dest).context("Failed to create file")?;
    let mut buf = [0u8; 8192];
    let mut downloaded: u64 = 0;

    loop {
        let n = reader.read(&mut buf).context("Read error")?;
        if n == 0 {
            break;
        }
        file.write_all(&buf[..n]).context("Write error")?;
        downloaded += n as u64;
        if len > 0 && downloaded % (1024 * 1024) < 8192 {
            log::debug!("Downloaded {} / {} bytes", downloaded, len);
        }
    }
    Ok(())
}

fn extract_archive(archive: &Path, dest: &Path) -> Result<()> {
    let ext = archive
        .extension()
        .and_then(|e| e.to_str())
        .unwrap_or("");
    let path_str = archive.to_string_lossy();

    if path_str.ends_with(".zip") {
        extract_zip(archive, dest)?;
    } else if path_str.ends_with(".tar.gz") || path_str.ends_with(".tgz") || ext == "gz" {
        extract_tar_gz(archive, dest)?;
    } else if path_str.ends_with(".tar.xz") || path_str.ends_with(".txz") || ext == "xz" {
        extract_tar_xz(archive, dest)?;
    } else if path_str.ends_with(".tar.bz2") || path_str.ends_with(".tbz2") {
        extract_tar_bz2(archive, dest)?;
    } else if path_str.ends_with(".tar") {
        extract_tar(archive, dest)?;
    } else {
        return Err(anyhow::anyhow!("Unsupported archive format: {}", path_str));
    }
    Ok(())
}

fn extract_tar_gz(archive: &Path, dest: &Path) -> Result<()> {
    let file = File::open(archive).context("Open archive")?;
    let dec = flate2::read::GzDecoder::new(BufReader::new(file));
    let mut tar = tar::Archive::new(dec);
    tar.unpack(dest).context("Extract tar.gz")?;
    Ok(())
}

fn extract_tar_xz(archive: &Path, dest: &Path) -> Result<()> {
    let file = File::open(archive).context("Open archive")?;
    let dec = xz2::read::XzDecoder::new(BufReader::new(file));
    let mut tar = tar::Archive::new(dec);
    tar.unpack(dest).context("Extract tar.xz")?;
    Ok(())
}

fn extract_tar_bz2(archive: &Path, dest: &Path) -> Result<()> {
    let file = File::open(archive).context("Open archive")?;
    let dec = bzip2::read::BzDecoder::new(BufReader::new(file));
    let mut tar = tar::Archive::new(dec);
    tar.unpack(dest).context("Extract tar.bz2")?;
    Ok(())
}

fn extract_tar(archive: &Path, dest: &Path) -> Result<()> {
    let file = File::open(archive).context("Open archive")?;
    let mut tar = tar::Archive::new(file);
    tar.unpack(dest).context("Extract tar")?;
    Ok(())
}

fn extract_zip(archive: &Path, dest: &Path) -> Result<()> {
    let file = File::open(archive).context("Open archive")?;
    let mut zip = zip::ZipArchive::new(BufReader::new(file)).context("Open zip")?;
    for i in 0..zip.len() {
        let mut entry = zip.by_index(i).context("Zip entry")?;
        let name = entry.name();
        if name.contains("..") {
            continue;
        }
        let out_path = dest.join(name);
        if name.ends_with('/') {
            std::fs::create_dir_all(&out_path).context("Create dir")?;
        } else {
            if let Some(parent) = out_path.parent() {
                std::fs::create_dir_all(parent).context("Create parent")?;
            }
            let mut out = File::create(&out_path).context("Create file")?;
            std::io::copy(&mut entry, &mut out).context("Extract file")?;
        }
    }
    Ok(())
}

fn fetch_git(pkg: &Package, dest_dir: &Path, force: bool) -> Result<std::path::PathBuf> {
    let url = pkg
        .source
        .url
        .as_ref()
        .ok_or_else(|| anyhow::anyhow!("Missing URL for git source"))?;

    let clone_dir = dest_dir.join(format!("{}-{}", pkg.name, pkg.version));
    if clone_dir.exists() {
        if force {
            std::fs::remove_dir_all(&clone_dir).context("Remove existing")?;
        } else {
            return Ok(clone_dir);
        }
    }

    let mut cmd = std::process::Command::new("git");
    cmd.arg("clone");
    if let Some(ref branch) = pkg.source.branch {
        cmd.args(["--branch", branch]);
    } else if let Some(ref tag) = pkg.source.tag {
        cmd.args(["--branch", tag]);
    }
    cmd.arg(url).arg(&clone_dir);

    let status = cmd.status().context("Failed to run git clone")?;
    if !status.success() {
        return Err(anyhow::anyhow!("git clone failed"));
    }

    if let Some(ref commit) = pkg.source.commit {
        let status = std::process::Command::new("git")
            .args(["checkout", commit])
            .current_dir(&clone_dir)
            .status()
            .context("git checkout")?;
        if !status.success() {
            return Err(anyhow::anyhow!("git checkout failed"));
        }
    }

    Ok(clone_dir)
}

fn fetch_local(pkg: &Package, dest_dir: &Path) -> Result<std::path::PathBuf> {
    let path = pkg
        .source
        .path
        .as_ref()
        .ok_or_else(|| anyhow::anyhow!("Missing path for local source"))?;

    let src = Path::new(path);
    if !src.exists() {
        return Err(anyhow::anyhow!("Local path does not exist: {}", path));
    }

    let dest = dest_dir.join(format!("{}-{}", pkg.name, pkg.version));
    if dest.exists() {
        std::fs::remove_dir_all(&dest).context("Remove existing")?;
    }
    copy_dir_all(src, &dest)?;
    Ok(dest)
}

fn copy_dir_all(src: &Path, dst: &Path) -> Result<()> {
    std::fs::create_dir_all(dst)?;
    for entry in std::fs::read_dir(src)? {
        let entry = entry?;
        let ty = entry.file_type()?;
        let dst_path = dst.join(entry.file_name());
        if ty.is_dir() {
            copy_dir_all(&entry.path(), &dst_path)?;
        } else {
            std::fs::copy(entry.path(), dst_path)?;
        }
    }
    Ok(())
}
