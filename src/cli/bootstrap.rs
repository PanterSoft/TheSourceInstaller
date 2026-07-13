use crate::core::bootstrap;
use crate::core::database::Database;
use crate::core::registry::Registry;
use crate::core::resolver;
use crate::ops::install as ops_install;
use crate::ui;
use anyhow::Result;
use clap::Args;

#[derive(Args)]
pub struct BootstrapArgs {
    #[arg(long)]
    pub prefix: Option<String>,
    /// Show full build output (default: compact, one line per step)
    #[arg(long)]
    pub verbose: bool,
}

pub fn run(args: BootstrapArgs) -> Result<()> {
    let (prefix, packages_dir) = crate::cli::resolve_packages_dir(args.prefix.as_deref())?;
    let _guard = crate::ops::install_lock::acquire_install_lock(&prefix)?;
    let db_dir = prefix.join("db");

    let registry = Registry::load_from_dir(&packages_dir)?;
    let mut db = Database::new(&db_dir)?;

    let mut remaining: Vec<String> = bootstrap::BOOTSTRAP_PACKAGES
        .iter()
        .filter(|name| !db.is_installed(name))
        .map(|name| (*name).to_string())
        .collect();

    if remaining.is_empty() {
        ui::output::section("Bootstrap toolchain already complete.");
        return Ok(());
    }

    ui::output::section("Bootstrapping core toolchain packages");
    ui::output::detail(format!("Packages: {}", remaining.join(", ")));

    let total = remaining.len();
    for (idx, name) in remaining.drain(..).enumerate() {
        let spec = name.as_str();
        ui::output::section(format!(
            "Installing bootstrap package {}/{}: {}",
            idx + 1,
            total,
            spec
        ));

        let installed_set = db.installed_set();
        let packages = resolver::resolve(&registry, spec, &installed_set)?;
        let order = resolver::get_build_order(&packages);

        for pkg in &order {
            let use_isolation = false;
            ops_install::install_package(
                pkg,
                &prefix,
                &mut db,
                false,
                use_isolation,
                args.verbose,
            )?;
        }
    }

    ui::output::section("Bootstrap toolchain installation complete.");

    Ok(())
}
