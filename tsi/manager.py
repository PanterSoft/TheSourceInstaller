"""Main package manager orchestrator."""

import sys
from pathlib import Path
from typing import Optional

from tsi.config import Config
from tsi.package import Package
from tsi.repository import Repository
from tsi.database import Database
from tsi.resolver import DependencyResolver
from tsi.fetcher import SourceFetcher
from tsi.builder import Builder


class PackageManager:
    """Main package manager class."""

    def __init__(self, config: Config):
        """Initialize package manager."""
        self.config = config
        self.repository = Repository(config.repo_dir)
        self.database = Database(config.db_dir)
        self.resolver = DependencyResolver(self.repository)
        self.fetcher = SourceFetcher(config.source_dir)
        self.builder = Builder(config)

    def install(self, package_spec: str, force: bool = False):
        """Install a package and its dependencies."""
        print(f"Installing package: {package_spec}")

        # Get package
        try:
            package = self.repository.get_package(package_spec)
        except ValueError:
            # Try as file path
            package_path = Path(package_spec)
            if package_path.exists():
                package = Package.from_file(package_path)
            else:
                raise ValueError(f"Package not found: {package_spec}")

        # Check if already installed
        if self.database.is_installed(package.name) and not force:
            print(f"Package {package.name} is already installed. Use --force to reinstall.")
            return

        # Resolve dependencies
        print(f"Resolving dependencies for {package.name}...")
        installed = self.database.get_all_installed()
        dependencies = self.resolver.resolve(package.name, installed)
        build_order = self.resolver.get_build_order(dependencies)

        # Install dependencies first
        for dep_name in build_order:
            if dep_name == package.name:
                continue

            if self.database.is_installed(dep_name) and not force:
                print(f"Dependency {dep_name} already installed, skipping...")
                continue

            print(f"Installing dependency: {dep_name}")
            self._install_package(dep_name, force)

        # Install main package
        print(f"Installing {package.name}...")
        self._install_package(package.name, force)

        print(f"Successfully installed {package.name}")

    def _install_package(self, package_name: str, force: bool = False):
        """Install a single package."""
        package = self.repository.get_package(package_name)

        # Fetch source
        print(f"Fetching source for {package.name}...")
        source_dir = self.fetcher.fetch(package, force=force)

        # Build
        print(f"Building {package.name}...")
        build_dir = self.config.build_dir / package.name
        env = self.builder._prepare_environment(package)
        self.builder.build(package, source_dir, build_dir)

        # Install
        print(f"Installing {package.name}...")
        self.builder.install(package, source_dir, build_dir, env)

        # Record in database
        self.database.add_package(package, self.config.install_dir)

    def remove(self, package_name: str):
        """Remove an installed package."""
        if not self.database.is_installed(package_name):
            print(f"Package {package_name} is not installed.")
            return

        # TODO: Check for reverse dependencies
        print(f"Removing {package_name}...")
        self.database.remove_package(package_name)
        print(f"Removed {package_name}")

    def list_installed(self, show_all: bool = False):
        """List installed packages."""
        installed = self.database.list_installed()
        if not installed:
            print("No packages installed.")
            return

        print("Installed packages:")
        for pkg_name in sorted(installed):
            info = self.database.get_package_info(pkg_name)
            version = info.get("version", "unknown")
            print(f"  {pkg_name} ({version})")

    def search(self, query: str):
        """Search for packages."""
        results = self.repository.search(query)
        if not results:
            print(f"No packages found matching '{query}'")
            return

        print(f"Found {len(results)} package(s):")
        for package in results:
            print(f"  {package.name} - {package.description}")

    def update(self, package_name: Optional[str] = None):
        """Update a package or all packages."""
        if package_name:
            print(f"Updating {package_name}...")
            self.install(package_name, force=True)
        else:
            print("Updating all packages...")
            installed = self.database.list_installed()
            for pkg_name in installed:
                print(f"Updating {pkg_name}...")
                self.install(pkg_name, force=True)

    def show_info(self, package_name: str):
        """Show information about a package."""
        try:
            package = self.repository.get_package(package_name)
        except ValueError:
            print(f"Package not found: {package_name}")
            return

        print(f"Package: {package.name}")
        print(f"Version: {package.version}")
        print(f"Description: {package.description}")
        print(f"Build System: {package.build_system}")
        if package.dependencies:
            print(f"Dependencies: {', '.join(package.dependencies)}")
        if package.build_dependencies:
            print(f"Build Dependencies: {', '.join(package.build_dependencies)}")

        if self.database.is_installed(package_name):
            info = self.database.get_package_info(package_name)
            print(f"Status: Installed")
            print(f"Installed at: {info.get('installed_at', 'unknown')}")
        else:
            print("Status: Not installed")

    def build(self, package_spec: str):
        """Build a package without installing."""
        print(f"Building package: {package_spec}")

        # Get package
        try:
            package = self.repository.get_package(package_spec)
        except ValueError:
            package_path = Path(package_spec)
            if package_path.exists():
                package = Package.from_file(package_path)
            else:
                raise ValueError(f"Package not found: {package_spec}")

        # Fetch source
        print(f"Fetching source for {package.name}...")
        source_dir = self.fetcher.fetch(package, force=False)

        # Build
        print(f"Building {package.name}...")
        build_dir = self.config.build_dir / package.name
        env = self.builder._prepare_environment(package)
        self.builder.build(package, source_dir, build_dir)

        print(f"Successfully built {package.name}")

