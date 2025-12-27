# Quick Start

## Basic Commands

```bash
# Install a package (latest version)
tsi install <package-name>

# Install a specific version
tsi install <package-name>@<version>
# Example: tsi install curl@8.7.1

# Install from a package manifest file
tsi install ./packages/example.json

# List installed packages
tsi list

# Show package information (shows all available versions)
tsi info <package-name>

# Show information for a specific version
tsi info <package-name>@<version>

# Remove a package
tsi remove <package-name>

# Update package repository
tsi update                                    # Update from default TSI repository
tsi update --repo https://github.com/user/repo.git  # Update from custom repository
tsi update --local /path/to/packages          # Update from local directory

# Uninstall TSI (removes everything including all data)
tsi uninstall                                 # Remove TSI and all data (packages, database, etc.)
tsi uninstall --prefix /opt/tsi              # Uninstall from custom location
```

## Example: Installing curl

```bash
# Update package repository
tsi update

# Install curl (will automatically install dependencies: openssl, zlib)
tsi install curl

# Check what was installed
tsi list

# Get information about curl
tsi info curl
```

## How It Works

1. **Dependency Resolution**: TSI resolves all dependencies recursively
2. **Source Fetching**: Downloads or clones source code
3. **Building**: Compiles packages using the appropriate build system
4. **Installation**: Installs to an isolated prefix (`~/.tsi/install` by default)
5. **Database**: Tracks installed packages in a local database

## Directory Structure

After installation, TSI creates the following structure:

```
~/.tsi/
├── build/          # Build directories
├── install/        # Installed packages
│   ├── bin/
│   ├── lib/
│   └── include/
├── sources/        # Downloaded source code
├── db/             # Package database
└── packages/       # Package repository
```

## Next Steps

- Learn about [Package Management](../user-guide/package-management.md)
- Understand the [Package Format](../user-guide/package-format.md)
- Explore [External Packages](../user-guide/external-packages.md)

