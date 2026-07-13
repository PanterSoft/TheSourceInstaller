use anyhow::Result;
use clap::Parser;
use tsi::cli;

fn main() -> Result<()> {
    let cli = match cli::Cli::try_parse() {
        Ok(c) => c,
        Err(e) => {
            _ = e.print();
            std::process::exit(e.exit_code());
        }
    };

    // Load config early so we can use log_level from tsi.toml as the default.
    // RUST_LOG still overrides this (power-user behaviour preserved).
    let prefix = tsi::platform::resolve_prefix(cli::prefix_from_cli(&cli));
    let config = tsi::core::config::Config::load(&prefix);
    let log_default = std::env::var("RUST_LOG").unwrap_or(config.log_level);
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or(&log_default))
        .init();

    cli::run_with(cli)
}
