use crate::core::package::Package;
use anyhow::{Context, Result};
use std::fs::File;
use std::io::{BufReader, Read, Write};
use std::path::Path;

/// Fetches package sources (archive, git, or local) into dest_dir and returns the source tree path.
pub fn fetch(pkg: &Package, dest_dir: &Path, force: bool) -> Result<std::path::PathBuf> {
    let source_dir = dest_dir.join(format!("{}-{}", pkg.name, pkg.version));
    if source_dir.exists() && !force {
        return Ok(source_dir);
    }

    match pkg.source.source_type.as_str() {
        "tarball" | "zip" => fetch_archive(pkg, dest_dir, force),
        "git" => fetch_git(pkg, dest_dir, force),
        "local" => fetch_local(pkg, dest_dir),
        _ => Err(anyhow::anyhow!(
            "Unknown source type: {}",
            pkg.source.source_type
        )),
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
        download_file_with_retry(url, &archive_path)?;
    }

    // Verify SHA-256 checksum if the package definition supplies one, whether the archive
    // was just downloaded or reused from a previous run. This catches a truncated/corrupt
    // file left behind by a failed prior run that would otherwise be handed to extraction.
    if let Some(expected) = &pkg.source.sha256 {
        let actual = crate::util::sha256::sha256_file(&archive_path)
            .context("Computing SHA-256 of archive")?;
        if actual != expected.to_lowercase() {
            // Remove the bad file and re-download once before giving up.
            let _ = std::fs::remove_file(&archive_path);
            download_file_with_retry(url, &archive_path)?;
            let actual = crate::util::sha256::sha256_file(&archive_path)
                .context("Computing SHA-256 of re-downloaded archive")?;
            if actual != expected.to_lowercase() {
                let _ = std::fs::remove_file(&archive_path);
                anyhow::bail!(
                    "SHA-256 mismatch for {}: expected {}, got {}",
                    archive_path.display(),
                    expected,
                    actual
                );
            }
        }
        log::debug!("SHA-256 verified for {}", archive_path.display());
    }

    let target_dir = dest_dir.join(format!("{}-{}", pkg.name, pkg.version));
    if target_dir.exists() {
        std::fs::remove_dir_all(&target_dir).context("Failed to remove existing source dir")?;
    }

    // Unpack into a dedicated scratch dir. Extracting into `dest_dir` would list *all* sibling
    // source trees when counting entries, and the multi-entry branch would incorrectly move
    // unrelated packages into this package's target directory.
    let scratch = dest_dir.join(format!(".unpack-{}-{}", pkg.name, pkg.version));
    if scratch.exists() {
        std::fs::remove_dir_all(&scratch).context("Remove stale unpack scratch dir")?;
    }
    std::fs::create_dir_all(&scratch).context("Create unpack scratch dir")?;
    extract_archive(&archive_path, &scratch)?;

    let entries: Vec<_> = std::fs::read_dir(&scratch)?
        .filter_map(|e| e.ok())
        .filter(|e| e.file_name().to_str().is_some_and(|n| !n.starts_with('.')))
        .collect();

    if entries.len() == 1 {
        let single = entries[0].path();
        if single.is_dir() {
            std::fs::rename(&single, &target_dir).context("Rename extracted dir")?;
        } else {
            std::fs::create_dir_all(&target_dir)?;
            std::fs::rename(&single, target_dir.join(entries[0].file_name()))
                .context("Move extracted file into source dir")?;
        }
    } else if entries.is_empty() {
        anyhow::bail!(
            "Archive contained no files after extract: {}",
            archive_path.display()
        );
    } else {
        std::fs::create_dir_all(&target_dir)?;
        for e in entries {
            let p = e.path();
            if p.is_dir() {
                let dest = target_dir.join(e.file_name());
                std::fs::rename(&p, &dest).context("Move extracted dir")?;
            } else {
                std::fs::rename(&p, target_dir.join(e.file_name()))
                    .context("Move extracted file")?;
            }
        }
    }

    let _ = std::fs::remove_dir_all(&scratch);

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

/// Download `url` to `dest`, retrying up to 3 times on transient failures.
/// Uses exponential backoff: 1 s after attempt 1, 2 s after attempt 2.
fn download_file_with_retry(url: &str, dest: &Path) -> Result<()> {
    const MAX_ATTEMPTS: u32 = 3;
    for attempt in 0..MAX_ATTEMPTS {
        match download_file(url, dest) {
            Ok(()) => return Ok(()),
            Err(e) if attempt + 1 < MAX_ATTEMPTS => {
                log::warn!(
                    "Download attempt {} of {} failed: {}. Retrying…",
                    attempt + 1,
                    MAX_ATTEMPTS,
                    e
                );
                std::thread::sleep(std::time::Duration::from_secs(2u64.pow(attempt)));
            }
            Err(e) => return Err(e),
        }
    }
    unreachable!()
}

fn detect_archive_format_from_magic(archive: &Path) -> Option<&'static str> {
    let mut f = File::open(archive).ok()?;
    let mut buf = [0u8; 6];
    // A single `read` may return a short count even on a local file; fill the whole
    // buffer so the longest signature (xz, 6 bytes) can always be compared.
    let n = read_up_to(&mut f, &mut buf)?;
    let head = &buf[..n];
    let starts_with = |sig: &[u8]| head.starts_with(sig);

    if starts_with(&[0x1f, 0x8b]) {
        return Some("gz");
    }
    // xz: FD '7' 'z' 'X' 'Z' 00
    if starts_with(&[0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00]) {
        return Some("xz");
    }
    if starts_with(b"BZh") {
        return Some("bz2");
    }
    if starts_with(&[0x50, 0x4b, 0x03]) || starts_with(&[0x50, 0x4b, 0x05]) {
        return Some("zip");
    }
    None
}

/// Reads until `buf` is full or EOF, returning the number of bytes read.
fn read_up_to(f: &mut File, buf: &mut [u8]) -> Option<usize> {
    let mut filled = 0;
    while filled < buf.len() {
        match std::io::Read::read(f, &mut buf[filled..]) {
            Ok(0) => break,
            Ok(n) => filled += n,
            Err(_) => return None,
        }
    }
    Some(filled)
}

fn extract_archive(archive: &Path, dest: &Path) -> Result<()> {
    let ext = archive.extension().and_then(|e| e.to_str()).unwrap_or("");
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
        let detected = detect_archive_format_from_magic(archive);
        match detected {
            Some("gz") => extract_tar_gz(archive, dest)?,
            Some("xz") => extract_tar_xz(archive, dest)?,
            Some("bz2") => extract_tar_bz2(archive, dest)?,
            Some("zip") => extract_zip(archive, dest)?,
            Some(_) => return Err(anyhow::anyhow!("Unsupported archive format: {}", path_str)),
            None => return Err(anyhow::anyhow!("Unsupported archive format: {}", path_str)),
        }
    }
    Ok(())
}

fn extract_tar_with<R: Read>(reader: R, dest: &Path) -> Result<()> {
    let mut tar = tar::Archive::new(reader);
    tar.unpack(dest).context("Extract tar")?;
    Ok(())
}

fn extract_tar_gz(archive: &Path, dest: &Path) -> Result<()> {
    let file = File::open(archive).context("Open archive")?;
    let dec = flate2::read::GzDecoder::new(BufReader::new(file));
    extract_tar_with(dec, dest)
}

fn extract_tar_xz(archive: &Path, dest: &Path) -> Result<()> {
    let file = File::open(archive).context("Open archive")?;
    let dec = xz2::read::XzDecoder::new(BufReader::new(file));
    extract_tar_with(dec, dest)
}

fn extract_tar_bz2(archive: &Path, dest: &Path) -> Result<()> {
    let file = File::open(archive).context("Open archive")?;
    let dec = bzip2::read::BzDecoder::new(BufReader::new(file));
    extract_tar_with(dec, dest)
}

fn extract_tar(archive: &Path, dest: &Path) -> Result<()> {
    let file = File::open(archive).context("Open archive")?;
    extract_tar_with(BufReader::new(file), dest)
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
        if !out_path.starts_with(dest) {
            continue;
        }
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
            // Cache hit: still make sure submodules are synced, since packages fetched by an
            // older tsi version (or cloned before this check existed) may not have them.
            // This is a no-op if submodules are already initialized.
            let _ = std::process::Command::new("git")
                .args(["submodule", "update", "--init", "--recursive"])
                .current_dir(&clone_dir)
                .status();
            return Ok(clone_dir);
        }
    }

    let mut cmd = std::process::Command::new("git");
    cmd.arg("clone");
    // Use a shallow clone when no specific commit is pinned — safe for branch/tag fetches
    // and significantly faster for large repositories.
    if pkg.source.commit.is_none() {
        cmd.args(["--depth", "1"]);
    }
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

    // GitHub (and similar) archive tarballs omit submodules; clone-based packages often need them.
    let sub_status = std::process::Command::new("git")
        .args(["submodule", "update", "--init", "--recursive"])
        .current_dir(&clone_dir)
        .status()
        .context("git submodule update")?;
    if !sub_status.success() {
        return Err(anyhow::anyhow!("git submodule update failed"));
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
        // `file_type()` doesn't follow symlinks; a symlink-to-directory needs `metadata()`
        // (which does follow them) to be recognized and recursed into instead of being
        // passed to `fs::copy`, which only supports regular files.
        if ty.is_dir() || (ty.is_symlink() && entry.path().is_dir()) {
            copy_dir_all(&entry.path(), &dst_path)?;
        } else {
            std::fs::copy(entry.path(), dst_path)?;
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Builds a `.tar` of `files` and wraps it with `compress`.
    fn make_tar(files: &[(&str, &[u8])], compress: impl Fn(Vec<u8>) -> Vec<u8>) -> Vec<u8> {
        let mut builder = tar::Builder::new(Vec::new());
        for (name, body) in files {
            let mut header = tar::Header::new_gnu();
            header.set_size(body.len() as u64);
            header.set_mode(0o644);
            header.set_cksum();
            builder.append_data(&mut header, name, *body).unwrap();
        }
        compress(builder.into_inner().unwrap())
    }

    fn gzip(data: Vec<u8>) -> Vec<u8> {
        use std::io::Write;
        let mut enc = flate2::write::GzEncoder::new(Vec::new(), flate2::Compression::fast());
        enc.write_all(&data).unwrap();
        enc.finish().unwrap()
    }

    fn xz(data: Vec<u8>) -> Vec<u8> {
        use std::io::Write;
        let mut enc = xz2::write::XzEncoder::new(Vec::new(), 1);
        enc.write_all(&data).unwrap();
        enc.finish().unwrap()
    }

    fn bzip2(data: Vec<u8>) -> Vec<u8> {
        use std::io::Write;
        let mut enc = bzip2::write::BzEncoder::new(Vec::new(), bzip2::Compression::fast());
        enc.write_all(&data).unwrap();
        enc.finish().unwrap()
    }

    fn write_temp(name: &str, bytes: &[u8]) -> (tempfile::TempDir, std::path::PathBuf) {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join(name);
        std::fs::write(&path, bytes).unwrap();
        (dir, path)
    }

    #[test]
    fn extracts_every_supported_compression() {
        for (name, bytes) in [
            ("src.tar.gz", make_tar(&[("a.txt", b"hi")], gzip)),
            ("src.tgz", make_tar(&[("a.txt", b"hi")], gzip)),
            ("src.tar.xz", make_tar(&[("a.txt", b"hi")], xz)),
            ("src.tar.bz2", make_tar(&[("a.txt", b"hi")], bzip2)),
            ("src.tar", make_tar(&[("a.txt", b"hi")], |d| d)),
        ] {
            let (dir, archive) = write_temp(name, &bytes);
            let dest = dir.path().join("out");
            std::fs::create_dir_all(&dest).unwrap();
            extract_archive(&archive, &dest).unwrap_or_else(|e| panic!("{name}: {e}"));
            assert_eq!(
                std::fs::read_to_string(dest.join("a.txt")).unwrap(),
                "hi",
                "{name}"
            );
        }
    }

    #[test]
    fn falls_back_to_magic_bytes_for_a_misnamed_archive() {
        // Servers hand out extensionless or wrongly-named downloads; the content decides.
        for (name, bytes) in [
            ("download", make_tar(&[("a.txt", b"hi")], gzip)),
            ("download.bin", make_tar(&[("a.txt", b"hi")], xz)),
            ("release?raw=1", make_tar(&[("a.txt", b"hi")], bzip2)),
        ] {
            let (dir, archive) = write_temp(name, &bytes);
            let dest = dir.path().join("out");
            std::fs::create_dir_all(&dest).unwrap();
            extract_archive(&archive, &dest).unwrap_or_else(|e| panic!("{name}: {e}"));
            assert!(dest.join("a.txt").is_file(), "{name}");
        }
    }

    #[test]
    fn unrecognizable_content_is_an_error_not_a_silent_success() {
        let (dir, archive) = write_temp("mystery.bin", b"not an archive at all");
        let dest = dir.path().join("out");
        std::fs::create_dir_all(&dest).unwrap();
        let err = extract_archive(&archive, &dest).unwrap_err();
        assert!(
            err.to_string().contains("Unsupported archive format"),
            "got: {err}"
        );
    }

    // Tar traversal is tar-rs's guarantee (it rejects `..` on both read and write, so
    // such an archive can't even be built here). The zip loop below is our own code.
    #[test]
    fn zip_entries_cannot_escape_the_destination() {
        let mut buf = std::io::Cursor::new(Vec::new());
        {
            use std::io::Write;
            let mut zip = zip::ZipWriter::new(&mut buf);
            let opts: zip::write::FileOptions = Default::default();
            zip.start_file("../escaped.txt", opts).unwrap();
            zip.write_all(b"pwned").unwrap();
            zip.start_file("safe.txt", opts).unwrap();
            zip.write_all(b"ok").unwrap();
            zip.finish().unwrap();
        }
        let (dir, archive) = write_temp("evil.zip", &buf.into_inner());
        let dest = dir.path().join("out");
        std::fs::create_dir_all(&dest).unwrap();

        extract_archive(&archive, &dest).unwrap();
        assert!(
            !dir.path().join("escaped.txt").exists(),
            "zip escaped the destination directory"
        );
        assert_eq!(
            std::fs::read_to_string(dest.join("safe.txt")).unwrap(),
            "ok"
        );
    }

    #[test]
    fn nested_zip_paths_are_created() {
        let mut buf = std::io::Cursor::new(Vec::new());
        {
            use std::io::Write;
            let mut zip = zip::ZipWriter::new(&mut buf);
            let opts: zip::write::FileOptions = Default::default();
            zip.start_file("pkg-1.0/src/main.c", opts).unwrap();
            zip.write_all(b"int main(){}").unwrap();
            zip.finish().unwrap();
        }
        let (dir, archive) = write_temp("pkg.zip", &buf.into_inner());
        let dest = dir.path().join("out");
        std::fs::create_dir_all(&dest).unwrap();

        extract_archive(&archive, &dest).unwrap();
        assert!(dest.join("pkg-1.0/src/main.c").is_file());
    }

    #[test]
    fn magic_detection_recognizes_each_format() {
        for (bytes, expected) in [
            (make_tar(&[("a", b"x")], gzip), "gz"),
            (make_tar(&[("a", b"x")], xz), "xz"),
            (make_tar(&[("a", b"x")], bzip2), "bz2"),
        ] {
            let (_d, path) = write_temp("blob", &bytes);
            assert_eq!(detect_archive_format_from_magic(&path), Some(expected));
        }

        let (_d, path) = write_temp("blob", b"plain text file");
        assert_eq!(detect_archive_format_from_magic(&path), None);

        // Shorter than any magic signature: must not panic on the slice.
        let (_d, path) = write_temp("blob", b"\x1f");
        assert_eq!(detect_archive_format_from_magic(&path), None);
    }

    #[test]
    fn local_source_must_exist() {
        let pkg: crate::core::package::Package = {
            let json = r#"{"name":"p","version":"1.0",
                "source":{"type":"local","path":"/definitely/not/here"}}"#;
            crate::core::package::parse_package_file(json).unwrap()[0].clone()
        };
        let dir = tempfile::tempdir().unwrap();
        let err = fetch(&pkg, dir.path(), false).unwrap_err();
        assert!(err.to_string().contains("does not exist"), "got: {err}");
    }

    #[test]
    fn unknown_source_type_is_rejected() {
        let json = r#"{"name":"p","version":"1.0","source":{"type":"carrier-pigeon"}}"#;
        let pkg = crate::core::package::parse_package_file(json).unwrap()[0].clone();
        let dir = tempfile::tempdir().unwrap();
        let err = fetch(&pkg, dir.path(), false).unwrap_err();
        assert!(
            err.to_string().contains("Unknown source type"),
            "got: {err}"
        );
    }

    #[test]
    fn copy_dir_all_copies_nested_trees() {
        let dir = tempfile::tempdir().unwrap();
        let src = dir.path().join("src");
        std::fs::create_dir_all(src.join("a/b")).unwrap();
        std::fs::write(src.join("top.txt"), "top").unwrap();
        std::fs::write(src.join("a/b/deep.txt"), "deep").unwrap();

        let dst = dir.path().join("dst");
        copy_dir_all(&src, &dst).unwrap();
        assert_eq!(std::fs::read_to_string(dst.join("top.txt")).unwrap(), "top");
        assert_eq!(
            std::fs::read_to_string(dst.join("a/b/deep.txt")).unwrap(),
            "deep"
        );
    }
}
