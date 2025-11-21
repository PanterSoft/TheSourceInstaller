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

## Installation

### One-Line Install (Recommended)

Install TSI with a single command. The installer downloads TSI source and builds it:

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

Or using `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

The installer will automatically:
- Download TSI source code (via git clone or tarball)
- Build TSI using your C compiler
- Install TSI to `~/.tsi`

**Custom installation location:**
```bash
PREFIX=/opt/tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

**Repair/Update existing installation:**
```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --repair
```

This will rebuild and reinstall TSI, useful for:
- Fixing broken installations
- Updating to the latest version
- Rebuilding after system changes

After installation, add to your PATH:
```bash
export PATH="$HOME/.tsi/bin:$PATH"
```

### Manual Build

```bash
# Clone the repository
git clone https://github.com/PanterSoft/tsi.git
cd tsi

# Build
cd src
make

# Install
sudo make install

# Or install to custom location
make install DESTDIR=/opt/tsi
```

### Requirements

- **C compiler** (gcc, clang, or cc)
- **make**
- **git** or **wget/curl** (for downloading sources)
- **tar** (for extracting archives)

That's it! No Python, no other dependencies.

### Add to PATH

After installation, add TSI to your PATH:

```bash
# For bash/zsh
export PATH="$HOME/.tsi/bin:$PATH"

# Or add to your shell profile
echo 'export PATH="$HOME/.tsi/bin:$PATH"' >> ~/.bashrc
```

### Enable Autocomplete

TSI includes comprehensive shell completion for bash and zsh with full support for all commands and options:

**Bash:**
```bash
source ~/.tsi/share/completions/tsi.bash
# Or add to ~/.bashrc:
echo 'source ~/.tsi/share/completions/tsi.bash' >> ~/.bashrc
```

**Zsh:**
```bash
source ~/.tsi/share/completions/tsi.zsh
# Or add to ~/.zshrc:
echo 'source ~/.tsi/share/completions/tsi.zsh' >> ~/.zshrc
```

**Complete autocomplete support:**

- **Command completion**: `tsi <TAB>` - Shows all commands (install, remove, list, info, update, uninstall, --help, --version)
- **Package completion**:
  - `tsi install <TAB>` - Shows available packages from repository
  - `tsi install --force <TAB>` - Shows packages (after options)
  - `tsi info <TAB>` - Shows available packages
- **Installed package completion**:
  - `tsi remove <TAB>` - Shows installed packages
- **Option completion**:
  - `tsi install --<TAB>` - Shows `--force`, `--prefix`
  - `tsi update --<TAB>` - Shows `--repo`, `--local`, `--prefix`
  - `tsi uninstall --<TAB>` - Shows `--all`, `--prefix`
- **Directory completion**:
  - `tsi install --prefix <TAB>` - Completes directory paths
  - `tsi update --local <TAB>` - Completes directory paths
  - `tsi uninstall --prefix <TAB>` - Completes directory paths

## Usage

### Basic Commands

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

# Uninstall TSI
tsi uninstall                                 # Remove TSI binary and completion scripts
tsi uninstall --all                           # Remove TSI and all data (packages, database, etc.)
tsi uninstall --prefix /opt/tsi              # Uninstall from custom location

# List installed packages
tsi list
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
  "dependencies": ["libfoo@1.0.0", "libbar"],  // Version constraints supported
  "build_dependencies": ["cmake@3.20.0", "pkg-config"],  // Mix of versioned and unversioned
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

## Testing

TSI includes comprehensive Docker-based testing:

```bash
# Run all tests
make test

# Or manually
cd docker
./run-tests.sh
```

### Test Scenarios

- Minimal Alpine/Ubuntu (no tools) - should fail gracefully
- Systems with C compiler only - should build and work

### CI/CD

Tests run automatically on:
- **GitHub Actions**: On push, PR, and manual trigger
- **GitLab CI**: Similar automated testing

See [tests/README.md](tests/README.md) and [docker/README.md](docker/README.md) for detailed testing documentation.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## License

MIT License - see LICENSE file for details.
