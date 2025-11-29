# TSI - The Source Installer

A distribution-independent source-based package manager that enables building packages from source with all their dependencies.

**Pure C implementation - No Python required!**

## Features

- **Distribution Independent**: Works on any Linux/Unix distribution
- **Source-Based**: Builds everything from source code
- **Dependency Resolution**: Automatically resolves and builds dependencies
- **Multiple Build Systems**: Supports autotools, CMake, Meson, Make
- **Minimal Requirements**: Only needs a C compiler!
- **Isolated Installation**: Installs packages to a separate prefix, avoiding conflicts

## Installation

### Quick Install

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

Add to PATH:
```bash
export PATH="$HOME/.tsi/bin:$PATH"
```

**Custom location:**
```bash
PREFIX=/opt/tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

### Manual Build

```bash
git clone https://github.com/PanterSoft/tsi.git
cd tsi/src
make
sudo make install
```

## Quick Start

```bash
# Install a package
tsi install curl

# Install specific version
tsi install git@2.45.0

# List installed packages
tsi list

# Show package information
tsi info curl

# List all available versions
tsi versions git

# Remove a package
tsi remove curl
```

## Requirements

- **C compiler** (gcc, clang, or cc)
- **make**
- **git** or **wget/curl** (for downloading sources)

That's it! No Python or other runtime dependencies.

## Documentation

Complete documentation is available at [https://pantersoft.github.io/TheSourceInstaller/](https://pantersoft.github.io/TheSourceInstaller/).

- [Installation Guide](https://pantersoft.github.io/TheSourceInstaller/getting-started/installation/)
- [User Guide](https://pantersoft.github.io/TheSourceInstaller/user-guide/package-management/)
- [Package Format](https://pantersoft.github.io/TheSourceInstaller/user-guide/package-format/)
- [Developer Guide](https://pantersoft.github.io/TheSourceInstaller/developer-guide/architecture/)

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## License

MIT License - see LICENSE file for details.
