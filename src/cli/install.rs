use crate::core::database::Database;
use crate::core::registry::Registry;
use crate::core::resolver;
use crate::ops::install as ops_install;
use crate::platform;
use crate::ui;
use anyhow::Result;
use clap::Args;
#[derive(Args)]
pub struct InstallArgs {
    pub package: String,
    #[arg(long)]
    pub force: bool,
    #[arg(long)]
    pub prefix: Option<String>,
}

pub fn run(args: InstallArgs) -> Result<()> {
    let prefix = platform::resolve_prefix(args.prefix.as_deref());
    let packages_dir = prefix.join("packages");
    let db_dir = prefix.join("db");

    if !packages_dir.exists() {
        ui::output::error("No package definitions found. Run 'tsi update' first.");
        return Err(anyhow::anyhow!(
            "Package directory not found: {}",
            packages_dir.display()
        ));
    }

    let registry = Registry::load_from_dir(&packages_dir)?;
    let mut db = Database::new(&db_dir)?;
    let installed = db.installed_set();

    let packages = resolver::resolve(&registry, &args.package, &installed)?;
    let order = resolver::get_build_order(&packages);

    if order.is_empty() {
        ui::output::section("Nothing to install");
        return Ok(());
    }

    ui::output::section(format!("Resolving dependencies for {}...", args.package));
    ui::output::section(format!(
        "Installing {} packages: {}",
        order.len(),
        order
            .iter()
            .map(|p| p.name.as_str())
            .collect::<Vec<_>>()
            .join(", ")
    ));

    for pkg in order.iter() {
        ui::output::section(format!("Fetching {}-{}", pkg.name, pkg.version));
        let url = pkg.source.url.as_deref().unwrap_or("(git)");
        ui::output::detail(url);

        ui::output::section(format!("Building {} {}", pkg.name, pkg.version));
        ops_install::install_package(pkg, &prefix, &mut db, args.force)?;
        ui::output::section(format!(
            "Linking {} {} into {}",
            pkg.name,
            pkg.version,
            prefix.display()
        ));
    }

    ui::output::section("Summary");
    ui::output::detail(format!("{} packages installed successfully.", order.len()));
    if let Some(last) = order.last() {
        ui::output::detail(format!("{} {} is ready to use.", last.name, last.version));
    }

    Ok(())
}
