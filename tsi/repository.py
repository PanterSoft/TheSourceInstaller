"""Package repository management."""

import json
from pathlib import Path
from typing import Dict, List, Optional

from tsi.package import Package


class Repository:
    """Manages package repositories and package lookups."""

    def __init__(self, repo_dir: Path):
        """Initialize repository with directory."""
        self.repo_dir = repo_dir
        self.repo_dir.mkdir(parents=True, exist_ok=True)
        self._cache: Dict[str, Package] = {}

    def get_package(self, name: str) -> Package:
        """Get a package by name."""
        if name in self._cache:
            return self._cache[name]

        # Try to find package in repository
        package_file = self.repo_dir / f"{name}.json"
        if package_file.exists():
            package = Package.from_file(package_file)
            self._cache[name] = package
            return package

        # Try to find package as a file path
        package_path = Path(name)
        if package_path.exists() and package_path.suffix == ".json":
            package = Package.from_file(package_path)
            self._cache[name] = package
            return package

        raise ValueError(f"Package not found: {name}")

    def add_package(self, package: Package):
        """Add a package to the repository."""
        package_file = self.repo_dir / f"{package.name}.json"
        with open(package_file, "w") as f:
            json.dump(package.to_dict(), f, indent=2)
        self._cache[package.name] = package

    def list_packages(self) -> List[str]:
        """List all packages in the repository."""
        packages = []
        for json_file in self.repo_dir.glob("*.json"):
            packages.append(json_file.stem)
        return sorted(packages)

    def search(self, query: str) -> List[Package]:
        """Search for packages matching query."""
        results = []
        query_lower = query.lower()

        for package_name in self.list_packages():
            try:
                package = self.get_package(package_name)
                if (
                    query_lower in package.name.lower()
                    or query_lower in package.description.lower()
                ):
                    results.append(package)
            except Exception:
                continue

        return results

