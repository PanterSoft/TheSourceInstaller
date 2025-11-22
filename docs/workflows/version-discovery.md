# Automatic Version Discovery

TSI includes an automated system for discovering and adding new package versions to the package repository. This eliminates the need to manually track and add new versions for each package.

## Overview

The version discovery system:

1. **Automatically discovers** available versions from package sources (GitHub, git repos, etc.)
2. **Generates version definitions** based on existing package templates
3. **Adds new versions** to package files while preserving existing versions
4. **Runs automatically** via GitHub Actions on a weekly schedule
5. **Creates pull requests** when new versions are found

## How It Works

### Discovery Methods

The system supports multiple discovery methods:

#### 1. GitHub Releases/Tags

For packages hosted on GitHub, the system uses the GitHub API to discover:
- **Releases**: Published releases (preferred)
- **Tags**: Git tags if no releases are available

**Example**: For a package with source URL `https://github.com/user/repo/releases/download/v1.2.3/package-1.2.3.tar.gz`, the system will:
1. Extract the repository (`user/repo`)
2. Query GitHub API for releases/tags
3. Extract version numbers
4. Generate new version definitions

#### 2. Git Repository Tags

For git-based sources, the system can discover versions from repository tags.

#### 3. Website-Specific Handlers

Some websites require custom discovery logic. Currently supported:
- **curl.se**: Discovers curl versions from the download page

### Version Definition Generation

When a new version is discovered, the system:

1. Uses the **latest existing version** as a template
2. Replaces the version number in:
   - The `version` field
   - Source URLs (replaces version in URL)
   - Git tags (if applicable)
3. Preserves all other configuration:
   - Dependencies
   - Build system settings
   - Configure arguments
   - Environment variables

### Example

Given an existing package definition:

```json
{
  "name": "example",
  "version": "1.2.3",
  "source": {
    "type": "tarball",
    "url": "https://github.com/user/repo/releases/download/v1.2.3/example-1.2.3.tar.gz"
  },
  "dependencies": ["zlib"],
  "build_system": "cmake"
}
```

When version `1.2.4` is discovered, the system generates:

```json
{
  "version": "1.2.4",
  "source": {
    "type": "tarball",
    "url": "https://github.com/user/repo/releases/download/v1.2.4/example-1.2.4.tar.gz"
  },
  "dependencies": ["zlib"],
  "build_system": "cmake"
}
```

## Usage

### Command Line

#### Discover versions for a single package:

```bash
python3 scripts/discover-versions.py <package-name>
```

**Example:**
```bash
python3 scripts/discover-versions.py curl
```

#### Discover versions for all packages:

```bash
python3 scripts/discover-versions.py --all
```

#### Options:

- `--max-versions N`: Limit the number of versions discovered per package (default: all versions)
- `--dry-run`: Show what would be added without modifying files
- `--packages-dir PATH`: Specify custom packages directory

**Note:** By default, the script discovers **ALL available versions** for each package. Use `--max-versions` only if you want to limit the number of versions discovered.

**Examples:**
```bash
# Dry run to see what would be added (discovers ALL versions)
python3 scripts/discover-versions.py curl --dry-run

# Discover only 5 versions per package (limit)
python3 scripts/discover-versions.py --all --max-versions 5

# Discover ALL versions for all packages (default behavior)
python3 scripts/discover-versions.py --all

# Custom packages directory
python3 scripts/discover-versions.py git --packages-dir /path/to/packages
```

### GitHub Actions

The system includes a GitHub Actions workflow (`.github/workflows/discover-versions.yml`) that:

#### Automatic Schedule

- Runs **weekly on Mondays at 00:00 UTC**
- Discovers versions for all packages
- Creates pull requests when new versions are found

#### Manual Trigger

You can manually trigger the workflow:

1. Go to **Actions** → **Discover Package Versions**
2. Click **Run workflow**
3. Optionally specify:
   - Package name (leave empty for all packages)
   - Maximum versions per package

## Best Practices

### Package Definition Format

For best results, ensure your package definitions:

1. **Use semantic versioning** in URLs (e.g., `1.2.3` not `v1.2.3`)
2. **Include version in source URL** so it can be automatically replaced
3. **Use consistent URL patterns** across versions

### Example Good Format:

```json
{
  "name": "example",
  "version": "1.2.3",
  "source": {
    "type": "tarball",
    "url": "https://example.com/releases/example-1.2.3.tar.gz"
  }
}
```

### Example Problematic Format:

```json
{
  "name": "example",
  "version": "1.2.3",
  "source": {
    "type": "tarball",
    "url": "https://example.com/releases/latest.tar.gz"  // No version in URL
  }
}
```

## Limitations

1. **GitHub Rate Limiting**: The GitHub API has rate limits. When checking many packages, you may hit limits.

2. **Website-Specific Logic**: Some websites require custom discovery logic. Currently, only GitHub and curl.se are fully supported.

3. **Version Format**: The system works best with semantic versioning. Non-standard version formats may not be discovered correctly.

4. **URL Patterns**: The system needs version numbers in URLs to automatically generate new version definitions. Packages without versioned URLs require manual configuration.

## Extending Discovery

To add support for new discovery methods:

1. Add a new discovery function in `scripts/discover-versions.py`
2. Update `discover_package_versions()` to call the new function
3. Test with a sample package

**Example:**
```python
def discover_custom_website_versions(url: str) -> List[str]:
    """Discover versions from a custom website."""
    # Implement discovery logic
    return versions
```

## Integration with External Packages

The version discovery system works alongside the [External Package Configuration](external-packages.md) system:

- **External packages**: Projects maintain their own `.tsi.json` files
- **Version discovery**: Automatically finds and adds new versions
- **Both systems**: Can work together to keep packages up-to-date

## See Also

- [External Package Configuration](external-packages.md) - How projects can include their own package configs
- [Package Repository](../reference/package-repository.md) - Package format documentation
- [Scripts Reference](../reference/scripts.md) - Detailed script documentation

