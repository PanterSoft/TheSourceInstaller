use crate::core::bootstrap;
use crate::core::config::Config;
use crate::core::database::Database;
use crate::core::registry::Registry;
use crate::platform;
use crate::ui;
use anyhow::Result;
use clap::Args;
#[derive(Args)]
pub struct DoctorArgs {
    #[arg(long)]
    pub prefix: Option<String>,
}

pub fn run(args: DoctorArgs) -> Result<()> {
    let prefix = platform::resolve_prefix(args.prefix.as_deref());
    let packages_dir = prefix.join("packages");

    ui::output::section("Checking system...");

    let mut warnings = 0;

    let config = Config::load(&prefix);
    let db_dir = prefix.join("db");
    let db_result = Database::new(&db_dir);
    let bootstrap_complete = match &db_result {
        Ok(db) => bootstrap::is_bootstrap_complete(db),
        Err(e) => {
            ui::output::warning(format!("Cannot open database at {}: {}", db_dir.display(), e));
            warnings += 1;
            false
        }
    };

    if config.strict_isolation && bootstrap_complete {
        ui::output::detail("Strict isolation enabled; checking TSI toolchain...");

        let tsi_bin = prefix.join("bin");
        for tool in ["gcc", "make", "tar", "patch"] {
            let path = tsi_bin.join(tool);
            if path.is_file() {
                ui::output::success(format!("TSI {} found at {}", tool, path.display()));
            } else {
                ui::output::warning(format!(
                    "TSI {} not found at {} (run 'tsi bootstrap' or install {})",
                    tool,
                    path.display(),
                    tool
                ));
                warnings += 1;
            }
        }
    } else {
        let cc = if cfg!(windows) { "cl" } else { "cc" };
        if std::process::Command::new(cc)
            .arg("--version")
            .output()
            .is_ok()
        {
            ui::output::success("C compiler found");
        } else {
            ui::output::warning("C compiler not found -- required for building");
            warnings += 1;
        }

        if std::process::Command::new("make")
            .arg("--version")
            .output()
            .is_ok()
        {
            ui::output::success("make found");
        } else {
            ui::output::warning("make not found -- required for most packages");
            warnings += 1;
        }

        if config.strict_isolation && !bootstrap_complete {
            ui::output::warning(
                "Strict isolation enabled but bootstrap toolchain is incomplete. Run 'tsi bootstrap'.",
            );
            warnings += 1;
        }
    }

    if packages_dir.exists() {
        let registry = Registry::load_from_dir(&packages_dir).unwrap_or_else(|_| Registry::new());
        ui::output::success(format!(
            "Package definitions: {} packages available",
            registry.count()
        ));
    } else {
        ui::output::warning("No package definitions -- run 'tsi update'");
        warnings += 1;
    }

    if prefix.exists() {
        if let Ok(meta) = std::fs::metadata(&prefix) {
            if meta.is_dir() {
                ui::output::success(format!("Install prefix: {} (writable)", prefix.display()));
            }
        }
    } else {
        if std::fs::create_dir_all(&prefix).is_ok() {
            ui::output::success(format!("Install prefix: {} (created)", prefix.display()));
        } else {
            ui::output::warning(format!("Cannot create prefix: {}", prefix.display()));
            warnings += 1;
        }
    }

    if std::process::Command::new("git")
        .arg("--version")
        .output()
        .is_ok()
    {
        ui::output::success("git found");
    } else {
        ui::output::warning("git not found -- some packages require git sources");
        warnings += 1;
    }

    if warnings > 0 {
        ui::output::section(format!("{} warning(s) found.", warnings));
    } else {
        ui::output::section("All checks passed.");
    }

    Ok(())
}
