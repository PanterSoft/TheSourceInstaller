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
        if !path.exists() {
            return Self {
                strict_isolation: true,
                log_level: "info".to_string(),
            };
        }
        let toml = match std::fs::read_to_string(&path) {
            Ok(t) => t,
            Err(e) => {
                log::warn!("Failed to read config {}: {}", path.display(), e);
                return Self::default();
            }
        };
        let cfg = match toml::from_str::<ConfigFile>(&toml) {
            Ok(c) => c,
            Err(e) => {
                log::warn!("Failed to parse config {}: {}", path.display(), e);
                return Self::default();
            }
        };
        Self {
            strict_isolation: cfg.strict_isolation.unwrap_or(true),
            log_level: cfg.log_level.unwrap_or_else(|| "info".to_string()),
        }
    }
}
