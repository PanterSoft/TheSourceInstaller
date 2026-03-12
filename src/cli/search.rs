use crate::core::registry::Registry;
use crate::platform;
use crate::ui;
use anyhow::Result;
use clap::Args;
#[derive(Args)]
pub struct SearchArgs {
    pub query: String,
    #[arg(long)]
    pub prefix: Option<String>,
}

pub fn run(args: SearchArgs) -> Result<()> {
    let prefix = platform::resolve_prefix(args.prefix.as_deref());
    let packages_dir = prefix.join("packages");

    if !packages_dir.exists() {
        ui::output::error("No package definitions found. Run 'tsi update' first.");
        return Err(anyhow::anyhow!("Package directory not found"));
    }

    let registry = Registry::load_from_dir(&packages_dir)?;
    let results = registry.search(&args.query);

    ui::output::section(format!("Results for \"{}\"", args.query));
    if results.is_empty() {
        ui::output::detail("No matching packages found");
        return Ok(());
    }

    let max_name = results.iter().map(|p| p.name.len()).max().unwrap_or(0).max(20);
    for pkg in results {
        ui::output::detail(format!(
            "{} {}  {}",
            ui::table::pad_right(&pkg.name, max_name),
            pkg.version,
            pkg.description
        ));
    }

    Ok(())
}
