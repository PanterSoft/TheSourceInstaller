# TSI Package Repository

This directory contains package definitions for TSI. Each package is defined as a JSON file.

**Total Packages: 100**

## Package Categories

### Build Tools & Compilers

- **pkg-config** - Helper tool for finding installed libraries
- **cmake** - Cross-platform build system generator
- **meson** - Build system
- **ninja** - Small build system
- **autoconf** - Build system generator
- **automake** - Build system generator (depends on autoconf)
- **libtool** - Generic library support script
- **make** - GNU Make build tool
- **gcc** - GNU Compiler Collection
- **clang** - C language family frontend for LLVM
- **llvm** - LLVM compiler infrastructure
- **rust** - Rust programming language
- **go** - Go programming language

### Core System Libraries

- **zlib** - Compression library (very common dependency)
- **openssl** - Cryptography and SSL/TLS toolkit
- **libffi** - Foreign Function Interface library
- **pcre2** - Perl Compatible Regular Expressions library
- **oniguruma** - Regular expressions library
- **re2** - Fast, safe, thread-friendly regular expression library
- **ncurses** - Terminal control library
- **readline** - Command-line editing library
- **libedit** - Command line editing library
- **libuuid** - UUID generation library
- **libcap** - Linux capabilities library
- **libseccomp** - Linux seccomp library
- **liburing** - Linux io_uring library
- **libuv** - Cross-platform asynchronous I/O library
- **libmagic** - File type identification library

### Compression & Archiving

- **bzip2** - High-quality data compressor
- **xz** - XZ compression library
- **lz4** - Extremely fast compression algorithm
- **zstd** - Fast real-time compression algorithm
- **snappy** - Fast compression/decompression library
- **libarchive** - Multi-format archive and compression library
- **tar** - GNU tar archiving utility
- **gzip** - GNU compression utility
- **unzip** - Extraction utility for .zip archives

### Networking & HTTP

- **curl** - Command line tool and library for transferring data with URLs
- **wget** - Network utility to retrieve files from the Web
- **aria2** - Lightweight multi-protocol download utility
- **libssh** - SSH client library
- **libevent** - Event notification library
- **libev** - High-performance event loop library
- **zeromq** - High-performance messaging library
- **nanomsg** - Socket library
- **grpc** - High performance RPC framework

### Data Serialization

- **protobuf** - Protocol Buffers data serialization
- **msgpack** - Efficient binary serialization format
- **avro** - Data serialization system

### XML & JSON Parsing

- **libxml2** - XML C parser and toolkit
- **libxslt** - XSLT library
- **expat** - XML parser library
- **yajl** - Yet Another JSON Library
- **jansson** - C library for encoding, decoding and manipulating JSON data
- **cjson** - Ultralightweight JSON parser in ANSI C
- **rapidjson** - Fast JSON parser/generator for C++

### Configuration & Data Formats

- **libyaml** - YAML parser and emitter library

### Database Libraries

- **sqlite** - SQL database engine
- **gdbm** - GNU database manager
- **berkeley-db** - Berkeley DB embedded database library
- **lmdb** - Lightning Memory-Mapped Database
- **leveldb** - Fast key-value storage library
- **rocksdb** - Persistent key-value store
- **redis** - In-memory data structure store
- **postgresql** - PostgreSQL database server
- **mysql** - MySQL database server
- **mariadb** - MariaDB database server
- **mongodb** - MongoDB database server

### Graphics & Image Processing

- **libpng** - PNG reference library
- **libjpeg-turbo** - JPEG image codec library
- **libwebp** - WebP image library
- **libtiff** - TIFF library and utilities
- **libgif** - GIF library
- **libavif** - AV1 Image File Format library
- **libheif** - HEIF image format library
- **freetype** - FreeType font rendering library
- **harfbuzz** - Text shaping library
- **cairo** - 2D graphics library
- **pixman** - Pixel manipulation library
- **pango** - Text layout and rendering library
- **gdk-pixbuf** - Image loading library

### GUI & Desktop Libraries

- **glib** - Core application building blocks
- **gobject-introspection** - GObject introspection framework

### Math & Scientific Computing

- **gmp** - GNU Multiple Precision Arithmetic Library
- **mpfr** - Multiple-precision floating-point library
- **mpc** - Multiple-precision complex arithmetic library
- **boost** - C++ libraries
- **icu** - International Components for Unicode

### Version Control & Development Tools

- **git** - Distributed version control system
- **libgit2** - Git library

### Text Editors & Shells

- **vim** - Vi IMproved text editor
- **emacs** - GNU Emacs text editor
- **tmux** - Terminal multiplexer
- **bash** - GNU Bourne-Again SHell
- **zsh** - Z shell
- **fish** - Friendly interactive shell

### Programming Languages & Runtimes

- **python** - Python programming language
- **ruby** - Ruby programming language
- **node** - Node.js JavaScript runtime

### Applications

- **curl** - Command line tool and library for transferring data with URLs

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

### Manual Addition

1. Create a new JSON file in this directory
2. Follow the format of existing packages
3. Test installation: `tsi install <package-name>`

### External Package Configuration

Projects can include their own `.tsi.json` file in their repository, and a GitHub Actions workflow will automatically sync updates. See [External Package Configuration](../user-guide/external-packages.md) for details.

This allows project maintainers to:
- Include package configuration directly in their repository
- Automatically sync new versions to TSI
- Maintain a single source of truth for package definitions

### Automatic Version Discovery

TSI can automatically discover and add new package versions using the version discovery system. See [Version Discovery](../user-guide/version-discovery.md) for details.

**Quick start:**
```bash
# Discover versions for a package
python3 scripts/discover-versions.py <package-name>

# Discover versions for all packages
python3 scripts/discover-versions.py --all --dry-run
```

The system:
- Automatically discovers versions from GitHub, git repos, and other sources
- Generates version definitions based on existing templates
- Adds new versions while preserving existing ones
- Runs weekly via GitHub Actions to keep packages up-to-date

## Notes

- All packages install to `${TSI_PREFIX}` (default: `~/.tsi/install`)
- Environment variables like `PKG_CONFIG_PATH` are set automatically
- Dependencies are resolved and installed automatically
- Build order is determined automatically via topological sort

