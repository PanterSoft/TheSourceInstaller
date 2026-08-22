use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Source location and type (git, tarball, zip, local).
#[derive(Debug, Clone, Default, Deserialize, Serialize)]
pub struct PackageSource {
    #[serde(rename = "type")]
    pub source_type: String,
    pub url: Option<String>,
    pub branch: Option<String>,
    pub tag: Option<String>,
    pub commit: Option<String>,
    pub path: Option<String>,
    /// Optional SHA-256 checksum (lowercase hex) for archive downloads.
    /// When present, the downloaded archive is verified before extraction.
    #[serde(default)]
    pub sha256: Option<String>,
}

/// Single version definition within a package.
#[derive(Debug, Clone, Deserialize, Serialize)]
pub struct PackageVersion {
    pub version: String,
    #[serde(default)]
    pub description: String,
    /// Absent for `build_system: "meta"` packages, which install nothing of
    /// their own and exist only to pull in their dependencies.
    #[serde(default)]
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
    pub cmake_args_darwin: Option<Vec<String>>,
    #[serde(default)]
    pub cmake_args_linux: Option<Vec<String>>,
    #[serde(default)]
    pub cmake_args_windows: Option<Vec<String>>,
    #[serde(default)]
    pub make_args_darwin: Option<Vec<String>>,
    #[serde(default)]
    pub make_args_linux: Option<Vec<String>>,
    #[serde(default)]
    pub make_args_windows: Option<Vec<String>>,
    #[serde(default)]
    pub env_x86_64: Option<HashMap<String, String>>,
    #[serde(default)]
    pub env_aarch64: Option<HashMap<String, String>>,
    #[serde(default)]
    pub configure_args_x86_64: Option<Vec<String>>,
    #[serde(default)]
    pub configure_args_aarch64: Option<Vec<String>>,
    #[serde(default)]
    pub patches: Vec<String>,
    /// Subdirectory within the fetched source tree where the build root lives (e.g. "avro-c-1.11.3" when the tarball extracts with that top-level dir and we store as {name}-{version}).
    #[serde(default)]
    pub source_dir: Option<String>,
    /// Platforms this version can build on. Entries are `os` ("linux", "darwin",
    /// "windows") or `os-arch` ("linux-aarch64"). Empty means "every platform".
    #[serde(default)]
    pub platforms: Vec<String>,
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
    pub source_dir: Option<String>,
    pub platforms: Vec<String>,
}

impl Package {
    pub fn from_version(name: &str, v: &PackageVersion) -> Self {
        let env = merge_env_for_os(v);
        let configure_args = merge_configure_args_for_os(v);
        let cmake_args = merge_cmake_args_for_os(v);
        let make_args = merge_make_args_for_os(v);

        Self {
            name: name.to_string(),
            version: v.version.clone(),
            description: v.description.clone(),
            source: v.source.clone(),
            dependencies: v.dependencies.clone(),
            build_dependencies: v.build_dependencies.clone(),
            build_system: v.build_system.clone(),
            configure_args,
            cmake_args,
            make_args,
            build_commands: v.build_commands.clone(),
            env,
            patches: v.patches.clone(),
            source_dir: v.source_dir.clone(),
            platforms: v.platforms.clone(),
        }
    }

    /// True when this package declares no platform restriction, or declares one
    /// that matches the host. Entries match either the bare OS ("linux") or the
    /// full `os-arch` pair ("linux-aarch64").
    pub fn supports_host(&self) -> bool {
        supports(
            &self.platforms,
            crate::platform::os_name(),
            crate::platform::arch_name(),
        )
    }

    pub fn spec(&self) -> String {
        format!("{}@{}", self.name, self.version)
    }
}

fn supports(platforms: &[String], os: &str, arch: &str) -> bool {
    platforms.is_empty()
        || platforms
            .iter()
            .any(|p| p == os || *p == format!("{}-{}", os, arch))
}

fn merge_env_for_os(v: &PackageVersion) -> HashMap<String, String> {
    let mut env = v.env.clone();
    // Apply OS-specific overrides first.
    let os_env = match crate::platform::os_name() {
        "darwin" => v.env_darwin.as_ref(),
        "linux" => v.env_linux.as_ref(),
        "windows" => v.env_windows.as_ref(),
        _ => None,
    };
    if let Some(ov) = os_env {
        for (k, val) in ov {
            env.insert(k.clone(), val.clone());
        }
    }
    // Apply arch-specific overrides on top (arch wins over OS).
    let arch_env = match crate::platform::arch_name() {
        "x86_64" => v.env_x86_64.as_ref(),
        "aarch64" => v.env_aarch64.as_ref(),
        _ => None,
    };
    if let Some(ov) = arch_env {
        for (k, val) in ov {
            env.insert(k.clone(), val.clone());
        }
    }
    env
}

fn merge_configure_args_for_os(v: &PackageVersion) -> Vec<String> {
    // OS-specific args replace the base args (existing behaviour).
    let os_override = match crate::platform::os_name() {
        "darwin" => v.configure_args_darwin.as_ref(),
        "linux" => v.configure_args_linux.as_ref(),
        "windows" => v.configure_args_windows.as_ref(),
        _ => None,
    };
    let mut args = os_override
        .cloned()
        .unwrap_or_else(|| v.configure_args.clone());
    // Arch-specific args are appended on top (additive).
    let arch_extra = match crate::platform::arch_name() {
        "x86_64" => v.configure_args_x86_64.as_deref(),
        "aarch64" => v.configure_args_aarch64.as_deref(),
        _ => None,
    };
    if let Some(extra) = arch_extra {
        args.extend_from_slice(extra);
    }
    args
}

fn merge_cmake_args_for_os(v: &PackageVersion) -> Vec<String> {
    // Same semantics as configure_args: OS-specific list replaces base when present.
    let os_override = match crate::platform::os_name() {
        "darwin" => v.cmake_args_darwin.as_ref(),
        "linux" => v.cmake_args_linux.as_ref(),
        "windows" => v.cmake_args_windows.as_ref(),
        _ => None,
    };
    os_override.cloned().unwrap_or_else(|| v.cmake_args.clone())
}

/// Same semantics as configure_args and cmake_args: an OS-specific list
/// replaces the base list when present.
///
/// giflib is why this exists. Its Makefile assigns CFLAGS itself, so only a
/// make command-line argument can change it, and on macOS the link line needs
/// an -install_name that would be an error on Linux -- there was no way to say
/// that in a package definition.
fn merge_make_args_for_os(v: &PackageVersion) -> Vec<String> {
    let os_override = match crate::platform::os_name() {
        "darwin" => v.make_args_darwin.as_ref(),
        "linux" => v.make_args_linux.as_ref(),
        "windows" => v.make_args_windows.as_ref(),
        _ => None,
    };
    os_override.cloned().unwrap_or_else(|| v.make_args.clone())
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

#[cfg(test)]
mod make_args_os_tests {
    use super::*;

    fn parse(json: &str) -> Package {
        parse_package_file(json).unwrap().remove(0)
    }

    #[test]
    fn an_os_specific_make_args_list_replaces_the_base_one() {
        // libgif needs an -install_name on macOS that would be an error on
        // Linux, and its Makefile assigns CFLAGS itself, so nothing but a make
        // command-line argument can carry it.
        let pkg = parse(
            r#"{
                "name": "libgif", "version": "5.2.2", "description": "d",
                "source": {"type": "tarball", "url": "http://x/libgif-5.2.2.tar.gz"},
                "build_system": "make",
                "make_args": ["MAKE=true", "PREFIX=$TSI_INSTALL_DIR"],
                "make_args_darwin": ["MAKE=true", "CFLAGS=-Wl,-install_name,x"],
                "make_args_linux": ["MAKE=true", "PREFIX=$TSI_INSTALL_DIR"]
            }"#,
        );
        let expected: Vec<String> = match crate::platform::os_name() {
            "darwin" => vec!["MAKE=true".into(), "CFLAGS=-Wl,-install_name,x".into()],
            "linux" => vec!["MAKE=true".into(), "PREFIX=$TSI_INSTALL_DIR".into()],
            // No list for this OS: the base one stands.
            _ => vec!["MAKE=true".into(), "PREFIX=$TSI_INSTALL_DIR".into()],
        };
        assert_eq!(pkg.make_args, expected);
    }

    #[test]
    fn without_an_os_list_the_base_make_args_are_used() {
        let pkg = parse(
            r#"{
                "name": "lmdb", "version": "0.9.31", "description": "d",
                "source": {"type": "tarball", "url": "http://x/lmdb-0.9.31.tar.gz"},
                "build_system": "make",
                "make_args": ["prefix=$TSI_INSTALL_DIR"]
            }"#,
        );
        assert_eq!(pkg.make_args, vec!["prefix=$TSI_INSTALL_DIR".to_string()]);
    }
}
