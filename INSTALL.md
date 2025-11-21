# TSI Installation Guide

This guide covers installing TSI on minimal systems, including Xilinx embedded systems with no pre-installed tools.

## Installation Methods

### Method 0: Intelligent Bootstrap (Recommended)

The intelligent bootstrap installer automatically detects available tools and adapts:

```sh
chmod +x bootstrap-default.sh
./bootstrap-default.sh
```

**What it does:**
- Detects Python, compilers, build tools, download tools
- Automatically chooses the best installation path
- Bootstraps Python if needed and possible
- Installs TSI using the most efficient method available

**This is the recommended method for all systems.**

### Method 1: Minimal Installer (Python Already Available)

If you have Python 3.8+ available:

```sh
chmod +x bootstrap-minimal.sh
./bootstrap-minimal.sh
```

This installer:
- Requires only Python 3.8+
- No build tools needed
- Installs TSI by copying files directly
- Works on any POSIX-compliant system

### Method 2: Full Bootstrap (No Python Available)

If you don't have Python but have basic build tools (gcc, make):

```sh
chmod +x bootstrap.sh
./bootstrap.sh
```

This installer:
- Bootstraps Python from source
- Requires: C compiler (gcc/clang), make
- Downloads Python source automatically
- Builds and installs Python, then TSI
- Takes longer but works on completely minimal systems

### Method 3: Manual Python Bootstrap

If the automated bootstrap fails, you can manually build Python:

1. **Download Python source:**
   ```sh
   # Using wget
   wget https://www.python.org/ftp/python/3.11.7/Python-3.11.7.tgz

   # Or using curl
   curl -O https://www.python.org/ftp/python/3.11.7/Python-3.11.7.tgz

   # Or download manually and transfer to system
   ```

2. **Extract and build:**
   ```sh
   tar -xzf Python-3.11.7.tgz
   cd Python-3.11.7
   ./configure --prefix=$HOME/.tsi --with-ensurepip=no
   make -j2
   make install
   ```

3. **Install TSI:**
   ```sh
   $HOME/.tsi/bin/python3 bootstrap-minimal.sh
   ```

## Xilinx System Installation

For Xilinx embedded systems (e.g., Zynq, Versal):

### Prerequisites Check

First, check what tools are available:

```sh
# Check for C compiler
which gcc || which cc || which clang

# Check for make
which make

# Check for Python
which python3 || which python

# Check for download tools
which wget || which curl
```

### Installation Steps

1. **If you have Python 3.8+:**
   ```sh
   ./bootstrap-minimal.sh
   ```

2. **If you have gcc and make but no Python:**
   ```sh
   ./bootstrap.sh
   ```

3. **If you have minimal tools:**
   - Transfer Python source tarball to the system
   - Build Python manually (see Method 3 above)
   - Then run `bootstrap-minimal.sh`

### Cross-Compilation Notes

For cross-compilation scenarios:

1. Build Python on host system with cross-compiler
2. Transfer Python installation to target
3. Run `bootstrap-minimal.sh` on target

## Troubleshooting

### "No suitable Python found"

**Solution:** Install Python 3.8+ or use `bootstrap.sh` to build from source.

### "Cannot download file - no download tool available"

**Solutions:**
- Install `wget` or `curl`
- Download files manually and place in work directory
- Use Python's urllib (if Python 2.x or 3.x is available)

### "Missing required build tools"

**Solution:** Install basic build tools:
- C compiler: `gcc` or `clang`
- Build system: `make`
- On Xilinx systems, these may be in a cross-compilation toolchain

### "Python build failed"

**Solutions:**
- Try single-threaded build: `make` instead of `make -j4`
- Check for missing development libraries (libssl-dev, libffi-dev, etc.)
- Use a simpler Python configuration:
  ```sh
  ./configure --prefix=$PREFIX --disable-optimizations --without-ensurepip
  ```

### "Permission denied"

**Solution:** Ensure installation directory is writable:
```sh
mkdir -p $HOME/.tsi
export PREFIX=$HOME/.tsi
```

## Minimal System Requirements

### For bootstrap-minimal.sh:
- Python 3.8+ (any method: system package, pre-built binary, or manually compiled)
- POSIX-compliant shell (`/bin/sh`)
- Basic file operations (cp, mkdir, chmod)

### For bootstrap.sh:
- C compiler (gcc, clang, or cc)
- make
- POSIX-compliant shell
- Download tool (wget, curl, or Python with urllib)
- Basic development libraries (for Python build)

## Post-Installation

After installation, add TSI to your PATH:

```sh
export PATH="$HOME/.tsi/bin:$PATH"
```

Or add to your shell profile:

```sh
echo 'export PATH="$HOME/.tsi/bin:$PATH"' >> ~/.profile
source ~/.profile
```

Verify installation:

```sh
tsi --help
```

## Directory Structure

After installation:

```
$PREFIX/
├── bin/
│   └── tsi          # TSI command
├── lib/
│   └── pythonX.Y/
│       └── site-packages/
│           └── tsi/ # TSI package
└── ...              # (if Python was bootstrapped)
```

## Advanced: Offline Installation

For completely offline systems:

1. **On a system with internet access:**
   - Download Python source tarball
   - Download TSI source (or clone repository)
   - Transfer both to target system

2. **On target system:**
   - Extract Python source
   - Build Python: `./configure --prefix=$PREFIX && make && make install`
   - Extract TSI source
   - Run: `$PREFIX/bin/python3 bootstrap-minimal.sh`

## Support

For issues or questions:
- Check the main README.md
- Review error messages carefully
- Ensure all prerequisites are met
- Try the minimal installer first before full bootstrap

