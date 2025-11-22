# TSI Architecture

## Current Implementation (C)

TSI is implemented in pure C (C11 standard) with no runtime dependencies.

### Advantages
- **Universal availability**: C compiler (gcc/clang/cc) is available on virtually all systems
- **Single binary**: Can be compiled as a static binary with no dependencies
- **Small footprint**: Compiled binary is small and efficient
- **Easy bootstrap**: Most minimal systems have a C compiler
- **No runtime dependency**: Self-contained executable
- **Performance**: Fast execution for package management operations
- **Distribution independence**: Works on any Unix-like system with a C compiler

### Implementation Details

- **Language**: C11 (ISO/IEC 9899:2011)
- **Build System**: Make
- **Dependencies**: None (pure C standard library)
- **Linking**: Static linking preferred (optional dynamic linking on macOS)

### Core Components

1. **Package Management** (`package.c/h`)
   - JSON manifest parsing
   - Package metadata handling
   - Dependency tracking

2. **Dependency Resolution** (`resolver.c/h`)
   - Topological sorting
   - Dependency graph construction
   - Build order determination

3. **Source Fetching** (`fetcher.c/h`)
   - Git repository cloning
   - Tarball/zip downloading
   - Local source handling

4. **Build System Integration** (`builder.c/h`)
   - Autotools support
   - CMake support
   - Meson support
   - Plain Makefile support
   - Custom build commands

5. **Database** (`database.c/h`)
   - Installed package tracking
   - JSON-based storage
   - Package metadata persistence

6. **Repository** (`resolver.c/h`)
   - Package manifest loading
   - Repository directory scanning
   - Package lookup

7. **CLI** (`main.c`)
   - Command-line interface
   - Install/remove/list commands
   - Package information display

## Design Principles

1. **Minimal Requirements**: Only needs a C compiler
2. **Source-Based**: Everything built from source
3. **Isolated Installation**: Packages installed to separate prefix
4. **Distribution Independent**: No reliance on system package managers
5. **Self-Contained**: Single binary, no runtime dependencies

## Future Considerations

### Potential Enhancements
- Parallel builds
- Build caching
- Binary package distribution (optional)
- Cross-compilation support
- Package signing/verification

### Platform Support
- **Primary**: Linux (all distributions)
- **Secondary**: macOS, BSD variants
- **Target**: Embedded systems (Xilinx, etc.)

## Build Process

1. Compile all C source files to object files
2. Link into single binary (`tsi`)
3. Optional: Static linking for maximum portability
4. Install to system or user directory

## Testing

Comprehensive Docker-based testing:
- Minimal systems (no tools)
- Systems with C compiler only
- Various Linux distributions (Alpine, Ubuntu)

See [docker/README.md](../docker/README.md) for testing details.
