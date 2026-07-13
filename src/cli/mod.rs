mod bootstrap;
mod doctor;
mod info;
mod install;
mod list;
mod remove;
mod search;
pub mod ui;
mod uninstall;
mod update;
mod upgrade;

use anyhow::Result;
use clap::{Parser, Subcommand};
use std::path::PathBuf;

use crate::platform;
use crate::ui as term;

/// Resolves prefix and packages directory; errors if packages dir does not exist.
pub fn resolve_packages_dir(prefix: Option<&str>) -> Result<(PathBuf, PathBuf)> {
    let prefix = platform::resolve_prefix(prefix);
    let packages_dir = prefix.join("packages");
    if !packages_dir.exists() {
        term::output::error("No package definitions found. Run 'tsi update' first.");
        return Err(anyhow::anyhow!(
            "Package directory not found: {}",
            packages_dir.display()
        ));
    }
    Ok((prefix, packages_dir))
}

#[derive(Parser)]
#[command(name = "tsi")]
#[command(about = "TSI - The Source Installer", long_about = None)]
#[command(version)]
pub struct Cli {
    #[command(subcommand)]
    pub command: Commands,
}

#[derive(Subcommand)]
pub enum Commands {
    /// Install a package from source
    Install(install::InstallArgs),
    /// Remove an installed package
    Uninstall(uninstall::UninstallArgs),
    /// Upgrade installed package(s)
    Upgrade(upgrade::UpgradeArgs),
    /// List installed packages
    List(list::ListArgs),
    /// Search available packages
    Search(search::SearchArgs),
    /// Show detailed package information
    Info(info::InfoArgs),
    /// Fetch the latest package definitions
    Update(update::UpdateArgs),
    /// Check your system for potential problems
    Doctor(doctor::DoctorArgs),
    /// Install or repair the TSI bootstrap toolchain
    Bootstrap(bootstrap::BootstrapArgs),
    /// Uninstall TSI from the system
    Remove(remove::RemoveArgs),
    /// Launch the interactive terminal UI
    Ui(ui::UiArgs),
}

/// Extract the `--prefix` argument from any subcommand before full dispatch.
/// This allows `main` to load config early (for log level etc.) without duplicating prefix logic.
pub fn prefix_from_cli(cli: &Cli) -> Option<&str> {
    match &cli.command {
        Commands::Install(a) => a.prefix.as_deref(),
        Commands::Uninstall(a) => a.prefix.as_deref(),
        Commands::Upgrade(a) => a.prefix.as_deref(),
        Commands::List(a) => a.prefix.as_deref(),
        Commands::Search(a) => a.prefix.as_deref(),
        Commands::Info(a) => a.prefix.as_deref(),
        Commands::Update(a) => a.prefix.as_deref(),
        Commands::Doctor(a) => a.prefix.as_deref(),
        Commands::Bootstrap(a) => a.prefix.as_deref(),
        Commands::Remove(a) => a.prefix.as_deref(),
        Commands::Ui(a) => a.prefix.as_deref(),
    }
}

pub fn run() -> Result<()> {
    let cli = Cli::parse();
    run_with(cli)
}

pub fn run_with(cli: Cli) -> Result<()> {
    match cli.command {
        Commands::Install(args) => install::run(args),
        Commands::Uninstall(args) => uninstall::run(args),
        Commands::Upgrade(args) => upgrade::run(args),
        Commands::List(args) => list::run(args),
        Commands::Search(args) => search::run(args),
        Commands::Info(args) => info::run(args),
        Commands::Update(args) => update::run(args),
        Commands::Doctor(args) => doctor::run(args),
        Commands::Bootstrap(args) => bootstrap::run(args),
        Commands::Remove(args) => remove::run(args),
        Commands::Ui(args) => ui::run(args),
    }
}
