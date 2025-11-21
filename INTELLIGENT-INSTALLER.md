# Intelligent Bootstrap Installer

The `bootstrap-default.sh` script is an intelligent installer that automatically detects available tools and adapts the installation procedure accordingly.

## How It Works

### 1. Tool Detection Phase

The installer detects the following tools:

**Python:**
- `python3` (checks if version >= 3.8)
- `python` (checks if version >= 3.8)
- Previously installed Python in `$PREFIX/bin/python3`

**Compilers:**
- `gcc`
- `cc`
- `clang`

**Build Tools:**
- `make`

**Download Tools:**
- `wget`
- `curl`
- `ftp`
- Python `urllib` (if Python available)

**Archive Tools:**
- `tar`
- `gzip`/`gunzip`

**Python Packages:**
- `setuptools` (if available)

### 2. Capability Assessment

Based on detected tools, the installer determines:

- **CAN_BUILD_PYTHON**: Has compiler + make
- **CAN_DOWNLOAD**: Has wget/curl/ftp or Python with urllib
- **PYTHON_VERSION_OK**: Has Python 3.8+
- **HAS_SETUPTOOLS**: Has setuptools available

### 3. Installation Strategy Selection

The installer chooses one of three strategies:

#### Strategy 1: Direct Installation (Python Available)
```
Python 3.8+ found → Install TSI directly
```
- Fastest method
- Bootstraps setuptools if needed
- Installs TSI using setuptools or manual copy

#### Strategy 2: Bootstrap Python (Can Build)
```
No Python + Can Build → Build Python → Install TSI
```
- Downloads Python source
- Builds Python from source
- Bootstraps setuptools
- Installs TSI

#### Strategy 3: Error (Cannot Proceed)
```
No Python + Cannot Build → Show error with options
```
- Provides clear error message
- Lists available options
- Gives manual installation instructions

## Example Scenarios

### Scenario 1: Full System (Xilinx with Build Tools)
```
Detected: gcc, make, wget, python3 (too old)
Strategy: Build Python 3.11.7 → Install TSI
```

### Scenario 2: Minimal System (Python Pre-installed)
```
Detected: python3 (3.9.0), setuptools
Strategy: Direct installation using setuptools
```

### Scenario 3: Embedded System (No Tools)
```
Detected: python3 (3.10.0), no setuptools
Strategy: Direct installation (manual copy)
```

### Scenario 4: Completely Minimal (Xilinx Bare)
```
Detected: gcc, make, curl
Strategy: Download & build Python → Install TSI
```

## Features

### Intelligent Fallbacks

1. **Download Methods**: Tries wget → curl → Python urllib → ftp
2. **Build Methods**: Tries parallel build → single-threaded
3. **Installation Methods**: Tries setuptools → manual copy
4. **Python Detection**: Checks system Python → prefix Python → version validation

### Clear Feedback

- Color-coded output (if terminal supports)
- Debug messages show what tools were found
- Summary of capabilities
- Clear error messages with solutions

### POSIX Compliance

- Uses only `/bin/sh` (not bash-specific)
- Standard POSIX commands only
- Works on any POSIX-compliant system

## Usage

```sh
# Basic usage
./bootstrap-default.sh

# Custom prefix
PREFIX=/opt/tsi ./bootstrap-default.sh

# Custom Python version (if building)
PYTHON_VERSION=3.10.12 ./bootstrap-default.sh

# Custom work directory
WORK_DIR=/tmp/build ./bootstrap-default.sh
```

## Output Example

```
[INFO] TSI Intelligent Bootstrap Installer
[INFO] ====================================
[INFO] Installation prefix: /home/user/.tsi
[INFO] Work directory: /home/user/tsi-bootstrap-work

[INFO] Detecting available tools...
[DEBUG] Found Python3: Python 3.9.5 - Version OK
[DEBUG] Found gcc: gcc (GCC) 9.3.0
[DEBUG] Found make: GNU Make 4.2.1
[DEBUG] Found wget
[DEBUG] Found setuptools
[INFO] Tool detection summary:
[INFO]   ✓ Python 3.8+ available: python3
[INFO]   ✓ Can build Python from source
[INFO]   ✓ Can download files
[INFO]   ✓ setuptools available

[INFO] Determining installation strategy...
[INFO] Strategy: Use existing Python and install TSI
[INFO] Installing TSI...
[INFO] Using setuptools to install TSI...
[INFO] TSI installed successfully!
```

## Comparison with Other Installers

| Feature | bootstrap-default.sh | bootstrap.sh | bootstrap-minimal.sh |
|---------|---------------------|--------------|---------------------|
| Tool Detection | ✓ Automatic | ✗ Manual | ✗ Manual |
| Strategy Selection | ✓ Automatic | ✗ Fixed | ✗ Fixed |
| Python Bootstrap | ✓ If needed | ✓ Always | ✗ No |
| Fallbacks | ✓ Multiple | ✓ Some | ✗ Minimal |
| Best For | All systems | No Python | Python available |

## Troubleshooting

### "Cannot install TSI: No Python 3.8+ found"

**Solution:** The installer detected no suitable Python. Options:
1. Install Python 3.8+ manually
2. Install build tools (gcc, make) and let installer build Python
3. Use pre-built Python binary

### "Cannot download Python source"

**Solution:** No download tool available. Options:
1. Install `wget` or `curl`
2. Download Python source manually and place in work directory
3. Use Python's urllib (if Python 2.x or 3.x available)

### "Python build failed"

**Solution:** Build issues. Try:
1. Single-threaded build (installer does this automatically)
2. Check for missing development libraries
3. Use a simpler Python configuration
4. Build Python manually first

## Advantages

1. **Zero Configuration**: Just run it, it figures out the rest
2. **Works Everywhere**: Adapts to any system configuration
3. **User-Friendly**: Clear messages about what's happening
4. **Robust**: Multiple fallbacks for each operation
5. **Efficient**: Chooses fastest available method

## See Also

- [INSTALL.md](INSTALL.md) - Detailed installation guide
- [BOOTSTRAP.md](BOOTSTRAP.md) - Quick reference
- [README.md](README.md) - Main documentation

