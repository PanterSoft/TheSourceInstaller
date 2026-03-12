# Usage

## Basic Commands

### Install Packages

```bash
# Install latest version
tsi install <package-name>

# Install specific version
tsi install <package-name>@<version>

# Install with custom prefix
tsi install <package-name> --prefix /opt/tsi

# Force reinstall
tsi install <package-name> --force
```

### List and Info

```bash
# List installed packages
tsi list

# Show package information (all versions)
tsi info <package-name>

# Show specific version information
tsi info <package-name>@<version>
```

### Uninstall Packages

```bash
# Remove a package
tsi uninstall <package-name>

# Remove multiple packages
tsi uninstall <package1> <package2>

# Remove with custom prefix
tsi uninstall <package-name> --prefix /opt/tsi
```

### Search Packages

```bash
# Search available packages
tsi search <query>

# Search with custom prefix
tsi search <query> --prefix /opt/tsi
```

### Upgrade Packages

```bash
# Upgrade all installed packages
tsi upgrade

# Upgrade specific packages
tsi upgrade <package1> <package2>

# Upgrade with custom prefix
tsi upgrade --prefix /opt/tsi
```

### Update Repository

```bash
# Update from default repository
tsi update

# Update from custom repository
tsi update --repo https://github.com/user/repo.git

# Update from local directory
tsi update --local /path/to/packages

# Update with custom prefix
tsi update --prefix /opt/tsi
```

### System Health

```bash
# Check your system for potential problems
tsi doctor

# Check with custom prefix
tsi doctor --prefix /opt/tsi
```

## Package Versioning

TSI supports multiple versions of packages:

```bash
# Install latest version
tsi install git

# Install specific version
tsi install git@2.45.0
tsi install git@2.44.0

# List available versions
tsi info git
```

## Environment Variables

TSI automatically sets up environment variables for installed packages:

- `PATH`: Includes `~/.tsi/install/bin` (or `%USERPROFILE%\.tsi\install\bin` on Windows)
- `PKG_CONFIG_PATH`: Includes `~/.tsi/install/lib/pkgconfig`
- `LD_LIBRARY_PATH`: Includes `~/.tsi/install/lib` (Unix)
- `CPPFLAGS`: Includes `-I~/.tsi/install/include`
- `LDFLAGS`: Includes `-L~/.tsi/install/lib`

These are set automatically when you use TSI commands.

## Custom Installation Prefix

You can install packages to a custom location:

```bash
# Install to custom prefix
tsi install curl --prefix /opt/tsi

# List packages in custom prefix
tsi list --prefix /opt/tsi

# Uninstall from custom prefix
tsi uninstall curl --prefix /opt/tsi
```

## Examples

### Installing Build Tools

```bash
# Install essential build tools
tsi install pkg-config
tsi install cmake
tsi install autoconf
tsi install automake
tsi install libtool
```

### Installing with Dependencies

```bash
# Install curl (automatically installs openssl and zlib)
tsi install curl

# TSI will:
# 1. Check dependencies (openssl, zlib)
# 2. Install dependencies first
# 3. Then install curl
```

### Working with Versions

```bash
# Check available versions
tsi info git

# Install specific version
tsi install git@2.45.0

# Upgrade to latest
tsi upgrade git
```

## Troubleshooting

### Package Not Found

```bash
# Update repository first
tsi update

# Search for the package
tsi search <query>

# Then try installing again
tsi install <package-name>
```

### Build Failures

- Run `tsi doctor` to check for required build tools (gcc, make, etc.)
- Verify dependencies are installed
- Check package definition for correct source URL

### Permission Issues

- Use `--prefix` to install to a writable location
- Or use `sudo` for system-wide installation
