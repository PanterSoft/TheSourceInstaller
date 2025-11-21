"""Package installation database."""

import json
from pathlib import Path
from typing import Dict, List, Set
from datetime import datetime

from tsi.package import Package


class Database:
    """Tracks installed packages."""

    def __init__(self, db_dir: Path):
        """Initialize database with directory."""
        self.db_dir = db_dir
        self.db_dir.mkdir(parents=True, exist_ok=True)
        self.db_file = self.db_dir / "installed.json"
        self._installed: Dict[str, Dict] = self._load()

    def _load(self) -> Dict[str, Dict]:
        """Load database from disk."""
        if not self.db_file.exists():
            return {}

        try:
            with open(self.db_file, "r") as f:
                return json.load(f)
        except Exception:
            return {}

    def _save(self):
        """Save database to disk."""
        with open(self.db_file, "w") as f:
            json.dump(self._installed, f, indent=2)

    def is_installed(self, package_name: str) -> bool:
        """Check if a package is installed."""
        return package_name in self._installed

    def add_package(self, package: Package, install_path: Path):
        """Record an installed package."""
        self._installed[package.name] = {
            "name": package.name,
            "version": package.version,
            "install_path": str(install_path),
            "installed_at": datetime.now().isoformat(),
            "dependencies": package.dependencies,
        }
        self._save()

    def remove_package(self, package_name: str):
        """Remove a package from database."""
        if package_name in self._installed:
            del self._installed[package_name]
            self._save()

    def list_installed(self) -> List[str]:
        """List all installed packages."""
        return list(self._installed.keys())

    def get_package_info(self, package_name: str) -> Dict:
        """Get information about an installed package."""
        return self._installed.get(package_name, {})

    def get_all_installed(self) -> Set[str]:
        """Get set of all installed package names."""
        return set(self._installed.keys())

