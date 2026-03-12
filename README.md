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

## Installation

### Quick Install

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

Add to PATH:
```bash
export PATH="$HOME/.tsi/bin:$PATH"
```

Or add permanently:
```bash
echo 'export PATH="$HOME/.tsi/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

#### Custom Installation Location

You can install TSI to a custom location using the `--prefix` option or `PREFIX` environment variable:

**Using --prefix option:**
```bash
# Install to /opt/tsi (system-wide, may require root)
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --prefix /opt/tsi

# Install to custom user location
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --prefix ~/my-tsi
```

**Using PREFIX environment variable:**
```bash
# Install to /opt/tsi (system-wide, may require root)
PREFIX=/opt/tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh

# Install to custom user location
PREFIX=~/my-tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

**Note:** Installing to system directories like `/opt/`, `/usr/local/`, or `/usr/` typically requires root permissions. Use `sudo` if needed:

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sudo sh -s -- --prefix /opt/tsi
```

After installation, add the custom location to your PATH:
```bash
# For /opt/tsi
export PATH="/opt/tsi/bin:$PATH"

# Or add permanently to ~/.bashrc or ~/.zshrc
echo 'export PATH="/opt/tsi/bin:$PATH"' >> ~/.bashrc
```

#### Bootstrap Options

The bootstrap installer supports several command-line options and environment variables:

**Command-line options:**
- `--prefix PATH` - Installation prefix (default: `~/.tsi` on Unix, `%USERPROFILE%\.tsi` on Windows)
- `--repair` - Repair/update existing TSI installation
- `--help, -h` - Show help message

**Environment variables (recommended - cleaner syntax, no '--' needed):**
- `PREFIX` - Installation prefix (same as `--prefix` option)
- `REPAIR` - Set to `1`, `true`, or `yes` to repair/update existing installation (same as `--repair` flag)
- `TSI_REPO` - Custom repository URL (default: `https://github.com/PanterSoft/tsi.git`)
- `TSI_BRANCH` - Branch to use from repository (default: `main`)
- `INSTALL_DIR` - Temporary directory for downloading and building source (default: `$HOME/tsi-install`)

**Examples:**

```bash
# Install to user location (~/.tsi) - using environment variable (recommended, no '--' needed)
PREFIX=~/.tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh

# Repair existing installation - using environment variable (recommended, no '--' needed)
REPAIR=1 curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh

# Or use command-line arguments
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --prefix ~/.tsi
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --repair

# Install from a fork or different branch
TSI_REPO=https://github.com/user/fork.git TSI_BRANCH=develop \
  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

### Manual Build (from source)

Requires [Rust](https://rustup.rs/) toolchain:

```bash
git clone https://github.com/PanterSoft/tsi.git
cd tsi
cargo build --release
```

The binary will be at `target/release/tsi`. To install:

```bash
# Unix
cp target/release/tsi ~/.tsi/bin/tsi

# Or use make install (with PREFIX set)
make install PREFIX=~/.tsi
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

## Requirements

**To run TSI:** None — the binary is self-contained.

**To build TSI from source:** [Rust](https://rustup.rs/) toolchain (rustc, cargo).

**To build packages with TSI:**
- **macOS**: Xcode Command Line Tools (clang, make)
- **Linux**: gcc (or clang) and make
- **Windows**: Visual Studio Build Tools or MinGW

TSI uses built-in HTTP and archive extraction — no system curl, wget, or tar required for downloads.

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
