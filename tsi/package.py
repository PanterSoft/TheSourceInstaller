"""Package manifest and definition handling."""

import json
from pathlib import Path
from typing import Dict, List, Optional


class Package:
    """Represents a package definition."""

    def __init__(self, manifest: Dict):
        """Initialize package from manifest dictionary."""
        self.name = manifest.get("name")
        self.version = manifest.get("version", "latest")
        self.description = manifest.get("description", "")
        self.source = manifest.get("source", {})
        self.dependencies = manifest.get("dependencies", [])
        self.build_dependencies = manifest.get("build_dependencies", [])
        self.build_system = manifest.get("build_system", "autotools")
        self.build_commands = manifest.get("build_commands", [])
        self.configure_args = manifest.get("configure_args", [])
        self.cmake_args = manifest.get("cmake_args", [])
        self.make_args = manifest.get("make_args", [])
        self.install_commands = manifest.get("install_commands", [])
        self.env = manifest.get("env", {})
        self.patches = manifest.get("patches", [])

        if not self.name:
            raise ValueError("Package manifest must include a 'name' field")

    @classmethod
    def from_file(cls, path: Path) -> "Package":
        """Load package from JSON manifest file."""
        with open(path, "r") as f:
            manifest = json.load(f)
        return cls(manifest)

    @classmethod
    def from_string(cls, content: str) -> "Package":
        """Load package from JSON string."""
        manifest = json.loads(content)
        return cls(manifest)

    def to_dict(self) -> Dict:
        """Convert package to dictionary."""
        return {
            "name": self.name,
            "version": self.version,
            "description": self.description,
            "source": self.source,
            "dependencies": self.dependencies,
            "build_dependencies": self.build_dependencies,
            "build_system": self.build_system,
            "build_commands": self.build_commands,
            "configure_args": self.configure_args,
            "cmake_args": self.cmake_args,
            "make_args": self.make_args,
            "install_commands": self.install_commands,
            "env": self.env,
            "patches": self.patches,
        }

    def get_all_dependencies(self) -> List[str]:
        """Get all dependencies including build dependencies."""
        return list(set(self.dependencies + self.build_dependencies))

