# External Package Configuration

This document describes how projects can include their own TSI package configuration in their repositories, similar to how Homebrew handles casks and formulas.

## Overview

Projects can include a `.tsi.json` file in their repository root that defines how to build and package their software using TSI. When this file is updated, a GitHub Actions workflow can automatically create a pull request to update the TSI package repository.

## Package Configuration Format

Projects should include a `.tsi.json` file in their repository root with the following format:

```json
{
  "name": "package-name",
  "version": "1.2.3",
  "description": "Package description",
  "source": {
    "type": "tarball",
    "url": "https://example.com/releases/package-1.2.3.tar.gz"
  },
  "dependencies": ["zlib", "openssl"],
  "build_dependencies": ["pkg-config", "cmake"],
  "build_system": "cmake",
  "cmake_args": ["-DBUILD_SHARED_LIBS=ON"],
  "env": {}
}
```

### Required Fields

- `name`: Package name (must match the package name in TSI repository)
- `version`: Package version (semantic versioning recommended)
- `description`: Brief description of the package
- `source`: Source information (see below)

### Source Types

The `source` object supports the following types:

1. **tarball**: Download and extract a tarball
   ```json
   {
     "type": "tarball",
     "url": "https://example.com/releases/package-1.2.3.tar.gz"
   }
   ```

2. **git**: Clone from a git repository
   ```json
   {
     "type": "git",
     "url": "https://github.com/user/repo.git",
     "branch": "main",
     "tag": "v1.2.3"
   }
   ```

3. **zip**: Download and extract a zip file
   ```json
   {
     "type": "zip",
     "url": "https://example.com/releases/package-1.2.3.zip"
   }
   ```

### Optional Fields

- `dependencies`: Array of runtime dependencies (package names)
- `build_dependencies`: Array of build-time dependencies (package names)
- `build_system`: Build system type (`autotools`, `cmake`, `meson`, `make`, `cargo`, `custom`)
- `configure_args`: Arguments for `./configure` (autotools)
- `cmake_args`: Arguments for `cmake`
- `make_args`: Arguments for `make`
- `env`: Environment variables as key-value pairs
- `patches`: Array of patch file URLs or paths

## Integration with TSI Repository

The TSI package repository uses a multi-version format where each package can have multiple versions:

```json
{
  "name": "package-name",
  "versions": [
    {
      "version": "1.2.3",
      "description": "...",
      "source": {...},
      ...
    },
    {
      "version": "1.2.2",
      "description": "...",
      "source": {...},
      ...
    }
  ]
}
```

When a project updates its `.tsi.json` file, the GitHub Actions workflow will:

1. Fetch the `.tsi.json` file from the project repository
2. Merge it into the existing package file in `packages/`
3. Add the new version to the `versions` array (or create the package if it doesn't exist)
4. **Preserve all existing versions** - old versions are kept in the `versions` array
5. Create a pull request with the changes

### Multiple Versions

The TSI package repository maintains **multiple versions** of each package in a `versions` array. This allows users to install specific versions:

```bash
# Install latest version
tsi install git

# Install specific version
tsi install git@2.45.0
tsi install git@2.44.0
```

When a new version is added via the external package workflow:
- The new version is **added** to the `versions` array
- All existing versions are **preserved**
- The new version is placed at the beginning of the array (latest first)
- If the version already exists, it is **updated** with the new definition

This ensures backward compatibility and allows users to install any previously available version.

## Workflow

### For Project Maintainers

1. Add a `.tsi.json` file to your repository root
2. When you release a new version, update the `version` and `source.url` fields
3. Commit and push the changes
4. Optionally, trigger the TSI workflow manually or wait for automatic detection

### For TSI Repository Maintainers

The GitHub Actions workflow (`sync-external-packages.yml`) can be triggered:

1. **Manually**: Via GitHub Actions UI with parameters:
   - `repo`: Repository URL (e.g., `https://github.com/user/repo`)
   - `branch`: Branch name (default: `main`)
   - `path`: Path to `.tsi.json` (default: `.tsi.json`)

2. **Via Webhook**: Projects can set up webhooks to trigger updates on release

3. **Scheduled**: Periodic checks for updates (optional)

## Example: Git Project

If the Git project wanted to include its TSI configuration, it would add a `.tsi.json` file:

```json
{
  "name": "git",
  "version": "2.45.0",
  "description": "Distributed version control system",
  "source": {
    "type": "tarball",
    "url": "https://www.kernel.org/pub/software/scm/git/git-2.45.0.tar.gz"
  },
  "dependencies": ["zlib", "openssl", "curl", "pcre2"],
  "build_dependencies": ["pkg-config"],
  "build_system": "autotools",
  "configure_args": [
    "--with-openssl",
    "--with-curl",
    "--with-libpcre",
    "--without-tcltk",
    "--with-zlib"
  ],
  "env": {}
}
```

When Git releases version 2.46.0, they would update the `.tsi.json` file, and the TSI workflow would automatically create a PR to add version 2.46.0 to `packages/git.json`.

## Benefits

- **Self-service**: Project maintainers can manage their own package definitions
- **Automated updates**: New versions are automatically synced
- **Single source of truth**: Package configuration lives with the project
- **Reduced maintenance**: TSI maintainers don't need to manually update every package

## See Also

- [Package Format Documentation](../packages/README.md)
- [TSI Main Documentation](../README.md)

