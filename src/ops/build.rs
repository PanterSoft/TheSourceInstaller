use crate::core::package::Package;
use anyhow::{Context, Result};
use std::path::Path;
use std::process::Command;

pub fn build(
    pkg: &Package,
    source_dir: &Path,
    build_dir: &Path,
    install_dir: &Path,
    prefix_install: &Path,
) -> Result<()> {
    std::fs::create_dir_all(build_dir).context("Create build dir")?;
    std::fs::create_dir_all(install_dir).context("Create install dir")?;

    for patch in &pkg.patches {
        let status = Command::new("patch")
            .args(["-p1", "-i", patch])
            .current_dir(source_dir)
            .status()
            .context("Run patch")?;
        if !status.success() {
            anyhow::bail!("Patch failed: {}", patch);
        }
    }

    let env = build_env_with_package(prefix_install, pkg);
    let install_path = install_dir.to_string_lossy();

    match pkg.build_system.as_str() {
        "autotools" => build_autotools(pkg, source_dir, build_dir, install_dir, &env)?,
        "cmake" => build_cmake(pkg, source_dir, build_dir, install_dir, &env)?,
        "meson" => build_meson(pkg, source_dir, build_dir, install_dir, &env)?,
        "make" => build_make(pkg, source_dir, build_dir, install_dir, &env)?,
        "custom" => build_custom(pkg, source_dir, &install_path, &env)?,
        _ => anyhow::bail!("Unknown build system: {}", pkg.build_system),
    }
    Ok(())
}

fn build_env_with_package(prefix: &Path, pkg: &Package) -> Vec<(String, String)> {
    let mut env = build_env_base(prefix);
    for (k, v) in &pkg.env {
        env.push((k.clone(), v.clone()));
    }
    env
}

fn build_env_base(prefix: &Path) -> Vec<(String, String)> {
    let bin = prefix.join("bin");
    let lib = prefix.join("lib");
    let include = prefix.join("include");
    let pkgconfig = lib.join("pkgconfig");

    let path = std::env::var("PATH").unwrap_or_default();
    let path_sep = if cfg!(windows) { ";" } else { ":" };
    let new_path = format!(
        "{}{}{}",
        bin.display(),
        path_sep,
        path
    );

    vec![
        ("PATH".to_string(), new_path),
        (
            "PKG_CONFIG_PATH".to_string(),
            pkgconfig.to_string_lossy().to_string(),
        ),
        (
            "LD_LIBRARY_PATH".to_string(),
            lib.to_string_lossy().to_string(),
        ),
        (
            "CPPFLAGS".to_string(),
            format!("-I{}", include.display()),
        ),
        (
            "LDFLAGS".to_string(),
            format!("-L{}", lib.display()),
        ),
    ]
}

fn run_cmd(cmd: &mut Command, env: &[(String, String)]) -> Result<()> {
    for (k, v) in env {
        cmd.env(k, v);
    }
    let status = cmd.status().context("Execute command")?;
    if !status.success() {
        anyhow::bail!("Command failed with exit code: {:?}", status.code());
    }
    Ok(())
}

fn build_autotools(
    pkg: &Package,
    source_dir: &Path,
    build_dir: &Path,
    install_dir: &Path,
    env: &[(String, String)],
) -> Result<()> {
    let prefix = install_dir.to_string_lossy();
    let mut configure_args = vec!["--prefix".to_string(), prefix.to_string()];
    configure_args.extend(pkg.configure_args.clone());

    run_cmd(
        Command::new("./configure")
            .args(&configure_args)
            .current_dir(source_dir)
            .env("TSI_INSTALL_DIR", &prefix),
        env,
    )?;

    run_cmd(Command::new("make").current_dir(source_dir), env)?;
    run_cmd(
        Command::new("make").args(["install"]).current_dir(source_dir),
        env,
    )?;
    Ok(())
}

fn build_cmake(
    pkg: &Package,
    source_dir: &Path,
    build_dir: &Path,
    install_dir: &Path,
    env: &[(String, String)],
) -> Result<()> {
    let prefix = install_dir.to_string_lossy();
    let mut cmake_args = vec![
        "-DCMAKE_INSTALL_PREFIX=".to_string() + &prefix,
        "-S".to_string(),
        source_dir.to_string_lossy().to_string(),
        "-B".to_string(),
        build_dir.to_string_lossy().to_string(),
    ];
    cmake_args.extend(pkg.cmake_args.clone());

    run_cmd(Command::new("cmake").args(&cmake_args), env)?;
    run_cmd(
        Command::new("cmake")
            .args(["--build", build_dir.to_string_lossy().as_ref()]),
        env,
    )?;
    run_cmd(
        Command::new("cmake")
            .args(["--install", build_dir.to_string_lossy().as_ref()]),
        env,
    )?;
    Ok(())
}

fn build_meson(
    pkg: &Package,
    source_dir: &Path,
    build_dir: &Path,
    install_dir: &Path,
    env: &[(String, String)],
) -> Result<()> {
    let prefix = install_dir.to_string_lossy();
    run_cmd(
        Command::new("meson")
            .args([
                "setup",
                build_dir.to_string_lossy().as_ref(),
                source_dir.to_string_lossy().as_ref(),
                "--prefix",
                &prefix,
            ]),
        env,
    )?;
    run_cmd(
        Command::new("meson")
            .args(["compile", "-C", build_dir.to_string_lossy().as_ref()]),
        env,
    )?;
    run_cmd(
        Command::new("meson")
            .args(["install", "-C", build_dir.to_string_lossy().as_ref()]),
        env,
    )?;
    Ok(())
}

fn build_make(
    pkg: &Package,
    source_dir: &Path,
    _build_dir: &Path,
    install_dir: &Path,
    env: &[(String, String)],
) -> Result<()> {
    let prefix = install_dir.to_string_lossy();
    let mut make_args = pkg.make_args.clone();
    make_args.push(format!("PREFIX={}", prefix));
    run_cmd(
        Command::new("make").args(&make_args).current_dir(source_dir),
        env,
    )?;
    run_cmd(
        Command::new("make")
            .args(["install"])
            .args(&make_args)
            .current_dir(source_dir),
        env,
    )?;
    Ok(())
}

fn build_custom(
    pkg: &Package,
    source_dir: &Path,
    install_path: &str,
    env: &[(String, String)],
) -> Result<()> {
    let shell = if cfg!(windows) { "cmd" } else { "sh" };
    let flag = if cfg!(windows) { "/C" } else { "-c" };
    for cmd_str in &pkg.build_commands {
        let expanded = cmd_str.replace("$TSI_INSTALL_DIR", install_path);
        let mut cmd = Command::new(shell);
        cmd.arg(flag)
            .arg(&expanded)
            .current_dir(source_dir)
            .env("TSI_INSTALL_DIR", install_path);
        run_cmd(&mut cmd, env)?;
    }
    Ok(())
}
