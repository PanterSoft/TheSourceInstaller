# TSI Bootstrap Options for Minimal Systems

## Current Status

TSI is implemented in pure C and requires only a C compiler to build.

## Installation Methods

### Method 1: One-Line Install (Recommended)

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

This automatically:
- Downloads TSI source code
- Builds TSI using your C compiler
- Installs TSI to `~/.tsi`

### Method 2: Manual Build

```bash
# Clone repository
git clone https://github.com/PanterSoft/tsi.git
cd tsi

# Build
cd src
make

# Install
sudo make install
```

### Method 3: Custom Prefix

```bash
# Build and install to custom location
cd src
make
make install DESTDIR=/opt/tsi
```

## Requirements

### Minimum Requirements
- **C compiler**: gcc, clang, or cc
- **make**: Build system
- **git** or **wget/curl**: For downloading sources (optional)
- **tar**: For extracting archives (optional)

### For Building Packages
- **make**: Usually pre-installed
- **gcc/clang**: For compiling C/C++ packages
- **git**: For git-based sources
- **wget/curl**: For downloading sources
- **tar/gzip**: For extracting archives

## Bootstrap Process

1. **Source Download**: Clone or download TSI source
2. **Compilation**: Build TSI binary using C compiler
3. **Installation**: Copy binary to installation directory
4. **PATH Setup**: Add TSI to PATH

## System Compatibility

### Supported Systems
- ✅ Linux (all distributions)
- ✅ macOS
- ✅ BSD variants
- ✅ Embedded Linux (Xilinx, etc.)

### Minimal Systems
TSI works on systems with:
- ✅ C compiler only
- ✅ C compiler + make
- ❌ No C compiler (not possible - TSI needs to be built)

## Static Binary Distribution

For systems where building is not possible, pre-compiled static binaries can be provided:

```bash
# Download pre-compiled binary
wget https://tsi.example.com/tsi-static-linux-x86_64
chmod +x tsi-static-linux-x86_64
./tsi-static-linux-x86_64 install package
```

**Note**: Static binaries need to be compiled for each target architecture.

## Cross-Compilation

TSI can be cross-compiled for target architectures:

```bash
# On build machine
CC=arm-linux-gnueabihf-gcc make
```

## Troubleshooting

### No C Compiler Available
- Use pre-compiled static binary
- Install C compiler from system package manager (if available)
- Use cross-compilation from another system

### Build Fails
- Check C compiler version (C11 support required)
- Verify make is installed
- Check for sufficient disk space

### Installation Issues
- Verify write permissions to installation directory
- Check PATH configuration
- Ensure binary is executable

## Next Steps

1. Provide pre-compiled static binaries for common architectures
2. Add architecture detection in bootstrap script
3. Support for more build systems
4. Package repository hosting
