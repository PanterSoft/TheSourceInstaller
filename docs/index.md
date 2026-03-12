# TSI - The Source Installer

A distribution-independent source-based package manager that enables building packages from source with all their dependencies.

**Written in Rust — single static binary, zero runtime dependencies.**

## Features

- **Cross-Platform**: Works on macOS, Linux, and Windows
- **Distribution Independent**: Works on any Linux/Unix distribution
- **Source-Based**: Builds everything from source code
- **Built-in HTTP & Archives**: No system curl, wget, or tar required — downloads and extracts via pure Rust
- **Dependency Resolution**: Automatically resolves and builds dependencies
- **Multiple Build Systems**: Supports autotools, CMake, Meson, Make, and custom commands
- **Minimal Requirements**: Single binary; only a C compiler and make needed for building packages
- **Isolated Installation**: Installs packages to a separate prefix, avoiding conflicts
- **Homebrew-Style CLI**: Clean, intuitive command-line interface

## Minimal Requirements

**To run TSI:** None — the binary is self-contained.

**To build TSI from source:** [Rust](https://rustup.rs/) toolchain (rustc, cargo).

**To build packages with TSI:**
- **macOS**: Xcode Command Line Tools (clang, make)
- **Linux**: gcc (or clang) and make
- **Windows**: Visual Studio Build Tools or MinGW

## Quick Install

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

## Quick Start

```bash
# Update package definitions (run once after install)
tsi update

# Install a package
tsi install curl

# Install specific version
tsi install git@2.45.0

# List installed packages
tsi list

# Show package information
tsi info curl

# Search available packages
tsi search ssl

# Upgrade installed packages
tsi upgrade

# Check system health
tsi doctor

# Remove a package
tsi uninstall curl
```

## Documentation

- [Installation Guide](getting-started/installation.md) - How to install TSI
- [User Guide](user-guide/package-management.md) - Using TSI
- [Package Format](user-guide/package-format.md) - Creating package definitions
- [Developer Guide](developer-guide/architecture.md) - TSI internals
- [Workflows](workflows/automation.md) - Automated package management

## Why TSI?

TSI solves the problem of installing software on minimal systems where traditional package managers aren't available or don't have the packages you need. It builds everything from source, ensuring:

- Works on any Linux/Unix system, plus macOS and Windows
- No distribution-specific packages needed
- Full control over build options
- Can install any version of any package
- Minimal system requirements — built-in HTTP and archive extraction
