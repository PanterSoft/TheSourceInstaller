use crate::core::package::{parse_package_file, parse_package_spec, Package};
use anyhow::{Context, Result};
use std::collections::HashMap;
use std::path::Path;
use walkdir::WalkDir;

#[derive(Debug, Default)]
pub struct Registry {
    packages: HashMap<String, Vec<Package>>,
}

impl Registry {
    pub fn new() -> Self {
        Self::default()
    }

    /// Loads all package JSON files from a directory. Parse errors are logged and skipped (best-effort).
    pub fn load_from_dir(dir: &Path) -> Result<Self> {
        let mut registry = Self::new();
        for entry in WalkDir::new(dir)
            .max_depth(1)
            .into_iter()
            .filter_map(|e| e.ok())
        {
            let path = entry.path();
            if path.extension().is_some_and(|e| e == "json") {
                let json = std::fs::read_to_string(path)
                    .with_context(|| format!("Failed to read {}", path.display()))?;
                match parse_package_file(&json) {
                    Ok(pkgs) => {
                        for pkg in pkgs {
                            registry
                                .packages
                                .entry(pkg.name.clone())
                                .or_default()
                                .push(pkg);
                        }
                    }
                    Err(e) => {
                        log::warn!("Skipping {} (parse error): {}", path.display(), e);
                    }
                }
            }
        }
        for versions in registry.packages.values_mut() {
            versions.sort_by(|a, b| semver_compare(&b.version, &a.version));
        }
        Ok(registry)
    }

    pub fn get(&self, spec: &str) -> Option<&Package> {
        let (name, version) = parse_package_spec(spec);
        let versions = self.packages.get(&name)?;
        match version {
            None => versions.first(),
            Some(v) => versions.iter().find(|p| p.version == v),
        }
    }

    pub fn get_versions(&self, name: &str) -> Option<&[Package]> {
        self.packages.get(name).map(|v| v.as_slice())
    }

    pub fn all_packages(&self) -> impl Iterator<Item = &Package> {
        self.packages.values().flat_map(|v| v.iter())
    }

    pub fn package_names(&self) -> impl Iterator<Item = &String> {
        self.packages.keys()
    }

    pub fn search(&self, query: &str) -> Vec<&Package> {
        let q = query.to_lowercase();
        let mut results: Vec<&Package> = self
            .packages
            .values()
            .filter_map(|versions| versions.first())
            .filter(|p| {
                p.name.to_lowercase().contains(&q) || p.description.to_lowercase().contains(&q)
            })
            .collect();
        results.sort_by(|a, b| a.name.cmp(&b.name));
        results
    }

    pub fn count(&self) -> usize {
        self.packages.len()
    }
}

fn semver_compare(a: &str, b: &str) -> std::cmp::Ordering {
    let a_parts: Vec<&str> = a.split(&['.', '-', '_'][..]).collect();
    let b_parts: Vec<&str> = b.split(&['.', '-', '_'][..]).collect();
    for i in 0..a_parts.len().max(b_parts.len()) {
        let a_val = a_parts.get(i);
        let b_val = b_parts.get(i);
        // A segment that's present but fails to parse (e.g. "rc1") is a
        // pre-release identifier and must sort lower than a numeric/absent
        // segment (which represents a genuine release), not be treated as
        // an equal "0".
        let a_nonnumeric = a_val.is_some_and(|s| s.parse::<u64>().is_err());
        let b_nonnumeric = b_val.is_some_and(|s| s.parse::<u64>().is_err());
        if a_nonnumeric != b_nonnumeric {
            return if a_nonnumeric {
                std::cmp::Ordering::Less
            } else {
                std::cmp::Ordering::Greater
            };
        }
        let a_num: u64 = a_val.and_then(|s| s.parse().ok()).unwrap_or(0);
        let b_num: u64 = b_val.and_then(|s| s.parse().ok()).unwrap_or(0);
        match a_num.cmp(&b_num) {
            std::cmp::Ordering::Equal => continue,
            o => return o,
        }
    }
    std::cmp::Ordering::Equal
}

#[cfg(test)]
mod semver_compare_tests {
    use super::semver_compare;

    #[test]
    fn release_sorts_above_its_own_prerelease() {
        assert_eq!(
            semver_compare("3.0.0", "3.0.0-rc1"),
            std::cmp::Ordering::Greater
        );
    }
}
