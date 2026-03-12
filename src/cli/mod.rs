mod doctor;
mod info;
mod install;
mod list;
mod search;
mod uninstall;
mod update;
mod upgrade;

use anyhow::Result;
use clap::{Parser, Subcommand};
use std::path::PathBuf;

use crate::platform;
use crate::ui;

/// Resolves prefix and packages directory; errors if packages dir does not exist.
pub fn resolve_packages_dir(prefix: Option<&str>) -> Result<(PathBuf, PathBuf)> {
    let prefix = platform::resolve_prefix(prefix);
    let packages_dir = prefix.join("packages");
    if !packages_dir.exists() {
        ui::output::error("No package definitions found. Run 'tsi update' first.");
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
}

pub fn run() -> Result<()> {
    let cli = Cli::parse();
    match cli.command {
        Commands::Install(args) => install::run(args),
        Commands::Uninstall(args) => uninstall::run(args),
        Commands::Upgrade(args) => upgrade::run(args),
        Commands::List(args) => list::run(args),
        Commands::Search(args) => search::run(args),
        Commands::Info(args) => info::run(args),
        Commands::Update(args) => update::run(args),
        Commands::Doctor(args) => doctor::run(args),
    }
}
