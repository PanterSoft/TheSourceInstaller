#!/usr/bin/env python3
"""
Discover and add new versions to TSI package definitions.

This script automatically discovers available versions for packages and adds them
to the package definitions. It supports multiple discovery methods:

1. GitHub Releases/Tags - For GitHub-hosted projects
2. URL Pattern Matching - For tarball URLs with version patterns
3. Git Tags - For git repositories

Usage:
    python3 discover-versions.py <package-name> [--max-versions N] [--dry-run]
    python3 discover-versions.py --all [--max-versions N] [--dry-run]
"""

import json
import sys
import os
import re
import argparse
import urllib.request
import urllib.error
import urllib.parse
from pathlib import Path
from typing import List, Dict, Optional, Tuple
from datetime import datetime


def load_json(filepath):
    """Load and parse a JSON file."""
    with open(filepath, 'r') as f:
        return json.load(f)


def save_json(filepath, data):
    """Save data as formatted JSON."""
    with open(filepath, 'w') as f:
        json.dump(data, f, indent=2)
        f.write('\n')


def fetch_url(url: str, headers: Optional[Dict] = None, token: Optional[str] = None) -> Optional[str]:
    """Fetch content from a URL."""
    try:
        req_headers = headers.copy() if headers else {}
        if token:
            req_headers['Authorization'] = f'token {token}'

        req = urllib.request.Request(url, headers=req_headers)
        with urllib.request.urlopen(req, timeout=30) as response:
            return response.read().decode('utf-8')
    except urllib.error.HTTPError as e:
        if e.code == 403:
            # Rate limit or forbidden - try to get rate limit info
            rate_limit = e.headers.get('X-RateLimit-Remaining', 'unknown')
            if rate_limit == '0':
                print(f"Warning: GitHub API rate limit exceeded for {url}", file=sys.stderr)
            else:
                print(f"Warning: HTTP 403 for {url} (rate limit remaining: {rate_limit})", file=sys.stderr)
        else:
            print(f"Warning: HTTP {e.code} for {url}: {e}", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Warning: Failed to fetch {url}: {e}", file=sys.stderr)
        return None


def discover_github_versions(repo: str, max_versions: Optional[int] = None, token: Optional[str] = None) -> List[str]:
    """
    Discover versions from GitHub releases and tags.

    Args:
        repo: Repository in format 'owner/repo' or full URL
        max_versions: Maximum number of versions to return (None = all versions)

    Returns:
        List of version strings (sorted, newest first)
    """
    # Extract owner/repo from URL if needed
    if 'github.com' in repo:
        match = re.search(r'github\.com/([^/]+)/([^/]+)', repo)
        if match:
            repo = f"{match.group(1)}/{match.group(2)}"
        else:
            return []

    versions = []

    # Try GitHub API for releases (with pagination)
    page = 1
    per_page = 100  # GitHub API max per page

    while True:
        releases_url = f"https://api.github.com/repos/{repo}/releases?page={page}&per_page={per_page}"
        content = fetch_url(releases_url, headers={"Accept": "application/vnd.github.v3+json"}, token=token)

        if not content:
            break

        try:
            releases = json.loads(content)
            if not releases:  # No more pages
                break

            for release in releases:
                tag = release.get('tag_name', '')
                # Remove 'v' prefix if present
                version = tag.lstrip('v') if tag.startswith('v') else tag
                if version:
                    versions.append(version)

                # Check if we've reached max_versions limit
                if max_versions and len(versions) >= max_versions:
                    return versions[:max_versions]

            page += 1
        except json.JSONDecodeError:
            break

    # If no releases, try tags (with pagination)
    if not versions:
        page = 1
        while True:
            tags_url = f"https://api.github.com/repos/{repo}/tags?page={page}&per_page={per_page}"
            content = fetch_url(tags_url, headers={"Accept": "application/vnd.github.v3+json"}, token=token)

            if not content:
                break

            try:
                tags = json.loads(content)
                if not tags:  # No more pages
                    break

                for tag in tags:
                    name = tag.get('name', '')
                    version = name.lstrip('v') if name.startswith('v') else name
                    if version:
                        versions.append(version)

                    # Check if we've reached max_versions limit
                    if max_versions and len(versions) >= max_versions:
                        return versions[:max_versions]

                page += 1
            except json.JSONDecodeError:
                break

    return versions


def discover_url_pattern_versions(base_url: str, version_pattern: str, max_versions: int = 10) -> List[str]:
    """
    Discover versions by checking URL patterns.

    Args:
        base_url: Base URL with {version} placeholder
        version_pattern: Regex pattern to extract versions from URLs
        max_versions: Maximum number of versions to return

    Returns:
        List of version strings
    """
    # This is a simplified version - in practice, you'd need to:
    # 1. Fetch a directory listing page
    # 2. Parse HTML/links
    # 3. Extract versions using the pattern

    # For now, return empty list - this would need more sophisticated implementation
    # based on the specific website structure
    return []


def discover_curl_versions(max_versions: Optional[int] = None) -> List[str]:
    """Discover curl versions from curl.se."""
    # curl.se has a releases page
    content = fetch_url("https://curl.se/download/")
    if not content:
        return []

    # Extract version numbers from the page
    versions = []
    pattern = r'curl-(\d+\.\d+\.\d+)\.tar\.gz'
    matches = re.findall(pattern, content)
    versions = sorted(set(matches), reverse=True)

    if max_versions:
        return versions[:max_versions]
    return versions


def extract_version_from_url(url: str) -> Optional[str]:
    """Extract version number from a URL."""
    # Common patterns: version-1.2.3, v1.2.3, 1.2.3, etc.
    patterns = [
        r'[vV]?(\d+\.\d+\.\d+(?:\.\d+)?(?:-[a-zA-Z0-9]+)?)',
        r'(\d+\.\d+\.\d+(?:\.\d+)?(?:-[a-zA-Z0-9]+)?)',
        r'(\d+\.\d+(?:\.\d+)?(?:-[a-zA-Z0-9]+)?)',
    ]

    for pattern in patterns:
        match = re.search(pattern, url)
        if match:
            return match.group(1)
    return None


def generate_version_definition(base_version: Dict, new_version: str) -> Dict:
    """
    Generate a new version definition based on a base version.

    Args:
        base_version: Existing version definition to use as template
        new_version: New version string

    Returns:
        New version definition dictionary
    """
    import copy
    new_def = copy.deepcopy(base_version)
    new_def['version'] = new_version

    # Update source URL if it contains version
    if 'source' in new_def and 'url' in new_def['source']:
        url = new_def['source']['url']
        old_version = base_version.get('version', '')

        # Try multiple replacement strategies
        if old_version in url:
            # Direct replacement
            new_def['source']['url'] = url.replace(old_version, new_version)
        elif f"v{old_version}" in url:
            # Version with 'v' prefix
            new_def['source']['url'] = url.replace(f"v{old_version}", f"v{new_version}")
        else:
            # Try to find and replace version pattern in URL
            # Match version patterns like: 1.2.3, v1.2.3, 1.2.3.4, etc.
            version_patterns = [
                r'\d+\.\d+\.\d+\.\d+',  # 1.2.3.4
                r'v\d+\.\d+\.\d+\.\d+',  # v1.2.3.4
                r'\d+\.\d+\.\d+',  # 1.2.3
                r'v\d+\.\d+\.\d+',  # v1.2.3
                r'\d+\.\d+',  # 1.2
                r'v\d+\.\d+',  # v1.2
            ]

            for pattern in version_patterns:
                if re.search(pattern, url):
                    # Replace the first match
                    new_def['source']['url'] = re.sub(pattern, new_version, url, count=1)
                    break

    # Update git tag if present
    if 'source' in new_def and new_def['source'].get('type') == 'git':
        if 'tag' in new_def['source']:
            old_tag = new_def['source']['tag']
            # Preserve 'v' prefix if it exists
            if old_tag.startswith('v'):
                new_def['source']['tag'] = f"v{new_version}"
            else:
                new_def['source']['tag'] = new_version
        elif 'branch' in new_def['source']:
            # For git, might want to use tag instead
            pass

    return new_def


def discover_package_versions(package_file: Path, max_versions: Optional[int] = None, token: Optional[str] = None) -> List[str]:
    """
    Discover available versions for a package.

    Args:
        package_file: Path to package JSON file
        max_versions: Maximum number of versions to discover

    Returns:
        List of discovered version strings
    """
    if not package_file.exists():
        return []

    pkg = load_json(package_file)

    # Get the latest version as template
    if 'versions' in pkg and pkg['versions']:
        latest = pkg['versions'][0]
    elif 'version' in pkg:
        latest = pkg
    else:
        return []

    source = latest.get('source', {})
    source_type = source.get('type', 'tarball')
    source_url = source.get('url', '')

    discovered = []

    # GitHub discovery
    if 'github.com' in source_url:
        repo_match = re.search(r'github\.com/([^/]+)/([^/]+)', source_url)
        if repo_match:
            repo = f"{repo_match.group(1)}/{repo_match.group(2)}"
            discovered = discover_github_versions(repo, max_versions, token)

    # Git repository discovery
    elif source_type == 'git' and 'github.com' in source_url:
        repo_match = re.search(r'github\.com/([^/]+)/([^/]+)', source_url)
        if repo_match:
            repo = f"{repo_match.group(1)}/{repo_match.group(2)}"
            discovered = discover_github_versions(repo, max_versions, token)

    # Special cases for specific websites
    elif 'curl.se' in source_url:
        discovered = discover_curl_versions(max_versions)

    # URL pattern discovery (simplified - would need website-specific logic)
    # For now, we'll focus on GitHub which covers most packages

    return discovered


def add_versions_to_package(package_file: Path, new_versions: List[str], dry_run: bool = False) -> Tuple[int, int]:
    """
    Add new versions to a package file.

    Args:
        package_file: Path to package JSON file
        new_versions: List of version strings to add
        dry_run: If True, don't actually modify files

    Returns:
        Tuple of (added_count, skipped_count)
    """
    if not package_file.exists():
        print(f"Error: Package file not found: {package_file}", file=sys.stderr)
        return 0, 0

    pkg = load_json(package_file)

    # Convert to multi-version format if needed
    if 'versions' not in pkg:
        if 'version' in pkg:
            existing_version = {k: v for k, v in pkg.items() if k != 'name'}
            pkg = {
                "name": pkg.get('name'),
                "versions": [existing_version]
            }
        else:
            print(f"Error: No version information found in {package_file}", file=sys.stderr)
            return 0, 0

    # Get existing versions
    existing_versions = {v.get('version') for v in pkg.get('versions', [])}

    # Get base version template (use latest)
    base_version = pkg['versions'][0] if pkg['versions'] else {}

    added_count = 0
    skipped_count = 0
    new_version_defs = []

    for version in new_versions:
        if version in existing_versions:
            skipped_count += 1
            continue

        # Generate new version definition
        new_def = generate_version_definition(base_version, version)
        new_version_defs.append(new_def)
        added_count += 1

    if new_version_defs:
        # Insert new versions at the beginning (latest first)
        pkg['versions'] = new_version_defs + pkg['versions']

        if not dry_run:
            save_json(package_file, pkg)
            print(f"Added {added_count} new version(s) to {package_file.name}")
        else:
            print(f"[DRY RUN] Would add {added_count} new version(s) to {package_file.name}")
            for v in new_version_defs:
                print(f"  - {v['version']}")

    if skipped_count > 0:
        print(f"Skipped {skipped_count} version(s) (already exist)")

    return added_count, skipped_count


def main():
    parser = argparse.ArgumentParser(
        description='Discover and add new versions to TSI package definitions'
    )
    parser.add_argument(
        'package',
        nargs='?',
        help='Package name to update (or --all for all packages)'
    )
    parser.add_argument(
        '--all',
        action='store_true',
        help='Update all packages'
    )
    parser.add_argument(
        '--max-versions',
        type=int,
        default=None,
        help='Maximum number of versions to discover per package (default: all versions)'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be added without modifying files'
    )
    parser.add_argument(
        '--packages-dir',
        default='packages',
        help='Path to packages directory (default: packages)'
    )
    parser.add_argument(
        '--github-token',
        default=None,
        help='GitHub token for API authentication (default: from GITHUB_TOKEN env var)'
    )

    args = parser.parse_args()

    # Get GitHub token from argument or environment variable
    github_token = args.github_token or os.getenv('GITHUB_TOKEN')

    packages_dir = Path(args.packages_dir)
    if not packages_dir.exists():
        print(f"Error: Packages directory not found: {packages_dir}", file=sys.stderr)
        sys.exit(1)

    if args.all:
        # Process all packages
        package_files = list(packages_dir.glob('*.json'))
        total_added = 0
        total_skipped = 0

        for pkg_file in package_files:
            # Skip README if it exists as JSON
            if pkg_file.name == 'README.json':
                continue

            print(f"\nProcessing {pkg_file.name}...")
            versions = discover_package_versions(pkg_file, args.max_versions, github_token)

            if versions:
                added, skipped = add_versions_to_package(pkg_file, versions, args.dry_run)
                total_added += added
                total_skipped += skipped
            else:
                print(f"  No new versions discovered")

        print(f"\nSummary: Added {total_added} version(s), skipped {total_skipped} version(s)")

    elif args.package:
        # Process single package
        pkg_file = packages_dir / f"{args.package}.json"
        if not pkg_file.exists():
            print(f"Error: Package file not found: {pkg_file}", file=sys.stderr)
            sys.exit(1)

        print(f"Discovering versions for {args.package}...")
        versions = discover_package_versions(pkg_file, args.max_versions, github_token)

        if versions:
            print(f"Discovered {len(versions)} version(s): {', '.join(versions[:5])}{'...' if len(versions) > 5 else ''}")
            added, skipped = add_versions_to_package(pkg_file, versions, args.dry_run)
            print(f"Added {added}, skipped {skipped}")
        else:
            print("No new versions discovered")
            sys.exit(1)

    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    main()

