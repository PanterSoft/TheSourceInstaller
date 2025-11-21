# TSI Bootstrap Installation

Quick reference for bootstrapping TSI on minimal systems (Xilinx, embedded, etc.).

## Quick Start (Recommended)

### Intelligent Bootstrap (Auto-detects tools)
```sh
./bootstrap-default.sh
```
**Automatically:**
- Detects available tools (Python, compilers, download tools)
- Chooses the best installation method
- Bootstraps missing dependencies if possible
- Works with any combination of available tools

## Manual Options

### Option 1: Minimal Install (Python Available)
```sh
./bootstrap-minimal.sh
```
**Requirements:** Python 3.8+ only

### Option 2: Full Bootstrap (No Python)
```sh
./bootstrap.sh
```
**Requirements:** C compiler (gcc/clang), make, download tool (wget/curl)

## What Gets Installed

- **Python 3.11.7** (if not available, built from source)
- **setuptools** (if needed, bootstrapped automatically)
- **TSI** (installed to `$PREFIX`, default: `$HOME/.tsi`)

## Installation Locations

- **Prefix:** `$HOME/.tsi` (or set `PREFIX=/custom/path`)
- **Work Directory:** `$HOME/tsi-bootstrap-work` (or set `WORK_DIR=/custom/path`)
- **Python:** `$PREFIX/bin/python3`
- **TSI:** `$PREFIX/bin/tsi`

## Environment Variables

```sh
# Custom installation prefix
export PREFIX=/opt/tsi
./bootstrap.sh

# Custom Python version
export PYTHON_VERSION=3.10.12
./bootstrap.sh

# Custom work directory
export WORK_DIR=/tmp/tsi-build
./bootstrap.sh
```

## After Installation

```sh
# Add to PATH
export PATH="$HOME/.tsi/bin:$PATH"

# Verify
tsi --help
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| No Python found | Use `bootstrap.sh` to build Python |
| No C compiler | Install gcc/clang or use pre-built Python |
| Download fails | Install wget/curl or download manually |
| Build fails | Try single-threaded: `make` instead of `make -j4` |
| Permission denied | Set `PREFIX` to writable location |

## Manual Steps (If Bootstrap Fails)

1. **Build Python:**
   ```sh
   wget https://www.python.org/ftp/python/3.11.7/Python-3.11.7.tgz
   tar -xzf Python-3.11.7.tgz
   cd Python-3.11.7
   ./configure --prefix=$HOME/.tsi
   make && make install
   ```

2. **Install TSI:**
   ```sh
   $HOME/.tsi/bin/python3 bootstrap-minimal.sh
   ```

## POSIX Compliance

Both bootstrap scripts use only POSIX-compliant shell features:
- `/bin/sh` (not bash-specific)
- Standard commands only
- Works on any POSIX system

## See Also

- [INSTALL.md](INSTALL.md) - Detailed installation guide
- [README.md](README.md) - Main documentation

