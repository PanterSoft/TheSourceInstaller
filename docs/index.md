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
- **Single Static Binary**: No runtime dependencies after compilation

## Minimal Requirements

TSI requires only:
- **C compiler** (gcc, clang, or cc)
- **make** (for building TSI itself)
- **Basic build tools** (for building packages):
  - `make` (usually pre-installed)
  - `gcc` or `clang` (for compiling C/C++ packages)
  - `git` (optional, only needed for git-based sources)
  - `wget` or `curl` (optional, for downloading sources)

**No Python or other runtime dependencies needed!**

Perfect for Xilinx and other minimal embedded systems.

## Quick Install

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
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
```

## Documentation

- [Installation Guide](getting-started/installation.md) - How to install TSI
- [User Guide](user-guide/package-management.md) - Using TSI
- [Package Format](user-guide/package-format.md) - Creating package definitions
- [Developer Guide](developer-guide/architecture.md) - TSI internals
- [Workflows](workflows/automation.md) - Automated package management

## Why TSI?

TSI solves the problem of installing software on minimal systems where traditional package managers aren't available or don't have the packages you need. It builds everything from source, ensuring:

- ✅ Works on any Linux/Unix system
- ✅ No distribution-specific packages needed
- ✅ Full control over build options
- ✅ Can install any version of any package
- ✅ Minimal system requirements

