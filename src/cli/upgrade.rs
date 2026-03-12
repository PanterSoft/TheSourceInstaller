use crate::core::database::Database;
use crate::core::registry::Registry;
use crate::core::resolver;
use crate::ops::install as ops_install;
use crate::platform;
use crate::ui;
use anyhow::Result;
use clap::Args;
use std::path::PathBuf;

#[derive(Args)]
pub struct UpgradeArgs {
    pub packages: Vec<String>,
    #[arg(long)]
    pub prefix: Option<String>,
}

pub fn run(args: UpgradeArgs) -> Result<()> {
    let prefix = PathBuf::from(platform::resolve_prefix(args.prefix.as_deref()));
    let packages_dir = prefix.join("packages");
    let db_dir = prefix.join("db");

    if !packages_dir.exists() {
        ui::output::error("No package definitions found. Run 'tsi update' first.");
        return Err(anyhow::anyhow!("Package directory not found"));
    }

    let registry = Registry::load_from_dir(&packages_dir)?;
    let db = Database::new(&db_dir)?;

    let to_upgrade: Vec<String> = if args.packages.is_empty() {
        db.list().iter().map(|p| p.name.clone()).collect()
    } else {
        args.packages.clone()
    };

    if to_upgrade.is_empty() {
        ui::output::section("Nothing to upgrade");
        return Ok(());
    }

    for name in &to_upgrade {
        if let Some(installed) = db.get(name) {
            if let Some(latest) = registry.get(name) {
                if latest.version != installed.version {
                    ui::output::section(format!(
                        "Upgrading {} from {} to {}",
                        name, installed.version, latest.version
                    ));
                    let mut db_mut = Database::new(&db_dir)?;
                    crate::ops::uninstall::uninstall_package(name, &prefix, &mut db_mut)?;
                    let installed_set = db_mut.installed_set();
                    let packages = resolver::resolve(&registry, &format!("{}@{}", name, latest.version), &installed_set)?;
                    let order = resolver::get_build_order(&packages);
                    for pkg in &order {
                        ops_install::install_package(pkg, &prefix, &mut db_mut, true)?;
                    }
                } else {
                    ui::output::detail(format!("{} {} is already up to date", name, installed.version));
                }
            }
        } else {
            ui::output::warning(format!("{} is not installed", name));
        }
    }

    Ok(())
}
