# Package Management

## Overview

TSI manages packages through a local repository system. Packages are defined as JSON files and can have multiple versions.

## Package Repository

The package repository is located at `~/.tsi/packages/` by default. Each package is defined as a JSON file.

### Updating the Repository

```bash
# Update from default repository
tsi update

# Update from custom repository
tsi update --repo https://github.com/user/repo.git

# Update from local directory
tsi update --local /path/to/packages
```

## Installing Packages

### Basic Installation

```bash
# Install latest version
tsi install <package-name>

# Install specific version
tsi install <package-name>@<version>
```

### Installation Process

When you install a package, TSI:

1. **Resolves Dependencies**: Checks all required dependencies
2. **Builds Dependency Graph**: Determines build order
3. **Fetches Sources**: Downloads or clones source code
4. **Builds Packages**: Compiles in dependency order
5. **Installs**: Copies files to installation prefix
6. **Updates Database**: Records installed packages

### Dependency Resolution

TSI automatically resolves and installs dependencies:

```bash
# Installing curl will automatically install:
# - openssl
# - zlib
# - pkg-config (build dependency)
tsi install curl
```

## Package Versions

TSI supports multiple versions of the same package:

```bash
# List available versions
tsi info git

# Install specific version
tsi install git@2.45.0
tsi install git@2.44.0
```

### Version Format

Versions can be specified in several ways:

- **Semantic versioning**: `1.2.3`
- **With prefix**: `v1.2.3` (automatically handled)
- **Git tags**: `2.45.0`

## Listing Installed Packages

```bash
# List all installed packages
tsi list

# List with custom prefix
tsi list --prefix /opt/tsi
```

## Package Information

```bash
# Show all available versions
tsi info <package-name>

# Show specific version
tsi info <package-name>@<version>
```

## Removing Packages

```bash
# Remove a package
tsi remove <package-name>

# Remove with custom prefix
tsi remove <package-name> --prefix /opt/tsi
```

**Note**: Removing a package does not remove its dependencies. Dependencies are only removed if no other packages depend on them.

## Custom Installation Prefix

You can install packages to a custom location:

```bash
# Install to custom prefix
tsi install curl --prefix /opt/tsi

# Environment variables are adjusted automatically
```

## Package Database

TSI maintains a database of installed packages at `~/.tsi/db/`. This tracks:

- Installed packages and versions
- Installation timestamps
- Dependencies
- Build information

## Best Practices

1. **Regular Updates**: Run `tsi update` regularly to get new packages
2. **Version Pinning**: Use specific versions for production environments
3. **Custom Prefix**: Use `--prefix` for isolated installations
4. **Dependency Management**: Let TSI handle dependencies automatically

## Troubleshooting

### Package Not Found

```bash
# Update repository
tsi update

# Verify package exists
tsi info <package-name>
```

### Dependency Conflicts

TSI handles dependencies automatically, but if issues occur:

1. Check package definitions for correct dependencies
2. Verify all dependencies are available
3. Check build order

### Build Failures

- Verify build tools are installed
- Check package source URLs are valid
- Review package build configuration

