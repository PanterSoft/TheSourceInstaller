use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Source location and type (git, tarball, zip, local).
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PackageSource {
    #[serde(rename = "type")]
    pub source_type: String,
    pub url: Option<String>,
    pub branch: Option<String>,
    pub tag: Option<String>,
    pub commit: Option<String>,
    pub path: Option<String>,
}

/// Single version definition within a package.
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PackageVersion {
    pub version: String,
    #[serde(default)]
    pub description: String,
    pub source: PackageSource,
    #[serde(default)]
    pub dependencies: Vec<String>,
    #[serde(default)]
    pub build_dependencies: Vec<String>,
    #[serde(default)]
    pub build_system: String,
    #[serde(default)]
    pub configure_args: Vec<String>,
    #[serde(default)]
    pub cmake_args: Vec<String>,
    #[serde(default)]
    pub make_args: Vec<String>,
    #[serde(default)]
    pub build_commands: Vec<String>,
    #[serde(default)]
    pub env: HashMap<String, String>,
    #[serde(default)]
    pub env_darwin: Option<HashMap<String, String>>,
    #[serde(default)]
    pub env_linux: Option<HashMap<String, String>>,
    #[serde(default)]
    pub env_windows: Option<HashMap<String, String>>,
    #[serde(default)]
    pub configure_args_darwin: Option<Vec<String>>,
    #[serde(default)]
    pub configure_args_linux: Option<Vec<String>>,
    #[serde(default)]
    pub configure_args_windows: Option<Vec<String>>,
    #[serde(default)]
    pub patches: Vec<String>,
}

#[derive(Debug, Clone, Deserialize, Serialize)]
#[serde(untagged)]
pub enum PackageFile {
    SingleVersion(Box<PackageVersionFile>),
    MultiVersion(MultiVersionPackageFile),
}

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PackageVersionFile {
    pub name: String,
    #[serde(flatten)]
    pub version: PackageVersion,
}

#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct MultiVersionPackageFile {
    pub name: String,
    pub versions: Vec<PackageVersion>,
}

/// Resolved package with a single version (name, version, source, deps, build config).
#[derive(Debug, Clone)]
pub struct Package {
    pub name: String,
    pub version: String,
    pub description: String,
    pub source: PackageSource,
    pub dependencies: Vec<String>,
    pub build_dependencies: Vec<String>,
    pub build_system: String,
    pub configure_args: Vec<String>,
    pub cmake_args: Vec<String>,
    pub make_args: Vec<String>,
    pub build_commands: Vec<String>,
    pub env: HashMap<String, String>,
    pub patches: Vec<String>,
}

impl Package {
    pub fn from_version(name: &str, v: &PackageVersion) -> Self {
        let env = merge_env_for_os(v);
        let configure_args = merge_configure_args_for_os(v);

        Self {
            name: name.to_string(),
            version: v.version.clone(),
            description: v.description.clone(),
            source: v.source.clone(),
            dependencies: v.dependencies.clone(),
            build_dependencies: v.build_dependencies.clone(),
            build_system: v.build_system.clone(),
            configure_args,
            cmake_args: v.cmake_args.clone(),
            make_args: v.make_args.clone(),
            build_commands: v.build_commands.clone(),
            env,
            patches: v.patches.clone(),
        }
    }

    pub fn spec(&self) -> String {
        format!("{}@{}", self.name, self.version)
    }
}

fn merge_env_for_os(v: &PackageVersion) -> HashMap<String, String> {
    let mut env = v.env.clone();
    let os = crate::platform::os_name();
    let override_env = match os {
        "darwin" => v.env_darwin.as_ref(),
        "linux" => v.env_linux.as_ref(),
        "windows" => v.env_windows.as_ref(),
        _ => None,
    };
    if let Some(ov) = override_env {
        for (k, val) in ov {
            env.insert(k.clone(), val.clone());
        }
    }
    env
}

fn merge_configure_args_for_os(v: &PackageVersion) -> Vec<String> {
    let override_args = match crate::platform::os_name() {
        "darwin" => v.configure_args_darwin.as_ref(),
        "linux" => v.configure_args_linux.as_ref(),
        "windows" => v.configure_args_windows.as_ref(),
        _ => None,
    };
    override_args
        .cloned()
        .unwrap_or_else(|| v.configure_args.clone())
}

pub fn parse_package_file(json: &str) -> Result<Vec<Package>, anyhow::Error> {
    let file: PackageFile = serde_json::from_str(json)?;
    match file {
        PackageFile::SingleVersion(f) => {
            let pkg = Package::from_version(&f.name, &f.version);
            Ok(vec![pkg])
        }
        PackageFile::MultiVersion(f) => {
            let pkgs = f
                .versions
                .into_iter()
                .map(|v| Package::from_version(&f.name, &v))
                .collect();
            Ok(pkgs)
        }
    }
}

pub fn parse_package_spec(spec: &str) -> (String, Option<String>) {
    if let Some(at) = spec.find('@') {
        let name = spec[..at].to_string();
        let version = spec[at + 1..].to_string();
        (name, Some(version))
    } else {
        (spec.to_string(), None)
    }
}
