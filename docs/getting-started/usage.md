# Usage

## Basic Commands

### Install Packages

```bash
# Install latest version
tsi install <package-name>

# Install specific version
tsi install <package-name>@<version>

# Install from local package file
tsi install ./packages/example.json

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

### Remove Packages

```bash
# Remove a package
tsi remove <package-name>

# Remove with custom prefix
tsi remove <package-name> --prefix /opt/tsi
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

### Uninstall TSI

```bash
# Uninstall TSI (preserves data)
tsi uninstall

# Uninstall everything including data
tsi uninstall --all

# Uninstall from custom location
tsi uninstall --prefix /opt/tsi
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

- `PATH`: Includes `~/.tsi/install/bin`
- `PKG_CONFIG_PATH`: Includes `~/.tsi/install/lib/pkgconfig`
- `LD_LIBRARY_PATH`: Includes `~/.tsi/install/lib`
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

# Remove from custom prefix
tsi remove curl --prefix /opt/tsi
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

# Install different version
tsi install git@2.44.0
```

## Troubleshooting

### Package Not Found

```bash
# Update repository first
tsi update

# Then try installing again
tsi install <package-name>
```

### Build Failures

- Check that you have required build tools (gcc, make, etc.)
- Verify dependencies are installed
- Check package definition for correct source URL

### Permission Issues

- Use `--prefix` to install to a writable location
- Or use `sudo` for system-wide installation

