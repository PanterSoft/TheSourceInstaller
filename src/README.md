# TSI Rust Implementation

Rust implementation of TSI - single static binary, zero runtime dependencies.

## Building

### Requirements

- Rust toolchain (1.70+)
- For building packages: C compiler (gcc/clang) and make

### Build

```bash
cargo build --release
```

This creates a binary at `target/release/tsi` (or `tsi.exe` on Windows).

### From Repository Root

```bash
make build
# or
cargo build --release
```

## Features

### Implemented

- Package manifest parsing (JSON)
- Dependency resolution with topological sort
- Database management (installed packages)
- Registry and repository management
- Source code fetching (git, HTTP, archive extraction - built-in)
- Build system integration (autotools, CMake, Meson, Make)
- Installation and uninstallation
- Full CLI: install, uninstall, upgrade, list, search, info, update, doctor

### Build Systems Supported

- **autotools**: `./configure && make && make install`
- **cmake**: CMake-based builds
- **meson**: Meson build system
- **make**: Plain Makefile

## Usage

After building:

```bash
./target/release/tsi --help
./target/release/tsi list
./target/release/tsi info <package>
./target/release/tsi install <package>
./target/release/tsi uninstall <package>
```

## Module Layout

```
src/
├── main.rs           # Entry point
├── lib.rs            # Library root
├── cli/              # Command-line interface (clap)
├── core/             # Package, registry, resolver, database, config
├── ops/              # Fetch, build, install, uninstall, link
├── ui/               # Output, progress, table
└── platform/         # OS detection, prefix resolution (unix, windows)
```

## Installation

```bash
# From repository root
make install

# Or with custom prefix
make install PREFIX=/opt/tsi
```

## Development

For rapid development and testing:

```bash
# Build and run
cargo run -- install <package>

# Run tests
cargo test

# Lint
cargo clippy
cargo fmt --check
```

## Directory Structure

After installation, TSI creates:

```
~/.tsi/                    # or %USERPROFILE%\.tsi on Windows
├── build/                 # Build directories
├── install/               # Installed packages
│   ├── bin/
│   ├── lib/
│   └── include/
├── sources/               # Downloaded source code
├── db/                    # Package database
└── packages/              # Package repository
```

## See Also

- [../README.md](../README.md) - Main documentation
- [../docs/](../docs/) - Full documentation
