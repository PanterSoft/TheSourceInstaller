"""Source code fetching for packages."""

import subprocess
import tarfile
import zipfile
from pathlib import Path
from typing import Optional
from urllib.parse import urlparse
from urllib.request import urlretrieve, urlopen
import shutil
import os


class SourceFetcher:
    """Fetches source code from various sources."""

    def __init__(self, source_dir: Path):
        """Initialize fetcher with source directory."""
        self.source_dir = source_dir
        self.source_dir.mkdir(parents=True, exist_ok=True)

    def fetch(self, package: "Package", force: bool = False) -> Path:
        """
        Fetch source code for a package.
        Returns the path to the extracted source directory.
        """
        source = package.source
        source_type = source.get("type", "git")

        package_source_dir = self.source_dir / package.name

        if package_source_dir.exists() and not force:
            print(f"Source already exists for {package.name}, skipping fetch")
            return package_source_dir

        if source_type == "git":
            return self._fetch_git(source, package_source_dir, force)
        elif source_type == "tarball":
            return self._fetch_tarball(source, package_source_dir, force)
        elif source_type == "zip":
            return self._fetch_zip(source, package_source_dir, force)
        elif source_type == "local":
            return self._fetch_local(source, package_source_dir)
        else:
            raise ValueError(f"Unknown source type: {source_type}")

    def _fetch_git(self, source: dict, target_dir: Path, force: bool) -> Path:
        """Fetch from git repository."""
        url = source.get("url")
        if not url:
            raise ValueError("Git source must include 'url'")

        # Check if git is available
        try:
            subprocess.run(["git", "--version"], check=True, capture_output=True)
        except (FileNotFoundError, subprocess.CalledProcessError):
            raise RuntimeError(
                "Git is required for git sources. Install git or use tarball/zip sources."
            )

        branch = source.get("branch", "main")
        tag = source.get("tag")
        commit = source.get("commit")

        if target_dir.exists():
            if force:
                shutil.rmtree(target_dir)
            else:
                # Update existing repo
                subprocess.run(
                    ["git", "fetch", "origin"],
                    cwd=target_dir,
                    check=True,
                    capture_output=True,
                )
                if tag:
                    subprocess.run(
                        ["git", "checkout", tag],
                        cwd=target_dir,
                        check=True,
                        capture_output=True,
                    )
                elif commit:
                    subprocess.run(
                        ["git", "checkout", commit],
                        cwd=target_dir,
                        check=True,
                        capture_output=True,
                    )
                else:
                    subprocess.run(
                        ["git", "checkout", branch],
                        cwd=target_dir,
                        check=True,
                        capture_output=True,
                    )
                return target_dir

        # Clone repository
        subprocess.run(
            ["git", "clone", "--depth", "1", url, str(target_dir)],
            check=True,
            capture_output=True,
        )

        if tag:
            subprocess.run(
                ["git", "checkout", tag],
                cwd=target_dir,
                check=True,
                capture_output=True,
            )
        elif commit:
            subprocess.run(
                ["git", "checkout", commit],
                cwd=target_dir,
                check=True,
                capture_output=True,
            )
        elif branch != "main":
            subprocess.run(
                ["git", "checkout", branch],
                cwd=target_dir,
                check=True,
                capture_output=True,
            )

        return target_dir

    def _fetch_tarball(self, source: dict, target_dir: Path, force: bool) -> Path:
        """Fetch and extract tarball."""
        url = source.get("url")
        if not url:
            raise ValueError("Tarball source must include 'url'")

        # Download tarball
        parsed = urlparse(url)
        filename = Path(parsed.path).name
        if not filename or filename == "/":
            filename = "source.tar.gz"
        tarball_path = self.source_dir / filename

        if not tarball_path.exists() or force:
            self._download_file(url, tarball_path)

        # Extract
        if target_dir.exists() and force:
            shutil.rmtree(target_dir)

        if not target_dir.exists():
            target_dir.mkdir(parents=True, exist_ok=True)
            with tarfile.open(tarball_path) as tar:
                tar.extractall(target_dir)

            # Move contents if archive has a single top-level directory
            extracted_items = list(target_dir.iterdir())
            if len(extracted_items) == 1 and extracted_items[0].is_dir():
                temp_dir = target_dir / "temp"
                extracted_items[0].rename(temp_dir)
                for item in temp_dir.iterdir():
                    item.rename(target_dir / item.name)
                temp_dir.rmdir()

        return target_dir

    def _fetch_zip(self, source: dict, target_dir: Path, force: bool) -> Path:
        """Fetch and extract zip file."""
        url = source.get("url")
        if not url:
            raise ValueError("Zip source must include 'url'")

        # Download zip
        parsed = urlparse(url)
        filename = Path(parsed.path).name
        if not filename or filename == "/":
            filename = "source.zip"
        zip_path = self.source_dir / filename

        if not zip_path.exists() or force:
            self._download_file(url, zip_path)

        # Extract
        if target_dir.exists() and force:
            shutil.rmtree(target_dir)

        if not target_dir.exists():
            target_dir.mkdir(parents=True, exist_ok=True)
            with zipfile.ZipFile(zip_path) as zip_file:
                zip_file.extractall(target_dir)

        return target_dir

    def _fetch_local(self, source: dict, target_dir: Path) -> Path:
        """Use local source directory."""
        path = Path(source.get("path", "")).expanduser()
        if not path.exists():
            raise ValueError(f"Local source path does not exist: {path}")

        if target_dir.exists():
            shutil.rmtree(target_dir)

        shutil.copytree(path, target_dir)
        return target_dir

    def _download_file(self, url: str, target_path: Path):
        """Download a file using Python standard library (no curl required)."""
        print(f"Downloading {url}...")
        try:
            # Try using curl if available (faster, supports redirects better)
            result = subprocess.run(
                ["curl", "-L", "-f", "-o", str(target_path), url],
                capture_output=True,
                timeout=300,
            )
            if result.returncode == 0:
                return
        except (FileNotFoundError, subprocess.TimeoutExpired):
            pass

        # Fallback to Python urllib (handles redirects automatically)
        try:
            from urllib.request import Request, urlopen
            from urllib.error import HTTPError, URLError

            req = Request(url)
            req.add_header("User-Agent", "TSI/0.1.0")

            with urlopen(req, timeout=300) as response:
                with open(target_path, "wb") as f:
                    shutil.copyfileobj(response, f)
        except (HTTPError, URLError, Exception) as e:
            raise RuntimeError(f"Failed to download {url}: {e}")

