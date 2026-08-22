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
        let patch_path = resolve_patch_path(patch, source_dir, prefix_install);
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

    check_build_system_present(pkg, source_dir)?;

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
        "make" => build_make(
            pkg,
            source_dir,
            build_dir,
            install_dir,
            prefix_install,
            &env,
            verbose,
        )?,
        "custom" => build_custom(
            pkg,
            source_dir,
            prefix_install,
            &install_path,
            &env,
            verbose,
        )?,
        // Handled before any of this in ops::install; reaching build() means a
        // caller bypassed that, so say so rather than silently doing nothing.
        "meta" => anyhow::bail!("Metapackage {} has nothing to build", pkg.name),
        _ => anyhow::bail!("Unknown build system: {}", pkg.build_system),
    }
    Ok(())
}

/// The file each build system needs to find in the source tree, and what a
/// project that ships it instead usually means.
const BUILD_SYSTEM_MARKERS: &[(&str, &[&str])] = &[
    ("cmake", &["CMakeLists.txt"]),
    ("meson", &["meson.build"]),
    ("autotools", &["configure"]),
    ("make", &["Makefile", "makefile", "GNUmakefile"]),
];

/// Files that identify a build system a package did *not* declare, so the error
/// can name the likely correct one instead of only saying what is missing.
const FOREIGN_MARKERS: &[(&str, &str)] = &[
    ("CMakeLists.txt", "cmake"),
    ("meson.build", "meson"),
    ("configure", "autotools"),
    ("configure.ac", "autotools (run its autogen.sh first)"),
    ("SConstruct", "SCons, which TSI has no build system for"),
    ("BUILD.bazel", "Bazel, which TSI has no build system for"),
    (
        "setup.py",
        "Python packaging, best driven from build_commands",
    ),
];

/// Fails early when the declared build system is not actually in the source tree.
///
/// Without this the build system's own error surfaces instead, which describes
/// a missing file rather than a wrong package definition -- mongodb declared
/// `cmake` while shipping only a `SConstruct`, and said so as a CMake usage
/// error a hundred lines into the output.
fn check_build_system_present(pkg: &Package, source_dir: &Path) -> Result<()> {
    let Some((_, markers)) = BUILD_SYSTEM_MARKERS
        .iter()
        .find(|(name, _)| *name == pkg.build_system)
    else {
        // "custom" (and anything unknown, which the dispatch below rejects)
        // has no marker to look for.
        return Ok(());
    };

    if markers.iter().any(|m| source_dir.join(m).exists()) {
        return Ok(());
    }

    let found: Vec<String> = FOREIGN_MARKERS
        .iter()
        .filter(|(file, _)| source_dir.join(file).exists())
        .map(|(file, system)| format!("{} ({})", file, system))
        .collect();

    let hint = if found.is_empty() {
        String::new()
    } else {
        format!(" The source tree does contain: {}.", found.join(", "))
    };

    anyhow::bail!(
        "{} declares build_system \"{}\", but none of {:?} exists in {}.{}",
        pkg.name,
        pkg.build_system,
        markers,
        source_dir.display(),
        hint
    );
}

fn expand_build_vars(value: &str, install_prefix: &str) -> String {
    // `mut` is only exercised inside the macOS cfg block below.
    #[cfg_attr(not(target_os = "macos"), allow(unused_mut))]
    let mut s = value.replace("$TSI_INSTALL_DIR", install_prefix);
    #[cfg(target_os = "macos")]
    {
        if s.contains("$MACOSX_SDK") {
            if let Some(sdk) = cached_macosx_sdk_path() {
                s = s.replace("$MACOSX_SDK", &sdk);
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

/// `macosx_sdk_path()`/`macosx_sdk_path_fallback()` shell out / probe the filesystem; memoize
/// the result so repeated lookups (once per `$MACOSX_SDK` expansion, plus the SDKROOT pin) only
/// pay that cost once per process.
#[cfg(target_os = "macos")]
static MACOSX_SDK_PATH: std::sync::OnceLock<Option<String>> = std::sync::OnceLock::new();

#[cfg(target_os = "macos")]
fn cached_macosx_sdk_path() -> Option<String> {
    MACOSX_SDK_PATH
        .get_or_init(|| macosx_sdk_path().or_else(macosx_sdk_path_fallback))
        .clone()
}

fn resolve_patch_path(patch: &str, source_dir: &Path, prefix_install: &Path) -> PathBuf {
    let p = Path::new(patch);
    if p.is_absolute() {
        return p.to_path_buf();
    }
    let under_source = source_dir.join(p);
    if under_source.exists() {
        return under_source;
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
        ("CPPFLAGS".to_string(), default_cppflags(&include)),
        ("LDFLAGS".to_string(), default_ldflags(&lib)),
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

    // Pin the SDK explicitly so header/lib lookup is deterministic across shells; clang's
    // default search then orders libc++ headers before SDK C headers (see augment_cppflags).
    #[cfg(target_os = "macos")]
    if std::env::var_os("SDKROOT").is_none() {
        if let Some(sdk) = cached_macosx_sdk_path() {
            env.push(("SDKROOT".to_string(), sdk));
        }
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

/// Overrides passed to `make` for an autotools package.
///
/// An autotools package gets its `--prefix` from configure, so `make_args` here
/// are never about where to install -- they are overrides for variables the
/// Makefile assigns itself, which command-line arguments beat and nothing else
/// does. readline needs one: configure leaves `SHLIB_LIBS` empty, so its shared
/// library links no termcap library and every dependent dies on an undefined
/// `UP`.
///
/// `$TSI_INSTALL_DIR` expands to the shared prefix here, matching
/// `configure_args` in the same build system -- an autotools package never has
/// to name its own install directory, because configure was already told it.
fn autotools_make_args(pkg: &Package, deps_prefix: &str) -> Vec<String> {
    pkg.make_args
        .iter()
        .map(|a| expand_build_vars(a, deps_prefix))
        .collect()
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
    let env_ref: &[(String, String)] = env;

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

    let make_args = autotools_make_args(pkg, deps_prefix_str.as_ref());

    let jobs = build_jobs().to_string();
    run_cmd(
        Command::new("make")
            .arg(format!("-j{}", jobs))
            .args(&make_args)
            .current_dir(source_dir),
        env_ref,
        &format!("make -j{}", jobs),
        verbose,
    )?;
    run_cmd(
        Command::new("make")
            .args(["install"])
            .args(&make_args)
            .current_dir(source_dir),
        env_ref,
        "make install",
        verbose,
    )?;
    Ok(())
}

/// How many build jobs to run at once.
///
/// `TSI_JOBS` overrides it; `TSI_JOBS=1` forces a serial build, which is the
/// first thing to try when a parallel build fails or its output is interleaved
/// beyond reading. Anything unparseable or zero falls back to the CPU count.
fn build_jobs() -> usize {
    if let Ok(v) = std::env::var("TSI_JOBS") {
        if let Ok(n) = v.trim().parse::<usize>() {
            if n > 0 {
                return n;
            }
        }
        log::warn!("Ignoring invalid TSI_JOBS={:?}", v);
    }
    std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(1)
}

/// The default `LDFLAGS` TSI hands every build.
///
/// `-L` alone only resolves libraries at *link* time. TSI installs to a prefix
/// no dynamic loader searches, so without an RPATH the result links cleanly and
/// then fails to load: cjson's libcjson_utils could not find libcjson, its own
/// sibling in the same package. Recording the prefix's shared lib directory
/// makes what TSI builds runnable without LD_LIBRARY_PATH.
fn default_ldflags(lib: &Path) -> String {
    if cfg!(windows) {
        // No RPATH concept; the loader uses PATH and the binary's directory.
        format!("-L{}", lib.display())
    } else {
        format!("-L{} -Wl,-rpath,{}", lib.display(), lib.display())
    }
}

/// The default `CPPFLAGS` TSI hands every build.
///
/// On macOS the prefix is passed with `-idirafter`, not `-I`: prefix headers
/// must not shadow SDK ones (git's `archive.h`, prefix `uuid/uuid.h` vs Apple's
/// `uuid_string_t` for Cocoa). Injecting `-isystem $SDK/usr/include` instead
/// breaks C++ builds, because it puts C headers ahead of libc++'s wrappers
/// ("<cstdlib> tried including <stdlib.h> but didn't find libc++'s <stdlib.h>").
///
/// This is only TSI's *default*. A package that sets `CPPFLAGS` itself replaces
/// it outright and is taken at its word, which it previously could not be:
/// every CPPFLAGS value was rewritten to `-idirafter` on the way to the
/// compiler, including the package's own. That mattered because `-idirafter`
/// is unopposable -- given both `-idirafter DIR` and `-I DIR`, clang keeps the
/// lowest-priority position for DIR *whichever order they appear in*, so a
/// package asking for prefix headers first could never get them. PostgreSQL's
/// psql compiled against Apple's libedit `<readline/readline.h>` while linking
/// the prefix's GNU readline, and failed on undeclared `append_history`.
fn default_cppflags(include: &Path) -> String {
    if cfg!(target_os = "macos") {
        format!("-idirafter {}", include.display())
    } else {
        format!("-I{}", include.display())
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
    let mut env = env.to_vec();
    // CMake ignores CPPFLAGS; mirror it into CFLAGS/CXXFLAGS so prefix headers are found
    // (leveldb -> snappy.h, grpc -> openssl headers).
    let cpp = env
        .iter()
        .find(|(k, _)| k == "CPPFLAGS")
        .map(|(_, v)| v.clone())
        .unwrap_or_default();
    if !cpp.is_empty() {
        for key in ["CFLAGS", "CXXFLAGS"] {
            if let Some(e) = env.iter_mut().find(|(k, _)| k == key) {
                e.1 = format!("{} {}", e.1, cpp);
            } else {
                env.push((key.to_string(), cpp.clone()));
            }
        }
    }
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
        Command::new("cmake").args([
            "--build",
            build_dir.to_string_lossy().as_ref(),
            "--parallel",
            &build_jobs().to_string(),
        ]),
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

/// The `meson setup` arguments TSI always supplies.
///
/// `--libdir lib` matters. Left to itself meson picks the host's convention --
/// on Debian and Ubuntu that is the multiarch `lib/x86_64-linux-gnu` -- and
/// TSI's prefix has exactly one lib directory, which is what PKG_CONFIG_PATH,
/// the `-L` and the recorded rpath all name. pixman installed its pixman-1.pc
/// into the multiarch path, so cairo could not find pixman at all and tried to
/// download it as a subproject instead.
///
/// A package's own configure_args are appended after these, so it can still
/// override any of them.
fn meson_setup_args(build_dir: &Path, source_dir: &Path, prefix: &str) -> Vec<String> {
    vec![
        "setup".to_string(),
        build_dir.to_string_lossy().into_owned(),
        source_dir.to_string_lossy().into_owned(),
        "--prefix".to_string(),
        prefix.to_string(),
        "--libdir".to_string(),
        "lib".to_string(),
    ]
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
    let env_ref: &[(String, String)] = env;
    let prefix = install_dir.to_string_lossy();
    let deps_prefix_str = deps_prefix.to_string_lossy();
    let mut setup_args = meson_setup_args(build_dir, source_dir, prefix.as_ref());
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

/// `make` command-line arguments for a package: its own `make_args` with build
/// variables expanded, plus TSI's own `PREFIX=`.
///
/// The expansion is not optional. Passed through raw, `make` treats the leading
/// `$T` of `$TSI_INSTALL_DIR` as an (empty) make variable and the argument
/// silently becomes `prefix=SI_INSTALL_DIR` -- which is how libcap's install
/// step ended up writing man pages to a relative junk path.
fn make_args_with_prefix(pkg: &Package, prefix: &str) -> Vec<String> {
    let mut args: Vec<String> = pkg
        .make_args
        .iter()
        .map(|a| expand_build_vars(a, prefix))
        .collect();
    args.push(format!("PREFIX={}", prefix));
    args
}

fn build_make(
    pkg: &Package,
    source_dir: &Path,
    _build_dir: &Path,
    install_dir: &Path,
    _deps_prefix: &Path,
    env: &[(String, String)],
    verbose: bool,
) -> Result<()> {
    let env_ref: &[(String, String)] = env;

    let prefix = install_dir.to_string_lossy();
    let mut make_args = make_args_with_prefix(pkg, prefix.as_ref());

    // Promote AR and RANLIB from the build environment into make command-line arguments.
    // Make's file-level variable assignments (e.g. `AR=ar` in bzip2's Makefile) take
    // precedence over environment variables, but command-line arguments always win.
    for var in ["AR", "RANLIB", "CC", "CXX"] {
        if let Some((_, val)) = env_ref.iter().find(|(k, _)| k == var) {
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
            .current_dir(source_dir)
            .env("TSI_INSTALL_DIR", prefix.as_ref()),
        env_ref,
        "make",
        verbose,
    )?;
    run_cmd(
        Command::new("make")
            .args(["install"])
            .args(&make_args)
            .current_dir(source_dir)
            .env("TSI_INSTALL_DIR", prefix.as_ref()),
        env_ref,
        "make install",
        verbose,
    )?;
    Ok(())
}

fn build_custom(
    pkg: &Package,
    source_dir: &Path,
    _deps_prefix: &Path,
    install_path: &str,
    env: &[(String, String)],
    verbose: bool,
) -> Result<()> {
    let env_ref: &[(String, String)] = env;

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
        run_cmd(&mut cmd, env_ref, &step_name, verbose)?;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_ldflags_record_an_rpath_off_windows() {
        let flags = default_ldflags(Path::new("/p/lib"));
        assert!(flags.contains("-L/p/lib"), "{flags}");
        if cfg!(windows) {
            assert!(!flags.contains("rpath"), "{flags}");
        } else {
            // Without this the build links but the result cannot load: TSI
            // installs to a prefix no dynamic loader searches by default.
            assert!(flags.contains("-Wl,-rpath,/p/lib"), "{flags}");
        }
    }

    #[test]
    fn a_package_ldflags_replaces_the_default_including_the_rpath() {
        let json = r#"{
            "name": "p",
            "version": "1",
            "source": { "type": "tarball", "url": "https://example.com/x.tar.gz" },
            "build_system": "make",
            "env": { "LDFLAGS": "-L$TSI_INSTALL_DIR/lib -lcustom" }
        }"#;
        let pkg = crate::core::package::parse_package_file(json)
            .unwrap()
            .remove(0);
        let env = build_env_with_package(Path::new("/p"), &pkg, false);
        let effective = env
            .iter()
            .rfind(|(k, _)| k == "LDFLAGS")
            .map(|(_, v)| v.clone())
            .unwrap();
        // Documented merge semantics: a package's env value replaces the
        // default outright. Overriding LDFLAGS therefore drops the rpath, which
        // is worth knowing when a package needs to set it.
        assert_eq!(effective, "-L/p/lib -lcustom");
    }

    /// `TSI_JOBS` is process-global, so these assertions share one test rather
    /// than racing each other across threads.
    #[test]
    fn tsi_jobs_overrides_the_cpu_count_and_rejects_nonsense() {
        let restore = std::env::var("TSI_JOBS").ok();

        std::env::set_var("TSI_JOBS", "3");
        assert_eq!(build_jobs(), 3);

        // Forcing serial is the documented way to debug a parallel build.
        std::env::set_var("TSI_JOBS", "1");
        assert_eq!(build_jobs(), 1);

        let cpus = std::thread::available_parallelism()
            .map(|n| n.get())
            .unwrap_or(1);
        for bad in ["0", "-2", "many", ""] {
            std::env::set_var("TSI_JOBS", bad);
            assert_eq!(build_jobs(), cpus, "TSI_JOBS={bad:?} should fall back");
        }

        std::env::remove_var("TSI_JOBS");
        assert_eq!(build_jobs(), cpus);
        assert!(build_jobs() >= 1);

        if let Some(v) = restore {
            std::env::set_var("TSI_JOBS", v);
        }
    }

    #[test]
    fn the_default_prefix_include_is_demoted_only_on_macos() {
        let flags = default_cppflags(Path::new("/p/include"));
        if cfg!(target_os = "macos") {
            assert_eq!(flags, "-idirafter /p/include");
        } else {
            assert_eq!(flags, "-I/p/include");
        }
    }

    #[test]
    fn a_package_cppflags_replaces_the_default_verbatim() {
        let json = r#"{
            "name": "p",
            "version": "1",
            "source": { "type": "tarball", "url": "https://example.com/x.tar.gz" },
            "build_system": "make",
            "env": { "CPPFLAGS": "-I$TSI_INSTALL_DIR/include" }
        }"#;
        let pkg = crate::core::package::parse_package_file(json)
            .unwrap()
            .remove(0);
        let env = build_env_with_package(Path::new("/p"), &pkg, false);

        // Later entries win when applied to a Command, so the package's value is
        // the effective one -- and it must reach the compiler as written. If it
        // were rewritten to -idirafter, the package could not opt out of the
        // demotion at all: -idirafter beats -I for the same directory in either
        // order.
        let effective = env
            .iter()
            .rfind(|(k, _)| k == "CPPFLAGS")
            .map(|(_, v)| v.clone())
            .unwrap();
        assert_eq!(effective, "-I/p/include");
        assert!(!effective.contains("-idirafter"), "{effective}");
    }

    fn tmpdir(tag: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("tsi-bs-{}-{}", tag, std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    fn pkg_with_build_system(system: &str) -> Package {
        let json = format!(
            r#"{{
                "name": "p",
                "version": "1",
                "source": {{ "type": "tarball", "url": "https://example.com/x.tar.gz" }},
                "build_system": "{}"
            }}"#,
            system
        );
        crate::core::package::parse_package_file(&json)
            .unwrap()
            .remove(0)
    }

    #[test]
    fn a_present_build_system_passes() {
        for (system, marker) in [
            ("cmake", "CMakeLists.txt"),
            ("meson", "meson.build"),
            ("autotools", "configure"),
            ("make", "Makefile"),
        ] {
            let dir = tmpdir(system);
            std::fs::write(dir.join(marker), "").unwrap();
            check_build_system_present(&pkg_with_build_system(system), &dir)
                .unwrap_or_else(|e| panic!("{system}: {e}"));
            let _ = std::fs::remove_dir_all(&dir);
        }
    }

    #[test]
    fn a_missing_build_system_is_reported_with_the_one_actually_shipped() {
        // mongodb's real shape: declared cmake, ships only SConstruct.
        let dir = tmpdir("scons");
        std::fs::write(dir.join("SConstruct"), "").unwrap();

        let err = check_build_system_present(&pkg_with_build_system("cmake"), &dir)
            .unwrap_err()
            .to_string();
        assert!(err.contains("CMakeLists.txt"), "{err}");
        assert!(err.contains("SConstruct"), "{err}");
        assert!(err.contains("SCons"), "{err}");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_bare_source_tree_still_says_what_was_expected() {
        let dir = tmpdir("bare");
        let err = check_build_system_present(&pkg_with_build_system("meson"), &dir)
            .unwrap_err()
            .to_string();
        assert!(err.contains("meson.build"), "{err}");
        // Nothing to suggest, so no misleading "does contain" clause.
        assert!(!err.contains("does contain"), "{err}");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn custom_builds_are_not_second_guessed() {
        let dir = tmpdir("custom");
        check_build_system_present(&pkg_with_build_system("custom"), &dir).unwrap();
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn any_make_variant_filename_counts() {
        for name in ["Makefile", "makefile", "GNUmakefile"] {
            let dir = tmpdir(&format!("mk-{name}"));
            std::fs::write(dir.join(name), "").unwrap();
            check_build_system_present(&pkg_with_build_system("make"), &dir).unwrap();
            let _ = std::fs::remove_dir_all(&dir);
        }
    }

    use crate::core::package::parse_package_file;

    fn pkg_with_make_args(args: &str) -> Package {
        let json = format!(
            r#"{{
                "name": "p",
                "version": "1",
                "source": {{ "type": "tarball", "url": "https://example.com/x.tar.gz" }},
                "build_system": "make",
                "make_args": {}
            }}"#,
            args
        );
        parse_package_file(&json).unwrap().remove(0)
    }

    #[test]
    fn make_args_expand_install_dir_before_make_sees_them() {
        let pkg = pkg_with_make_args(r#"["prefix=$TSI_INSTALL_DIR", "lib=lib"]"#);
        let args = make_args_with_prefix(&pkg, "/opt/tsi/install/libcap-2.70");
        assert_eq!(
            args,
            vec![
                "prefix=/opt/tsi/install/libcap-2.70",
                "lib=lib",
                "PREFIX=/opt/tsi/install/libcap-2.70",
            ]
        );
        // Nothing may reach make still holding the literal variable: make would
        // eat `$T` and leave "SI_INSTALL_DIR".
        assert!(!args.iter().any(|a| a.contains("$TSI_INSTALL_DIR")));
    }

    #[test]
    fn make_args_always_get_tsi_prefix_appended() {
        let args = make_args_with_prefix(&pkg_with_make_args("[]"), "/p");
        assert_eq!(args, vec!["PREFIX=/p"]);
    }

    #[test]
    fn meson_installs_into_a_single_lib_dir() {
        // Without --libdir, meson follows the host convention; on Debian and
        // Ubuntu that is lib/<triple>, where nothing TSI sets up ever looks.
        let args = meson_setup_args(Path::new("/b"), Path::new("/s"), "/p");
        let i = args
            .iter()
            .position(|a| a == "--libdir")
            .expect("no --libdir");
        assert_eq!(args[i + 1], "lib");
        assert_eq!(args[0], "setup");
    }

    #[test]
    fn autotools_make_args_reach_make_and_name_the_shared_prefix() {
        // They were dropped on the floor: an autotools package could declare
        // make_args and nothing passed them to make. readline is the case that
        // needs them, and the path it needs is the shared prefix, where its
        // dependencies were linked -- not its own still-empty install dir.
        let pkg = pkg_with_make_args(r#"["SHLIB_LIBS=-L$TSI_INSTALL_DIR/lib -ltinfow"]"#);
        assert_eq!(
            autotools_make_args(&pkg, "/shared"),
            vec!["SHLIB_LIBS=-L/shared/lib -ltinfow"]
        );

        // No PREFIX= is appended here, unlike the make build system: configure
        // was already told where to install.
        assert!(autotools_make_args(&pkg_with_make_args("[]"), "/shared").is_empty());
    }
}
