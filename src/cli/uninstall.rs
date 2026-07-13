use crate::core::database::Database;
use crate::platform;
use crate::ui;
use anyhow::Result;
use clap::Args;
#[derive(Args)]
pub struct UninstallArgs {
    pub packages: Vec<String>,
    #[arg(long)]
    pub prefix: Option<String>,
}

pub fn run(args: UninstallArgs) -> Result<()> {
    if args.packages.is_empty() {
        ui::output::error("No packages specified");
        return Err(anyhow::anyhow!(
            "Usage: tsi uninstall <package> [package...]"
        ));
    }

    let prefix = platform::resolve_prefix(args.prefix.as_deref());
    let _guard = crate::ops::install_lock::acquire_install_lock(&prefix)?;
    let db_dir = prefix.join("db");
    let mut db = Database::new(&db_dir)?;

    for name in &args.packages {
        ui::output::section(format!("Uninstalling {}...", name));
        match crate::ops::uninstall::uninstall_package(name, &prefix, &mut db) {
            Ok(true) => ui::output::detail(format!("{} uninstalled", name)),
            Ok(false) => ui::output::warning(format!("{} was not installed", name)),
            Err(e) => ui::output::error(format!("Failed: {}", e)),
        }
    }

    Ok(())
}
