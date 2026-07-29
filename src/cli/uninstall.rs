use crate::core::database::Database;
use crate::platform;
use crate::ui;
use anyhow::Result;
use clap::Args;
#[derive(Args)]
pub struct UninstallArgs {
    pub packages: Vec<String>,
    /// Remove even if other installed packages depend on it
    #[arg(long)]
    pub force: bool,
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

    // Keep going through the whole list, but exit non-zero if anything was refused or
    // failed, so scripts can't mistake a partial uninstall for a clean one.
    let mut failed = 0usize;

    for name in &args.packages {
        let revdeps = db.reverse_dependencies(name);
        if !revdeps.is_empty() && !args.force {
            ui::output::error(format!(
                "{} is required by: {}. Uninstall those first, or pass --force.",
                name,
                revdeps.join(", ")
            ));
            failed += 1;
            continue;
        }
        ui::output::section(format!("Uninstalling {}...", name));
        match crate::ops::uninstall::uninstall_package(name, &prefix, &mut db) {
            Ok(true) => ui::output::detail(format!("{} uninstalled", name)),
            Ok(false) => ui::output::warning(format!("{} was not installed", name)),
            Err(e) => {
                ui::output::error(format!("Failed: {}", e));
                failed += 1;
            }
        }
    }

    if failed > 0 {
        anyhow::bail!("{} package(s) could not be uninstalled", failed);
    }
    Ok(())
}
