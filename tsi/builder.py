"""Build system abstraction for compiling packages."""

import os
import sys
import subprocess
import shutil
from pathlib import Path
from typing import List, Dict, Optional

from tsi.package import Package
from tsi.config import Config


class Builder:
    """Builds packages from source using various build systems."""

    def __init__(self, config: Config):
        """Initialize builder with configuration."""
        self.config = config

    def build(self, package: Package, source_dir: Path, build_dir: Path) -> Path:
        """
        Build a package from source.
        Returns the path to the built package.
        """
        build_dir.mkdir(parents=True, exist_ok=True)

        # Apply patches
        if package.patches:
            self._apply_patches(source_dir, package.patches)

        # Set up environment
        env = self._prepare_environment(package)

        # Run custom build commands if specified
        if package.build_commands:
            return self._build_custom(package, source_dir, build_dir, env)

        # Use build system
        build_system = package.build_system.lower()

        if build_system == "autotools":
            return self._build_autotools(package, source_dir, build_dir, env)
        elif build_system == "cmake":
            return self._build_cmake(package, source_dir, build_dir, env)
        elif build_system == "make":
            return self._build_make(package, source_dir, build_dir, env)
        elif build_system == "meson":
            return self._build_meson(package, source_dir, build_dir, env)
        elif build_system == "cargo":
            return self._build_cargo(package, source_dir, build_dir, env)
        elif build_system == "python":
            return self._build_python(package, source_dir, build_dir, env)
        else:
            raise ValueError(f"Unknown build system: {build_system}")

    def _prepare_environment(self, package: Package) -> Dict[str, str]:
        """Prepare environment variables for building."""
        env = os.environ.copy()
        env.update(self.config.env)
        env.update(package.env)
        return env

    def _apply_patches(self, source_dir: Path, patches: List[str]):
        """Apply patches to source code."""
        for patch in patches:
            patch_path = Path(patch).expanduser()
            if not patch_path.exists():
                print(f"Warning: Patch file not found: {patch_path}")
                continue

            subprocess.run(
                ["patch", "-p1", "-i", str(patch_path)],
                cwd=source_dir,
                check=True,
                capture_output=True,
            )

    def _build_custom(self, package: Package, source_dir: Path, build_dir: Path, env: Dict) -> Path:
        """Build using custom commands."""
        for cmd in package.build_commands:
            if isinstance(cmd, str):
                subprocess.run(
                    cmd,
                    shell=True,
                    cwd=source_dir,
                    env=env,
                    check=True,
                )
            else:
                subprocess.run(
                    cmd,
                    cwd=source_dir,
                    env=env,
                    check=True,
                )
        return build_dir

    def _build_autotools(self, package: Package, source_dir: Path, build_dir: Path, env: Dict) -> Path:
        """Build using autotools (./configure && make && make install)."""
        # Check if configure script exists
        configure_script = source_dir / "configure"
        if not configure_script.exists():
            # Try to generate configure script
            if (source_dir / "configure.ac").exists() or (source_dir / "configure.in").exists():
                subprocess.run(
                    ["autoreconf", "-fiv"],
                    cwd=source_dir,
                    env=env,
                    check=True,
                    capture_output=True,
                )

        # Configure
        configure_cmd = ["./configure", f"--prefix={self.config.install_dir}"]
        configure_cmd.extend(package.configure_args)
        subprocess.run(
            configure_cmd,
            cwd=source_dir,
            env=env,
            check=True,
        )

        # Make
        make_cmd = ["make"] + package.make_args
        subprocess.run(
            make_cmd,
            cwd=source_dir,
            env=env,
            check=True,
        )

        return build_dir

    def _build_cmake(self, package: Package, source_dir: Path, build_dir: Path, env: Dict) -> Path:
        """Build using CMake."""
        cmake_cmd = [
            "cmake",
            "-S", str(source_dir),
            "-B", str(build_dir),
            f"-DCMAKE_INSTALL_PREFIX={self.config.install_dir}",
        ]
        cmake_cmd.extend(package.cmake_args)

        subprocess.run(
            cmake_cmd,
            env=env,
            check=True,
        )

        make_cmd = ["cmake", "--build", str(build_dir)] + package.make_args
        subprocess.run(
            make_cmd,
            env=env,
            check=True,
        )

        return build_dir

    def _build_make(self, package: Package, source_dir: Path, build_dir: Path, env: Dict) -> Path:
        """Build using plain Makefile."""
        make_cmd = ["make"] + package.make_args
        subprocess.run(
            make_cmd,
            cwd=source_dir,
            env=env,
            check=True,
        )
        return build_dir

    def _build_meson(self, package: Package, source_dir: Path, build_dir: Path, env: Dict) -> Path:
        """Build using Meson."""
        # Setup
        subprocess.run(
            ["meson", "setup", str(build_dir), str(source_dir), f"--prefix={self.config.install_dir}"],
            env=env,
            check=True,
        )

        # Compile
        subprocess.run(
            ["meson", "compile", "-C", str(build_dir)],
            env=env,
            check=True,
        )

        return build_dir

    def _build_cargo(self, package: Package, source_dir: Path, build_dir: Path, env: Dict) -> Path:
        """Build Rust package using Cargo."""
        subprocess.run(
            ["cargo", "build", "--release"],
            cwd=source_dir,
            env=env,
            check=True,
        )
        return build_dir

    def _build_python(self, package: Package, source_dir: Path, build_dir: Path, env: Dict) -> Path:
        """Build Python package."""
        subprocess.run(
            [sys.executable, "-m", "pip", "install", "--prefix", str(self.config.install_dir), "."],
            cwd=source_dir,
            env=env,
            check=True,
        )
        return build_dir

    def install(self, package: Package, source_dir: Path, build_dir: Path, env: Dict):
        """Install a built package."""
        if package.install_commands:
            # Use custom install commands
            for cmd in package.install_commands:
                if isinstance(cmd, str):
                    subprocess.run(
                        cmd,
                        shell=True,
                        cwd=source_dir,
                        env=env,
                        check=True,
                    )
                else:
                    subprocess.run(
                        cmd,
                        cwd=source_dir,
                        env=env,
                        check=True,
                    )
            return

        # Use build system install
        build_system = package.build_system.lower()

        if build_system == "autotools":
            subprocess.run(
                ["make", "install"],
                cwd=source_dir,
                env=env,
                check=True,
            )
        elif build_system == "cmake":
            subprocess.run(
                ["cmake", "--install", str(build_dir)],
                env=env,
                check=True,
            )
        elif build_system == "meson":
            subprocess.run(
                ["meson", "install", "-C", str(build_dir)],
                env=env,
                check=True,
            )
        elif build_system == "make":
            # Try to find install target
            subprocess.run(
                ["make", "install", f"PREFIX={self.config.install_dir}"],
                cwd=source_dir,
                env=env,
                check=True,
            )
        elif build_system == "cargo":
            subprocess.run(
                ["cargo", "install", "--path", ".", "--root", str(self.config.install_dir)],
                cwd=source_dir,
                env=env,
                check=True,
            )
        elif build_system == "python":
            # Already installed during build
            pass

