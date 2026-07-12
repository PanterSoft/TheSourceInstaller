use crate::core::package::Package;
use crate::ui;
use anyhow::{Context, Result};
use console;
use std::io::{self, BufRead, BufReader, Read, Write};
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::mpsc;
use std::thread;
use std::time::Duration;

pub fn build(
    pkg: &Package,
    source_dir: &Path,
    build_dir: &Path,
    install_dir: &Path,
    prefix_install: &Path,
    isolated: bool,
    verbose: bool,
) -> Result<()> {
    std::fs::create_dir_all(build_dir).context("Create build dir")?;
    std::fs::create_dir_all(install_dir).context("Create install dir")?;

    let env = build_env_with_package(prefix_install, pkg, isolated);
    let env_ref: &[(String, String)] = &env;

    for patch in &pkg.patches {
        let patch_path = resolve_patch_path(patch, prefix_install);
        if !patch_path.exists() {
            anyhow::bail!(
                "Patch file not found: {} (resolved to {}). \
                 Place it under <prefix>/patches/ or use an absolute path.",
                patch,
                patch_path.display()
            );
        }
        let patch_str = patch_path.to_string_lossy().to_string();
        let step_name = format!("patch -p1 -i {}", patch_str);
        let mut cmd = Command::new("patch");
        cmd.args(["-p1", "-i", &patch_str]).current_dir(source_dir);
        run_cmd(&mut cmd, env_ref, &step_name, verbose)?;
    }

    let install_path = install_dir.to_string_lossy();

    match pkg.build_system.as_str() {
        "autotools" => build_autotools(
            pkg,
            source_dir,
            build_dir,
            install_dir,
            prefix_install,
            &env,
            verbose,
        )?,
        "cmake" => build_cmake(
            pkg,
            source_dir,
            build_dir,
            install_dir,
            prefix_install,
            &env,
            verbose,
        )?,
        "meson" => build_meson(
            pkg,
            source_dir,
            build_dir,
            install_dir,
            prefix_install,
            &env,
            verbose,
        )?,
        "make" => build_make(pkg, source_dir, build_dir, install_dir, &env, verbose)?,
        "custom" => build_custom(pkg, source_dir, &install_path, &env, verbose)?,
        _ => anyhow::bail!("Unknown build system: {}", pkg.build_system),
    }
    Ok(())
}

fn expand_build_vars(value: &str, install_prefix: &str) -> String {
    let mut s = value.replace("$TSI_INSTALL_DIR", install_prefix);
    #[cfg(target_os = "macos")]
    {
        if s.contains("$MACOSX_SDK") {
            if let Some(sdk) = macosx_sdk_path() {
                s = s.replace("$MACOSX_SDK", &sdk);
            }
            if s.contains("$MACOSX_SDK") {
                if let Some(sdk) = macosx_sdk_path_fallback() {
                    s = s.replace("$MACOSX_SDK", &sdk);
                }
            }
        }
    }
    s
}

/// Path from `xcrun --sdk macosx --show-sdk-path` (Apple headers, e.g. uuid/uuid.h with uuid_string_t).
#[cfg(target_os = "macos")]
fn macosx_sdk_path() -> Option<String> {
    for bin in ["/usr/bin/xcrun", "xcrun"] {
        let Ok(out) = std::process::Command::new(bin)
            .args(["--sdk", "macosx", "--show-sdk-path"])
            .output()
        else {
            continue;
        };
        if !out.status.success() {
            continue;
        }
        let Ok(path) = String::from_utf8(out.stdout) else {
            continue;
        };
        let path = path.trim().to_string();
        if !path.is_empty() {
            return Some(path);
        }
    }
    None
}

#[cfg(target_os = "macos")]
fn macosx_sdk_path_fallback() -> Option<String> {
    if let Ok(root) = std::env::var("SDKROOT") {
        let p = Path::new(&root);
        if p.join("usr/include/uuid/uuid.h").is_file() {
            return Some(root);
        }
    }
    let candidates = [
        "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk",
        "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk",
    ];
    for c in candidates {
        let p = Path::new(c);
        if p.join("usr/include/uuid/uuid.h").is_file() {
            return Some(c.to_string());
        }
    }
    None
}

fn resolve_patch_path(patch: &str, prefix_install: &Path) -> PathBuf {
    let p = Path::new(patch);
    if p.is_absolute() {
        return p.to_path_buf();
    }
    if p.exists() {
        return p.to_path_buf();
    }
    if let Some(prefix_root) = prefix_install.parent() {
        let under_prefix = prefix_root.join("patches").join(p);
        if under_prefix.exists() {
            return under_prefix;
        }
    }
    p.to_path_buf()
}

fn build_env_with_package(prefix: &Path, pkg: &Package, isolated: bool) -> Vec<(String, String)> {
    let install_prefix = prefix.to_string_lossy().to_string();
    let mut env = build_env_base(prefix, isolated);
    for (k, v) in &pkg.env {
        env.push((k.clone(), expand_build_vars(v, &install_prefix)));
    }
    env
}

fn build_env_base(prefix: &Path, isolated: bool) -> Vec<(String, String)> {
    let bin = prefix.join("bin");
    let lib = prefix.join("lib");
    let include = prefix.join("include");
    let pkgconfig = lib.join("pkgconfig");

    let path_sep = if cfg!(windows) { ";" } else { ":" };

    let base_path = if isolated {
        if cfg!(windows) {
            String::new()
        } else {
            "/bin".to_string()
        }
    } else {
        std::env::var("PATH").unwrap_or_default()
    };

    let new_path = if base_path.is_empty() {
        bin.to_string_lossy().to_string()
    } else {
        format!("{}{}{}", bin.display(), path_sep, base_path)
    };

    let mut env = vec![
        ("PATH".to_string(), new_path),
        (
            "PKG_CONFIG_PATH".to_string(),
            pkgconfig.to_string_lossy().to_string(),
        ),
        (
            "CMAKE_PREFIX_PATH".to_string(),
            prefix.to_string_lossy().to_string(),
        ),
        ("CPPFLAGS".to_string(), format!("-I{}", include.display())),
        ("LDFLAGS".to_string(), format!("-L{}", lib.display())),
    ];

    // macOS uses DYLD_LIBRARY_PATH; LD_LIBRARY_PATH is ignored by the Darwin dynamic linker.
    #[cfg(target_os = "macos")]
    env.push((
        "DYLD_LIBRARY_PATH".to_string(),
        lib.to_string_lossy().to_string(),
    ));
    #[cfg(not(target_os = "macos"))]
    env.push((
        "LD_LIBRARY_PATH".to_string(),
        lib.to_string_lossy().to_string(),
    ));

    // Some Python-based build tools (e.g. meson installed via `setup.py install`)
    // end up as a single `.egg` without standalone `dist-info`/`egg-info` directories.
    // The generated wrapper uses `importlib.metadata`, which requires that the *egg file*
    // itself is present on `sys.path` (via PYTHONPATH), not just the containing directory.
    let mut py_paths: Vec<String> = Vec::new();
    if let Ok(prefix_entries) = std::fs::read_dir(prefix) {
        for entry in prefix_entries.flatten() {
            let file_name = entry.file_name();
            let file_name = file_name.to_string_lossy();
            if !file_name.starts_with("meson-") {
                continue;
            }
            let meson_dir = entry.path();
            let lib_dir = meson_dir.join("lib");
            if !lib_dir.is_dir() {
                continue;
            }
            if let Ok(py_entries) = std::fs::read_dir(&lib_dir) {
                for py_entry in py_entries.flatten() {
                    let py_name = py_entry.file_name().to_string_lossy().to_string();
                    if !py_name.starts_with("python") {
                        continue;
                    }
                    let site_packages_dir = py_entry.path().join("site-packages");
                    if !site_packages_dir.is_dir() {
                        continue;
                    }
                    if let Ok(site_entries) = std::fs::read_dir(&site_packages_dir) {
                        for site_entry in site_entries.flatten() {
                            let p = site_entry.path();
                            if p.extension().and_then(|e| e.to_str()) == Some("egg") {
                                py_paths.push(p.to_string_lossy().to_string());
                            }
                        }
                    }
                }
            }
        }
    }
    if !py_paths.is_empty() {
        let existing = std::env::var("PYTHONPATH").unwrap_or_default();
        let new_py_path = if existing.is_empty() {
            py_paths.join(path_sep)
        } else {
            format!("{}{}{}", py_paths.join(path_sep), path_sep, existing)
        };
        env.push(("PYTHONPATH".to_string(), new_py_path));
    }

    // Isolated builds prepend GNU coreutils (including `ar`/`ranlib`) before `/usr/bin`.
    // On Darwin, GNU ar archives often break Apple `ld` (e.g. "archive member '/' not a mach-o file").
    // Use `xcrun --find` to locate the Xcode-blessed Apple ar/ranlib regardless of Xcode install path.
    if crate::platform::os_name() == "darwin" {
        env.push(("AR".to_string(), xcrun_find("ar")));
        env.push(("RANLIB".to_string(), xcrun_find("ranlib")));
    }
    env
}

/// Locate an Apple toolchain tool via `xcrun --find`, falling back to `/usr/bin/<tool>`.
/// Only compiled on macOS.
#[cfg(target_os = "macos")]
fn xcrun_find(tool: &str) -> String {
    std::process::Command::new("xcrun")
        .args(["--find", tool])
        .output()
        .ok()
        .filter(|o| o.status.success())
        .and_then(|o| String::from_utf8(o.stdout).ok())
        .map(|s| s.trim().to_string())
        .filter(|p| !p.is_empty())
        .unwrap_or_else(|| format!("/usr/bin/{}", tool))
}

/// On non-macOS platforms this path is never reached (the darwin guard above prevents it),
/// but we need a stub so the call site compiles on all targets.
#[cfg(not(target_os = "macos"))]
fn xcrun_find(tool: &str) -> String {
    format!("/usr/bin/{}", tool)
}

fn run_cmd(
    cmd: &mut Command,
    env: &[(String, String)],
    step_name: &str,
    verbose: bool,
) -> Result<()> {
    for (k, v) in env {
        cmd.env(k, v);
    }
    if verbose {
        ui::output::build_step(step_name);
        let status = cmd.status().context("Execute command")?;
        if !status.success() {
            anyhow::bail!("Command failed with exit code: {:?}", status.code());
        }
        return Ok(());
    }
    ui::output::build_step(step_name);
    cmd.stdout(Stdio::piped()).stderr(Stdio::piped());
    let mut child = cmd.spawn().context("Execute command")?;

    let is_tty = console::Term::stderr().features().is_attended();
    let (stderr_tx, stderr_rx) = mpsc::channel::<Option<String>>();

    let stdout_handle = child.stdout.take().map(|stdout| {
        thread::spawn(move || {
            let mut buf = Vec::new();
            let _ = BufReader::new(stdout).read_to_end(&mut buf);
            buf
        })
    });

    let stderr_handle = child.stderr.take().map(|stderr| {
        thread::spawn(move || {
            let reader = BufReader::new(stderr);
            for line in reader.lines() {
                if stderr_tx.send(Some(line.unwrap_or_default())).is_err() {
                    break;
                }
            }
            let _ = stderr_tx.send(None);
        })
    });

    let spinner_msg = format!("Running {}...", step_name);
    let spinner = if is_tty {
        Some(ui::progress::create_spinner(&spinner_msg))
    } else {
        None
    };

    let mut stderr_lines: Vec<String> = Vec::new();

    loop {
        match stderr_rx.recv_timeout(Duration::from_millis(100)) {
            Ok(Some(line)) => {
                let _ = writeln!(io::stderr(), "{}", line);
                stderr_lines.push(line);
            }
            Ok(None) => break,
            Err(mpsc::RecvTimeoutError::Timeout) => {
                if let Some(ref pb) = spinner {
                    pb.tick();
                }
            }
            Err(mpsc::RecvTimeoutError::Disconnected) => break,
        }
    }

    if let Some(ref pb) = spinner {
        pb.finish_and_clear();
    }

    let status = child.wait().context("Wait for command")?;
    let stdout_buf = stdout_handle
        .and_then(|h| h.join().ok())
        .unwrap_or_default();
    let _ = stderr_handle.and_then(|h| h.join().ok());

    if !status.success() {
        let mut h = io::stderr().lock();
        let _ = h.write_all(&stdout_buf);
        for line in &stderr_lines {
            let _ = writeln!(h, "{}", line);
        }
        let _ = h.flush();
        anyhow::bail!("Command failed with exit code: {:?}", status.code());
    }
    Ok(())
}

fn build_autotools(
    pkg: &Package,
    source_dir: &Path,
    _build_dir: &Path,
    install_dir: &Path,
    deps_prefix: &Path,
    env: &[(String, String)],
    verbose: bool,
) -> Result<()> {
    let env = augment_env_for_autotools(env, source_dir, deps_prefix)?;
    let env_ref: &[(String, String)] = &env;

    let prefix = install_dir.to_string_lossy();
    let deps_prefix_str = deps_prefix.to_string_lossy();
    let mut configure_args = vec!["--prefix".to_string(), prefix.to_string()];
    configure_args.extend(
        pkg.configure_args
            .iter()
            .map(|a| expand_build_vars(a, deps_prefix_str.as_ref())),
    );

    run_cmd(
        Command::new("./configure")
            .args(&configure_args)
            .current_dir(source_dir)
            .env("TSI_INSTALL_DIR", prefix.as_ref()),
        env_ref,
        "./configure",
        verbose,
    )?;

    run_cmd(
        Command::new("make").current_dir(source_dir),
        env_ref,
        "make",
        verbose,
    )?;
    run_cmd(
        Command::new("make")
            .args(["install"])
            .current_dir(source_dir),
        env_ref,
        "make install",
        verbose,
    )?;
    Ok(())
}

/// TSI's default `CPPFLAGS=-I$prefix/include` must not win over system/SDK headers (e.g. git's
/// `archive.h`, prefix `uuid/uuid.h` vs Apple `uuid_string_t` for Cocoa). On macOS, use `-isystem`
/// for the SDK and `-idirafter` for the prefix. Meson/CMake/compiler checks use the same `CPPFLAGS`.
fn augment_env_cppflags(
    env: &[(String, String)],
    source_dir: &Path,
    deps_prefix: &Path,
) -> Result<Vec<(String, String)>> {
    let mut out = Vec::with_capacity(env.len());
    for (k, v) in env {
        if k == "CPPFLAGS" {
            out.push((
                k.clone(),
                augment_cppflags_for_tsi_prefix(v, source_dir, deps_prefix)?,
            ));
        } else {
            out.push((k.clone(), v.clone()));
        }
    }
    Ok(out)
}

fn augment_env_for_autotools(
    env: &[(String, String)],
    source_dir: &Path,
    deps_prefix: &Path,
) -> Result<Vec<(String, String)>> {
    augment_env_cppflags(env, source_dir, deps_prefix)
}

fn augment_cppflags_for_tsi_prefix(
    cppflags: &str,
    source_dir: &Path,
    deps_prefix: &Path,
) -> Result<String> {
    let inc = deps_prefix.join("include");
    let inc_flag = format!("-I{}", inc.display());
    let src = format!("-I{}", source_dir.display());
    let mut rest = cppflags.trim().to_string();
    if rest.contains(&inc_flag) {
        rest = rest.replace(&inc_flag, "");
        rest = rest.split_whitespace().collect::<Vec<_>>().join(" ");
    }
    #[cfg(target_os = "macos")]
    {
        let sdk = macosx_sdk_path()
            .or_else(macosx_sdk_path_fallback)
            .ok_or_else(|| {
                anyhow::anyhow!(
                    "macOS SDK not found (xcrun --sdk macosx --show-sdk-path). Install Xcode or Command Line Tools."
                )
            })?;
        let sys = format!("-isystem {}/usr/include", sdk);
        let idir = format!("-idirafter {}", inc.display());
        let joined = format!("{} {} {} {}", src, sys, rest, idir);
        Ok(joined.split_whitespace().collect::<Vec<_>>().join(" "))
    }
    #[cfg(not(target_os = "macos"))]
    {
        if rest.is_empty() {
            Ok(src)
        } else {
            Ok(format!("{} {}", src, rest))
        }
    }
}

fn build_cmake(
    pkg: &Package,
    source_dir: &Path,
    build_dir: &Path,
    install_dir: &Path,
    deps_prefix: &Path,
    env: &[(String, String)],
    verbose: bool,
) -> Result<()> {
    let env = augment_env_cppflags(env, source_dir, deps_prefix)?;
    let env_ref: &[(String, String)] = &env;
    let prefix = install_dir.to_string_lossy();
    let deps_prefix_str = deps_prefix.to_string_lossy();
    let mut cmake_args = vec![
        "-DCMAKE_INSTALL_PREFIX=".to_string() + &prefix,
        // CMake 4.x rejects projects that only declare an ancient minimum; allow configuring.
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5".to_string(),
        "-S".to_string(),
        source_dir.to_string_lossy().to_string(),
        "-B".to_string(),
        build_dir.to_string_lossy().to_string(),
    ];
    cmake_args.extend(
        pkg.cmake_args
            .iter()
            .map(|a| expand_build_vars(a, deps_prefix_str.as_ref())),
    );

    run_cmd(
        Command::new("cmake").args(&cmake_args),
        env_ref,
        "cmake",
        verbose,
    )?;
    run_cmd(
        Command::new("cmake").args(["--build", build_dir.to_string_lossy().as_ref()]),
        env_ref,
        "cmake --build",
        verbose,
    )?;
    run_cmd(
        Command::new("cmake").args(["--install", build_dir.to_string_lossy().as_ref()]),
        env_ref,
        "cmake --install",
        verbose,
    )?;
    Ok(())
}

fn build_meson(
    pkg: &Package,
    source_dir: &Path,
    build_dir: &Path,
    install_dir: &Path,
    deps_prefix: &Path,
    env: &[(String, String)],
    verbose: bool,
) -> Result<()> {
    let env = augment_env_cppflags(env, source_dir, deps_prefix)?;
    let env_ref: &[(String, String)] = &env;
    let prefix = install_dir.to_string_lossy();
    let deps_prefix_str = deps_prefix.to_string_lossy();
    let mut setup_args = vec![
        "setup".to_string(),
        build_dir.to_string_lossy().into_owned(),
        source_dir.to_string_lossy().into_owned(),
        "--prefix".to_string(),
        prefix.into_owned(),
    ];
    setup_args.extend(
        pkg.configure_args
            .iter()
            .map(|a| expand_build_vars(a, deps_prefix_str.as_ref())),
    );
    run_cmd(
        Command::new("meson").args(&setup_args),
        env_ref,
        "meson setup",
        verbose,
    )?;
    run_cmd(
        Command::new("meson").args(["compile", "-C", build_dir.to_string_lossy().as_ref()]),
        env_ref,
        "meson compile",
        verbose,
    )?;
    run_cmd(
        Command::new("meson").args(["install", "-C", build_dir.to_string_lossy().as_ref()]),
        env_ref,
        "meson install",
        verbose,
    )?;
    Ok(())
}

fn build_make(
    pkg: &Package,
    source_dir: &Path,
    _build_dir: &Path,
    install_dir: &Path,
    env: &[(String, String)],
    verbose: bool,
) -> Result<()> {
    let prefix = install_dir.to_string_lossy();
    let mut make_args = pkg.make_args.clone();
    make_args.push(format!("PREFIX={}", prefix));

    // Promote AR and RANLIB from the build environment into make command-line arguments.
    // Make's file-level variable assignments (e.g. `AR=ar` in bzip2's Makefile) take
    // precedence over environment variables, but command-line arguments always win.
    for var in ["AR", "RANLIB", "CC", "CXX"] {
        if let Some((_, val)) = env.iter().find(|(k, _)| k == var) {
            if !make_args
                .iter()
                .any(|a| a.starts_with(&format!("{}=", var)))
            {
                make_args.push(format!("{}={}", var, val));
            }
        }
    }

    run_cmd(
        Command::new("make")
            .args(&make_args)
            .current_dir(source_dir),
        env,
        "make",
        verbose,
    )?;
    run_cmd(
        Command::new("make")
            .args(["install"])
            .args(&make_args)
            .current_dir(source_dir),
        env,
        "make install",
        verbose,
    )?;
    Ok(())
}

fn build_custom(
    pkg: &Package,
    source_dir: &Path,
    install_path: &str,
    env: &[(String, String)],
    verbose: bool,
) -> Result<()> {
    let shell = if cfg!(windows) { "cmd" } else { "sh" };
    let flag = if cfg!(windows) { "/C" } else { "-c" };
    for cmd_str in &pkg.build_commands {
        let expanded = expand_build_vars(cmd_str, install_path);
        let step_name = if expanded.len() <= 60 {
            expanded.clone()
        } else {
            format!("sh -c \"{}...\"", &expanded[..50.min(expanded.len())])
        };
        let mut cmd = Command::new(shell);
        cmd.arg(flag)
            .arg(&expanded)
            .current_dir(source_dir)
            .env("TSI_INSTALL_DIR", install_path);
        run_cmd(&mut cmd, env, &step_name, verbose)?;
    }
    Ok(())
}
