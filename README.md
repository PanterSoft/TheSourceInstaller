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
export PATH="/opt/tsi/bin:$PATH"
```

Or add permanently:
```bash
echo 'export PATH="/opt/tsi/bin:$PATH"' >> ~/.bashrc
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
- `--prefix PATH` - Installation prefix (default: `/opt/tsi`)
- `--repair` - Repair/update existing TSI installation
- `--help, -h` - Show help message

**Environment variables (recommended - cleaner syntax, no '--' needed):**
- `PREFIX` - Installation prefix (same as `--prefix` option, default: `/opt/tsi`)
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

# Or use command-line arguments (no '--' needed for 'repair')
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s repair
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s --prefix ~/.tsi
# Note: '--repair' requires '--' separator:
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --repair

# Install from a fork or different branch
TSI_REPO=https://github.com/user/fork.git TSI_BRANCH=develop \
  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sudo sh

# Custom build directory
INSTALL_DIR=/tmp/tsi-build \
  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sudo sh

# Combine options using environment variables
PREFIX=~/my-tsi REPAIR=1 curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

**Note:** When piping to `sh`, use environment variables (no `--` needed) for cleaner syntax. The `--` separator is only required when using `sh -s --` with command-line arguments.

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

## TUI Styles

TSI ships with a themed terminal UI so every command shares the same colors, icons, and progress indicators. Install and update operations show a live “command window” with the exact shell commands and their last few output lines while an overview line explains the current step. The default `vibrant` style uses Unicode glyphs and bright colors, but you can switch styles globally or per command:

- Set `TSI_TUI_STYLE=plain` to use ASCII-only output (also respects the standard `NO_COLOR` flag).
- Force the vibrant style even on limited terminals with `TSI_FORCE_COLOR=1`.
- Override per invocation with `tsi --style plain install curl`.

Available styles today: `vibrant` (default) and `plain`. More palettes can be added by creating new named styles without touching every printf in the codebase.

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
