use crate::core::database::Database;
use crate::platform;
use crate::ui;
use anyhow::Result;
use clap::Args;
use std::path::PathBuf;

#[derive(Args)]
pub struct ListArgs {
    #[arg(long)]
    pub versions: bool,
    #[arg(long)]
    pub prefix: Option<String>,
}

pub fn run(args: ListArgs) -> Result<()> {
    let prefix = PathBuf::from(platform::resolve_prefix(args.prefix.as_deref()));
    let db_dir = prefix.join("db");
    let db = Database::new(&db_dir)?;

    let packages = db.list();
    if packages.is_empty() {
        ui::output::info("No packages installed");
        return Ok(());
    }

    ui::output::section("Installed packages:");
    let max_name = packages.iter().map(|p| p.name.len()).max().unwrap_or(0).max(20);
    for pkg in packages {
        if args.versions {
            ui::output::detail(format!(
                "{} {}",
                ui::table::pad_right(&pkg.name, max_name),
                pkg.version
            ));
        } else {
            ui::output::detail(format!(
                "{} {}",
                ui::table::pad_right(&pkg.name, max_name),
                pkg.version
            ));
        }
    }

    Ok(())
}
