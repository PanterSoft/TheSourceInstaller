#!/usr/bin/env python3
"""
Simple Python-based installation script for TSI.
Works with minimal requirements - only needs Python 3.8+ standard library.
"""

import os
import sys
import shutil
import subprocess
from pathlib import Path


def main():
    """Install TSI with minimal requirements."""
    print("Installing TSI (TheSourceInstaller)...")

    # Check Python version
    if sys.version_info < (3, 8):
        print("Error: Python 3.8 or higher is required")
        sys.exit(1)

    print(f"Found Python {sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}")

    # Determine installation prefix
    prefix = os.environ.get("PREFIX", os.path.expanduser("~/.local"))
    install_dir = Path(prefix) / "bin"
    install_dir.mkdir(parents=True, exist_ok=True)

    print(f"Installation prefix: {prefix}")
    print(f"Binary directory: {install_dir}")

    # Check if setuptools is available
    try:
        import setuptools
    except ImportError:
        print("Installing setuptools (required for installation)...")
        try:
            import urllib.request
            import tempfile
            import zipfile

            # Download setuptools bootstrap
            with tempfile.TemporaryDirectory() as tmpdir:
                ez_setup = Path(tmpdir) / "ez_setup.py"
                urllib.request.urlretrieve(
                    "https://bootstrap.pypa.io/ez_setup.py",
                    ez_setup
                )
                subprocess.run([sys.executable, str(ez_setup)], check=True)
        except Exception as e:
            print(f"Error installing setuptools: {e}")
            print("Please install setuptools manually: python3 -m pip install setuptools")
            sys.exit(1)

    # Install TSI
    print("Installing TSI...")
    try:
        # Try using setuptools
        subprocess.run(
            [sys.executable, "setup.py", "install", "--prefix", prefix, "--user"],
            check=True,
        )
    except subprocess.CalledProcessError:
        # Fallback: simple file copy installation
        print("Using simple file copy installation...")
        tsi_dir = Path(__file__).parent / "tsi"
        site_packages = Path.home() / ".local" / "lib" / f"python{sys.version_info.major}.{sys.version_info.minor}" / "site-packages"
        site_packages.mkdir(parents=True, exist_ok=True)
        target_dir = site_packages / "tsi"
        if target_dir.exists():
            shutil.rmtree(target_dir)
        shutil.copytree(tsi_dir, target_dir)

        # Create wrapper script
        tsi_bin = install_dir / "tsi"
        with open(tsi_bin, "w") as f:
            f.write(f"""#!/usr/bin/env python3
import sys
from pathlib import Path
sys.path.insert(0, str(Path.home() / ".local" / "lib" / "python{sys.version_info.major}.{sys.version_info.minor}" / "site-packages"))
from tsi.cli import main
if __name__ == "__main__":
    main()
""")
        tsi_bin.chmod(0o755)

    print("")
    print("TSI installed successfully!")
    print("")
    print(f"Add to your PATH:")
    print(f'  export PATH="{install_dir}:$PATH"')
    print("")
    print("Or add to your shell profile (~/.bashrc, ~/.zshrc, etc.):")
    print(f'  echo \'export PATH="{install_dir}:\\$PATH"\' >> ~/.bashrc')
    print("")
    print("Then run: tsi --help")


if __name__ == "__main__":
    main()

