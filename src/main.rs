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
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();
    cli::run_with(cli)
}
