use crate::core::package::{parse_package_spec, Package};
use crate::core::registry::Registry;
use anyhow::{anyhow, Result};
use std::collections::{HashMap, HashSet};

/// Resolves a package spec (name or name@version) and its dependencies into a build order.
pub fn resolve(
    registry: &Registry,
    spec: &str,
    installed: &HashSet<String>,
) -> Result<Vec<Package>> {
    let mut result = Vec::new();
    let mut visited = HashSet::new();
    let mut stack = HashSet::new();
    resolve_recursive(
        registry,
        spec,
        installed,
        &mut result,
        &mut visited,
        &mut stack,
    )?;
    Ok(result)
}

fn resolve_recursive(
    registry: &Registry,
    spec: &str,
    installed: &HashSet<String>,
    result: &mut Vec<Package>,
    visited: &mut HashSet<String>,
    stack: &mut HashSet<String>,
) -> Result<()> {
    let (name, _) = parse_package_spec(spec);
    if stack.contains(&name) {
        return Err(anyhow!("Circular dependency detected involving: {}", name));
    }
    if visited.contains(&name) {
        return Ok(());
    }
    visited.insert(name.clone());
    stack.insert(name.clone());

    let pkg = registry
        .get(spec)
        .ok_or_else(|| anyhow!("Package not found: {}", spec))?
        .clone();

    for dep in pkg.dependencies.iter().chain(pkg.build_dependencies.iter()) {
        if installed.contains(dep) || result.iter().any(|p| p.name == *dep) {
            continue;
        }
        resolve_recursive(registry, dep, installed, result, visited, stack)?;
    }

    stack.remove(&name);

    if !installed.contains(&pkg.name) && !result.iter().any(|p| p.name == pkg.name) {
        result.push(pkg);
    }
    Ok(())
}

/// Returns packages in topological build order (dependencies before dependents).
pub fn get_build_order(packages: &[Package]) -> Vec<Package> {
    let name_to_pkg: HashMap<String, &Package> =
        packages.iter().map(|p| (p.name.clone(), p)).collect();
    let mut in_degree: HashMap<String, usize> =
        name_to_pkg.keys().map(|n| (n.clone(), 0)).collect();

    for pkg in packages {
        for dep in pkg.dependencies.iter().chain(pkg.build_dependencies.iter()) {
            if name_to_pkg.contains_key(dep) {
                *in_degree.get_mut(&pkg.name).unwrap() += 1;
            }
        }
    }

    let mut queue: Vec<String> = in_degree
        .iter()
        .filter(|(_, &d)| d == 0)
        .map(|(n, _)| n.clone())
        .collect();
    let mut order = Vec::new();

    while let Some(name) = queue.pop() {
        if let Some(&pkg) = name_to_pkg.get(&name) {
            order.push(pkg.clone());
        }
        for pkg in packages {
            if pkg.dependencies.contains(&name) || pkg.build_dependencies.contains(&name) {
                if let Some(d) = in_degree.get_mut(&pkg.name) {
                    *d = d.saturating_sub(1);
                    if *d == 0 {
                        queue.push(pkg.name.clone());
                    }
                }
            }
        }
    }

    for pkg in packages {
        if !order.iter().any(|p| p.name == pkg.name) {
            order.push(pkg.clone());
        }
    }

    order
}
