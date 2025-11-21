# TSI Package Repository

This directory contains package definitions for TSI. Each package is defined as a JSON file.

## Essential Packages

### Build Tools

- **pkg-config** (`pkg-config.json`) - Helper tool for finding installed libraries
- **cmake** (`cmake.json`) - Cross-platform build system generator
- **autoconf** (`autoconf.json`) - Build system generator
- **automake** (`automake.json`) - Build system generator (depends on autoconf)
- **libtool** (`libtool.json`) - Generic library support script

### Core Libraries

- **zlib** (`zlib.json`) - Compression library (very common dependency)
- **openssl** (`openssl.json`) - Cryptography and SSL/TLS toolkit
- **libffi** (`libffi.json`) - Foreign Function Interface library
- **pcre2** (`pcre2.json`) - Perl Compatible Regular Expressions library

### Applications

- **curl** (`curl.json`) - Command line tool and library for transferring data with URLs

## Package Dependencies

```
pkg-config (standalone)
zlib (standalone)
openssl (standalone)
libffi (standalone)
pcre2 (standalone)

autoconf (standalone)
automake (depends on: autoconf)
libtool (standalone)

cmake (depends on: openssl)

curl (depends on: openssl, zlib)
  └── build_dependencies: pkg-config
```

## Installation Order

For a minimal system, recommended installation order:

1. **pkg-config** - Needed by many packages
2. **zlib** - Common compression library
3. **openssl** - Security library
4. **autoconf** - Build system
5. **automake** - Build system (needs autoconf)
6. **libtool** - Library support
7. **libffi** - FFI library
8. **pcre2** - Regex library
9. **cmake** - Build system (needs openssl)
10. **curl** - HTTP client (needs openssl, zlib)

TSI will automatically resolve and install dependencies in the correct order.

## Usage

### Install a package

```bash
# Install pkg-config
tsi install pkg-config

# Install curl (will automatically install openssl and zlib first)
tsi install curl

# Install cmake (will automatically install openssl first)
tsi install cmake
```

### List available packages

```bash
# List all packages in repository
ls packages/*.json

# Get info about a package
tsi info pkg-config
```

## Package Format

See `example.json` for a basic package template.

### Required Fields

- `name`: Package name
- `version`: Package version
- `description`: Package description
- `source`: Source information (type, url, etc.)
- `build_system`: Build system type (autotools, cmake, make, custom)

### Optional Fields

- `dependencies`: Runtime dependencies
- `build_dependencies`: Build-time dependencies
- `configure_args`: Arguments for ./configure
- `cmake_args`: Arguments for cmake
- `make_args`: Arguments for make
- `env`: Environment variables
- `build_commands`: Custom build commands (for custom build system)

## Adding New Packages

1. Create a new JSON file in this directory
2. Follow the format of existing packages
3. Test installation: `tsi install <package-name>`

## Notes

- All packages install to `${TSI_PREFIX}` (default: `~/.tsi/install`)
- Environment variables like `PKG_CONFIG_PATH` are set automatically
- Dependencies are resolved and installed automatically
- Build order is determined automatically via topological sort

