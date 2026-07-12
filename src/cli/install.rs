use crate::core::bootstrap;
use crate::core::config::Config;
use crate::core::database::Database;
use crate::core::registry::Registry;
use crate::core::resolver;
use crate::ops::install as ops_install;
use crate::ui;
use anyhow::Result;
use clap::Args;
#[derive(Args)]
pub struct InstallArgs {
    /// One or more package names or specs (e.g. `llvm@21.1.8`, `grpc`, `mariadb`)
    #[arg(required = true)]
    pub packages: Vec<String>,
    #[arg(long)]
    pub force: bool,
    #[arg(long)]
    pub prefix: Option<String>,
    /// Show full build output (default: compact, one line per step)
    #[arg(long)]
    pub verbose: bool,
}

pub fn run(args: InstallArgs) -> Result<()> {
    let (prefix, packages_dir) = crate::cli::resolve_packages_dir(args.prefix.as_deref())?;
    let _guard = crate::ops::install_lock::acquire_install_lock(&prefix)?;
    let db_dir = prefix.join("db");

    let config = Config::load(&prefix);
    let registry = Registry::load_from_dir(&packages_dir)?;
    let mut db = Database::new(&db_dir)?;

    let mut total_installed = 0usize;
    let mut last_pkg: Option<(String, String)> = None;

    for spec in &args.packages {
        let mut installed = db.installed_set();
        if args.force {
            let (root_name, _) = crate::core::package::parse_package_spec(spec);
            installed.remove(&root_name);
        }

        let packages = resolver::resolve(&registry, spec, &installed)?;
        let order = resolver::get_build_order(&packages);

        if order.is_empty() {
            ui::output::section(format!("Nothing to install for {}", spec));
            continue;
        }

        ui::output::section(format!("Resolving dependencies for {}...", spec));
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
            let is_bootstrap_pkg = bootstrap::is_bootstrap_package(&pkg.name);
            let bootstrap_complete = bootstrap::is_bootstrap_complete(&db);
            let isolated = config.strict_isolation && bootstrap_complete && !is_bootstrap_pkg;

            ui::output::section(format!("Fetching {}-{}", pkg.name, pkg.version));
            let url = pkg.source.url.as_deref().unwrap_or("(git)");
            ui::output::detail(url);

            ui::output::section(format!("Building {} {}", pkg.name, pkg.version));
            ops_install::install_package(pkg, &prefix, &mut db, args.force, isolated, args.verbose)?;
            ui::output::section(format!(
                "Linking {} {} into {}",
                pkg.name,
                pkg.version,
                prefix.display()
            ));
            total_installed += 1;
            last_pkg = Some((pkg.name.clone(), pkg.version.clone()));
        }
    }

    ui::output::section("Summary");
    ui::output::detail(format!(
        "{} package build(s) completed successfully.",
        total_installed
    ));
    if let Some((name, version)) = last_pkg {
        ui::output::detail(format!("Last installed: {} {}.", name, version));
    }

    Ok(())
}
