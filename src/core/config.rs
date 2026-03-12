use serde::Deserialize;
use std::path::Path;

#[derive(Debug, Clone, Default)]
pub struct Config {
    pub strict_isolation: bool,
    pub log_level: String,
}

#[derive(Debug, Deserialize)]
struct ConfigFile {
    strict_isolation: Option<bool>,
    log_level: Option<String>,
}

impl Config {
    pub fn load(prefix: &Path) -> Self {
        let path = prefix.join("tsi.toml");
        if path.exists() {
            if let Ok(toml) = std::fs::read_to_string(&path) {
                if let Ok(cfg) = toml::from_str::<ConfigFile>(&toml) {
                    return Self {
                        strict_isolation: cfg.strict_isolation.unwrap_or(false),
                        log_level: cfg.log_level.unwrap_or_else(|| "info".to_string()),
                    };
                }
            }
        }
        Self::default()
    }
}
