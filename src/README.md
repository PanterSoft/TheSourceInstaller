# TSI C Implementation

Complete C implementation of TSI - no Python required!

## Building

### Requirements
- C compiler (gcc, clang, or cc)
- make
- Standard C library

### Build

```bash
cd src
make
```

This creates a static binary at `bin/tsi` that can be copied to any system with the same architecture.

### Static Build

The Makefile creates a static binary by default:
- No external dependencies
- Works on any Linux system with the same architecture
- Larger binary size (~2-5MB vs ~100KB dynamic)

### Cross-compilation

To cross-compile for a different architecture:

```bash
# For ARM64
CC=aarch64-linux-gnu-gcc make

# For ARMv7
CC=arm-linux-gnueabihf-gcc make
```

## Features

### Implemented
- ✅ Package manifest parsing (JSON)
- ✅ Dependency resolution with topological sort
- ✅ Database management (installed packages)
- ✅ Repository management
- ✅ Source code fetching (git, tarball, local)
- ✅ Build system integration (autotools, CMake, Meson, Make)
- ✅ Installation execution
- ✅ Full CLI (install, remove, list, info)

### Build Systems Supported
- **autotools**: `./configure && make && make install`
- **cmake**: CMake-based builds
- **meson**: Meson build system
- **make**: Plain Makefile

## Usage

After building:

```bash
./bin/tsi --help
./bin/tsi list
./bin/tsi info <package>
./bin/tsi install <package>
./bin/tsi remove <package>
```

## Architecture

```
src/
├── package.h/c      - Package manifest handling
├── database.h/c     - Installed packages database
├── resolver.h/c     - Dependency resolution
├── fetcher.h/c      - Source code fetching
├── builder.h/c      - Build system integration
├── main.c           - CLI interface
└── Makefile         - Build system
```

## Advantages

- ✅ Works on systems with only C compiler
- ✅ Single static binary, no dependencies
- ✅ Faster execution
- ✅ Smaller footprint
- ✅ Easier to bootstrap
- ✅ Perfect for minimal systems (Xilinx, embedded)

## Installation

```bash
# Build
make

# Install system-wide
sudo make install

# Or install to custom location
make install DESTDIR=/opt/tsi
```

## Directory Structure

After installation, TSI creates:

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

## Package Format

Packages are defined using JSON manifests (same format as Python version):

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
  "dependencies": ["libfoo"],
  "build_system": "autotools",
  "configure_args": ["--enable-shared"]
}
```

## Future Work

1. **Better JSON parser**: Currently uses simple string matching (works but limited)
2. **More build systems**: Cargo, Python packages
3. **Better error handling**: More detailed error messages
4. **Testing**: Unit tests for core components
5. **Performance**: Optimize dependency resolution

## See Also

- [../README.md](../README.md) - Main documentation
- [../INSTALL.md](../INSTALL.md) - Installation guide
