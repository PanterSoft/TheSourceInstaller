use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use std::collections::HashSet;
use std::path::Path;

const SCHEMA_VERSION: u32 = 1;

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct InstalledPackage {
    pub name: String,
    pub version: String,
    pub install_path: String,
    pub installed_at: i64,
    #[serde(default)]
    pub dependencies: Vec<String>,
}

#[derive(Debug, Deserialize, Serialize)]
struct DatabaseFile {
    #[serde(default)]
    schema_version: u32,
    installed: Vec<InstalledPackage>,
}

pub struct Database {
    path: std::path::PathBuf,
    packages: Vec<InstalledPackage>,
}

fn read_db_file(path: &Path) -> Result<Vec<InstalledPackage>> {
    if !path.exists() {
        return Ok(vec![]);
    }
    let json = std::fs::read_to_string(path).context("Failed to read database")?;
    let db: DatabaseFile = serde_json::from_str(&json)
        .context("Failed to parse database (it may be corrupted)")?;
    Ok(db.installed)
}

impl Database {
    pub fn new(db_dir: &Path) -> Result<Self> {
        std::fs::create_dir_all(db_dir).context("Failed to create database directory")?;
        let path = db_dir.join("installed.json");
        let packages = read_db_file(&path)?;
        Ok(Self { path, packages })
    }

    pub fn load(&mut self) -> Result<()> {
        self.packages = read_db_file(&self.path)?;
        Ok(())
    }

    fn save(&self) -> Result<()> {
        let db = DatabaseFile {
            schema_version: SCHEMA_VERSION,
            installed: self.packages.clone(),
        };
        let json = serde_json::to_string_pretty(&db).context("Failed to serialize database")?;
        std::fs::write(&self.path, json).context("Failed to write database")?;
        Ok(())
    }

    pub fn is_installed(&self, name: &str) -> bool {
        self.packages.iter().any(|p| p.name == name)
    }

    pub fn add(
        &mut self,
        name: &str,
        version: &str,
        install_path: &Path,
        deps: &[String],
    ) -> Result<()> {
        let installed_at = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .ok()
            .map(|d| d.as_secs() as i64)
            .unwrap_or(0);

        if let Some(existing) = self.packages.iter_mut().find(|p| p.name == name) {
            existing.version = version.to_string();
            existing.install_path = install_path.to_string_lossy().to_string();
            existing.installed_at = installed_at;
            existing.dependencies = deps.to_vec();
        } else {
            self.packages.push(InstalledPackage {
                name: name.to_string(),
                version: version.to_string(),
                install_path: install_path.to_string_lossy().to_string(),
                installed_at,
                dependencies: deps.to_vec(),
            });
        }
        self.save()
    }

    pub fn remove(&mut self, name: &str) -> Result<bool> {
        if let Some(pos) = self.packages.iter().position(|p| p.name == name) {
            self.packages.remove(pos);
            self.save()?;
            Ok(true)
        } else {
            Ok(false)
        }
    }

    pub fn get(&self, name: &str) -> Option<&InstalledPackage> {
        self.packages.iter().find(|p| p.name == name)
    }

    pub fn list(&self) -> &[InstalledPackage] {
        &self.packages
    }

    pub fn installed_set(&self) -> HashSet<String> {
        self.packages.iter().map(|p| p.name.clone()).collect()
    }
}
