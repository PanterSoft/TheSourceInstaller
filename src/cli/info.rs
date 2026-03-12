use crate::core::registry::Registry;
use crate::ui;
use anyhow::Result;
use clap::Args;
#[derive(Args)]
pub struct InfoArgs {
    pub package: String,
    #[arg(long)]
    pub prefix: Option<String>,
}

pub fn run(args: InfoArgs) -> Result<()> {
    let (_prefix, packages_dir) = crate::cli::resolve_packages_dir(args.prefix.as_deref())?;

    let registry = Registry::load_from_dir(&packages_dir)?;
    let pkg = registry
        .get(&args.package)
        .ok_or_else(|| anyhow::anyhow!("Package not found: {}", args.package))?;

    ui::output::section(format!("{} {}", pkg.name, pkg.version));
    ui::output::detail(format!("Description: {}", pkg.description));
    ui::output::detail(format!("Build system: {}", pkg.build_system));
    if let Some(url) = &pkg.source.url {
        ui::output::detail(format!("Source: {}", url));
    }
    if !pkg.dependencies.is_empty() {
        ui::output::detail(format!("Dependencies: {}", pkg.dependencies.join(", ")));
    }
    if !pkg.build_dependencies.is_empty() {
        ui::output::detail(format!(
            "Build dependencies: {}",
            pkg.build_dependencies.join(", ")
        ));
    }

    Ok(())
}
