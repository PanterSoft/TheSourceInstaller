# TSI - TheSourceInstaller

A distribution-independent source-based package manager that enables building packages from source with all their dependencies.

## Features

- **Distribution Independent**: Works on any Linux/Unix distribution
- **Source-Based**: Builds everything from source code
- **Dependency Resolution**: Automatically resolves and builds dependencies
- **Multiple Build Systems**: Supports autotools, CMake, Meson, Make, Cargo, Python, and custom build scripts
- **Minimal Requirements**: Can be installed with just Python 3.8+
- **Isolated Installation**: Installs packages to a separate prefix, avoiding conflicts

## Minimal Requirements

TSI itself requires minimal dependencies:

- **Python 3.8+** (standard library only)
- **Basic build tools** (for building packages):
  - `make` (usually pre-installed)
  - `gcc` or `clang` (for compiling C/C++ packages)
  - `git` (optional, only needed for git-based sources)
  - `curl` (optional, Python urllib is used as fallback)

## Installation

### One-Line Install (Recommended)

Install TSI with a single command. The installer automatically detects available tools and adapts:

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/bootstrap-default.sh | sh
```

Or using `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/PanterSoft/tsi/main/bootstrap-default.sh | sh
```

The installer will automatically:
- Detect available tools (Python, compilers, build tools)
- Bootstrap Python if needed
- Install TSI to `~/.tsi`

After installation, add to your PATH:
```bash
export PATH="$HOME/.tsi/bin:$PATH"
```

### For Minimal Systems (Xilinx, Embedded, etc.)

TSI provides intelligent bootstrap installers that automatically detect available tools:

```bash
# Recommended: Intelligent installer (detects tools and adapts)
chmod +x bootstrap-default.sh
./bootstrap-default.sh

# Alternative: Manual selection
# If you have Python 3.8+ available
./bootstrap-minimal.sh

# If you need to bootstrap Python from source
./bootstrap.sh
```

**Key Features:**
- **Intelligent detection**: Automatically detects available tools (Python, gcc, make, wget, curl, etc.)
- **Adaptive installation**: Chooses the best installation method based on available tools
- **POSIX-compliant**: Works with `/bin/sh` on any POSIX system
- **Graceful fallbacks**: Falls back to alternative methods when tools are missing
- **No external dependencies**: TSI itself uses only Python standard library
- **Works on Xilinx systems**: Designed for minimal embedded systems

See [INSTALL.md](INSTALL.md) for detailed installation instructions and [BOOTSTRAP.md](BOOTSTRAP.md) for quick reference.

### Quick Install (Standard Systems)

```bash
# Using the shell script
chmod +x install.sh
./install.sh

# Or using Python directly
python3 install.py
```

### Manual Install

```bash
# Clone the repository
git clone https://github.com/PanterSoft/tsi.git
cd tsi

# Install using setuptools
python3 setup.py install --user

# Or install to a custom prefix
PREFIX=/opt/tsi python3 setup.py install --prefix=$PREFIX
```

### Add to PATH

After installation, add TSI to your PATH:

```bash
# For bash/zsh
export PATH="$HOME/.local/bin:$PATH"

# Or add to your shell profile
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
```

## Usage

### Basic Commands

```bash
# Install a package
tsi install <package-name>

# Install from a package manifest file
tsi install ./packages/example.json

# List installed packages
tsi list

# Show package information
tsi info <package-name>

# Search for packages
tsi search <query>

# Remove a package
tsi remove <package-name>

# Update a package
tsi update <package-name>

# Build without installing
tsi build <package-name>
```

## Package Manifest Format

Packages are defined using JSON manifests. Here's an example:

```json
{
  "name": "example-package",
  "version": "1.0.0",
  "description": "An example package",
  "source": {
    "type": "git",
    "url": "https://github.com/user/repo.git",
    "branch": "main"
  },
  "dependencies": ["libfoo", "libbar"],
  "build_dependencies": ["cmake", "pkg-config"],
  "build_system": "cmake",
  "cmake_args": ["-DBUILD_SHARED_LIBS=ON"],
  "env": {
    "CXXFLAGS": "-O2"
  }
}
```

### Source Types

- **git**: Clone from a git repository
- **tarball**: Download and extract a tarball (.tar.gz, .tar.bz2, etc.)
- **zip**: Download and extract a zip file
- **local**: Use a local directory

### Build Systems

- **autotools**: `./configure && make && make install`
- **cmake**: CMake-based builds
- **meson**: Meson build system
- **make**: Plain Makefile
- **cargo**: Rust/Cargo projects
- **python**: Python packages (pip install)
- **custom**: Use `build_commands` for custom build steps

## Example Package Definitions

See the `packages/` directory for example package definitions.

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
└── repos/          # Package repository
```

## Distribution Independence

TSI achieves distribution independence by:

- Building everything from source
- Installing to an isolated prefix
- Setting up environment variables (PATH, PKG_CONFIG_PATH, LD_LIBRARY_PATH, etc.)
- Not relying on distribution-specific package managers

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## License

MIT License - see LICENSE file for details.
