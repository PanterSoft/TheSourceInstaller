"""Configuration management for TSI."""

import os
from pathlib import Path


class Config:
    """TSI configuration."""

    def __init__(self):
        """Initialize configuration with defaults."""
        self.prefix = Path.home() / ".tsi"
        self.build_dir = self.prefix / "build"
        self.install_dir = self.prefix / "install"
        self.source_dir = self.prefix / "sources"
        self.db_dir = self.prefix / "db"
        self.repo_dir = self.prefix / "repos"

        # Ensure directories exist
        self.prefix.mkdir(parents=True, exist_ok=True)
        self.build_dir.mkdir(parents=True, exist_ok=True)
        self.install_dir.mkdir(parents=True, exist_ok=True)
        self.source_dir.mkdir(parents=True, exist_ok=True)
        self.db_dir.mkdir(parents=True, exist_ok=True)
        self.repo_dir.mkdir(parents=True, exist_ok=True)

        # Environment variables
        self.env = {
            "PATH": f"{self.install_dir / 'bin'}:{os.environ.get('PATH', '')}",
            "PKG_CONFIG_PATH": f"{self.install_dir / 'lib' / 'pkgconfig'}:{os.environ.get('PKG_CONFIG_PATH', '')}",
            "LD_LIBRARY_PATH": f"{self.install_dir / 'lib'}:{os.environ.get('LD_LIBRARY_PATH', '')}",
            "CPPFLAGS": f"-I{self.install_dir / 'include'} {os.environ.get('CPPFLAGS', '')}",
            "LDFLAGS": f"-L{self.install_dir / 'lib'} {os.environ.get('LDFLAGS', '')}",
        }

