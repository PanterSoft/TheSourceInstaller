#!/bin/sh
# Intelligent bootstrap installer for TSI
# Automatically detects available tools and adapts installation procedure
# Works on minimal systems (e.g., Xilinx) with no pre-installed tools

set -e

# Configuration
PREFIX="${PREFIX:-$HOME/.tsi}"
PYTHON_VERSION="${PYTHON_VERSION:-3.11.7}"
PYTHON_URL="https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tgz"
WORK_DIR="${WORK_DIR:-$HOME/tsi-bootstrap-work}"

# Detection flags (will be set by detect_tools)
HAS_PYTHON3=0
HAS_PYTHON=0
PYTHON_CMD=""
PYTHON_VERSION_OK=0
HAS_GCC=0
HAS_CC=0
HAS_CLANG=0
HAS_MAKE=0
HAS_WGET=0
HAS_CURL=0
HAS_FTP=0
HAS_TAR=0
HAS_GZIP=0
HAS_SETUPTOOLS=0
CAN_BUILD_PYTHON=0
CAN_DOWNLOAD=0

# Colors for output
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    BLUE=''
    NC=''
fi

log_info() {
    echo "${GREEN}[INFO]${NC} $*"
}

log_warn() {
    echo "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo "${RED}[ERROR]${NC} $*" >&2
}

log_debug() {
    echo "${BLUE}[DEBUG]${NC} $*"
}

# Check if command exists (POSIX-compliant)
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Detect all available tools
detect_tools() {
    log_info "Detecting available tools..."

    # Check for Python
    if command_exists python3; then
        HAS_PYTHON3=1
        PYTHON_CMD="python3"
        if python3 -c "import sys; exit(0 if sys.version_info >= (3, 8) else 1)" 2>/dev/null; then
            PYTHON_VERSION_OK=1
            log_debug "Found Python3: $(python3 --version 2>&1) - Version OK"
        else
            log_debug "Found Python3: $(python3 --version 2>&1) - Version too old"
        fi
    fi

    if [ "$PYTHON_VERSION_OK" -eq 0 ] && command_exists python; then
        HAS_PYTHON=1
        if [ -z "$PYTHON_CMD" ]; then
            PYTHON_CMD="python"
        fi
        if python -c "import sys; exit(0 if sys.version_info >= (3, 8) else 1)" 2>/dev/null; then
            PYTHON_VERSION_OK=1
            PYTHON_CMD="python"
            log_debug "Found Python: $(python --version 2>&1) - Version OK"
        else
            log_debug "Found Python: $(python --version 2>&1) - Version too old or Python 2"
        fi
    fi

    # Check for previously installed Python in prefix
    if [ "$PYTHON_VERSION_OK" -eq 0 ] && [ -x "$PREFIX/bin/python3" ]; then
        if "$PREFIX/bin/python3" -c "import sys; exit(0 if sys.version_info >= (3, 8) else 1)" 2>/dev/null; then
            PYTHON_VERSION_OK=1
            PYTHON_CMD="$PREFIX/bin/python3"
            log_debug "Found previously installed Python: $PYTHON_CMD"
        fi
    fi

    # Check for C compilers
    if command_exists gcc; then
        HAS_GCC=1
        log_debug "Found gcc: $(gcc --version 2>&1 | head -1)"
    fi

    if command_exists cc; then
        HAS_CC=1
        log_debug "Found cc"
    fi

    if command_exists clang; then
        HAS_CLANG=1
        log_debug "Found clang: $(clang --version 2>&1 | head -1)"
    fi

    # Check for make
    if command_exists make; then
        HAS_MAKE=1
        log_debug "Found make: $(make --version 2>&1 | head -1)"
    fi

    # Check for download tools
    if command_exists wget; then
        HAS_WGET=1
        log_debug "Found wget"
    fi

    if command_exists curl; then
        HAS_CURL=1
        log_debug "Found curl"
    fi

    if command_exists ftp; then
        HAS_FTP=1
        log_debug "Found ftp"
    fi

    # Check for archive tools
    if command_exists tar; then
        HAS_TAR=1
        log_debug "Found tar"
    fi

    if command_exists gzip || command_exists gunzip; then
        HAS_GZIP=1
        log_debug "Found gzip/gunzip"
    fi

    # Check if we can build Python
    if [ "$HAS_MAKE" -eq 1 ] && ([ "$HAS_GCC" -eq 1 ] || [ "$HAS_CC" -eq 1 ] || [ "$HAS_CLANG" -eq 1 ]); then
        CAN_BUILD_PYTHON=1
        log_debug "Can build Python from source"
    fi

    # Check if we can download files
    if [ "$HAS_WGET" -eq 1 ] || [ "$HAS_CURL" -eq 1 ] || [ "$HAS_FTP" -eq 1 ] || [ "$PYTHON_VERSION_OK" -eq 1 ]; then
        CAN_DOWNLOAD=1
        log_debug "Can download files"
    fi

    # Check for setuptools (if Python is available)
    if [ "$PYTHON_VERSION_OK" -eq 1 ]; then
        if "$PYTHON_CMD" -c "import setuptools" 2>/dev/null; then
            HAS_SETUPTOOLS=1
            log_debug "Found setuptools"
        fi
    fi

    # Summary
    log_info "Tool detection summary:"
    [ "$PYTHON_VERSION_OK" -eq 1 ] && log_info "  ✓ Python 3.8+ available: $PYTHON_CMD" || log_warn "  ✗ Python 3.8+ not found"
    [ "$CAN_BUILD_PYTHON" -eq 1 ] && log_info "  ✓ Can build Python from source" || log_warn "  ✗ Cannot build Python (missing: compiler or make)"
    [ "$CAN_DOWNLOAD" -eq 1 ] && log_info "  ✓ Can download files" || log_warn "  ✗ Cannot download files"
    [ "$HAS_SETUPTOOLS" -eq 1 ] && log_info "  ✓ setuptools available" || log_warn "  ✗ setuptools not found"
    echo ""
}

# Download file using best available method
download_file() {
    url="$1"
    dest="$2"

    log_info "Downloading $(basename "$dest")..."

    # Try wget first (most common)
    if [ "$HAS_WGET" -eq 1 ]; then
        if wget -O "$dest" "$url" 2>&1; then
            return 0
        fi
    fi

    # Try curl
    if [ "$HAS_CURL" -eq 1 ]; then
        if curl -L -o "$dest" "$url" 2>&1; then
            return 0
        fi
    fi

    # Try Python urllib (if Python available)
    if [ "$PYTHON_VERSION_OK" -eq 1 ]; then
        if "$PYTHON_CMD" -c "
import urllib.request
try:
    urllib.request.urlretrieve('$url', '$dest')
    exit(0)
except:
    exit(1)
" 2>/dev/null; then
            return 0
        fi
    fi

    # Try Python 2 urllib2
    if [ "$HAS_PYTHON" -eq 1 ] || [ "$HAS_PYTHON3" -eq 1 ]; then
        if python -c "
import urllib2
import sys
try:
    f = urllib2.urlopen('$url')
    with open('$dest', 'wb') as out:
        out.write(f.read())
    sys.exit(0)
except:
    sys.exit(1)
" 2>/dev/null; then
            return 0
        fi
    fi

    # Try ftp (last resort)
    if [ "$HAS_FTP" -eq 1 ]; then
        host=$(echo "$url" | sed 's|https\?://\([^/]*\).*|\1|')
        path=$(echo "$url" | sed 's|https\?://[^/]*\(.*\)|\1|')
        if ftp -n "$host" <<EOF 2>/dev/null
binary
get $path $dest
quit
EOF
        then
            [ -f "$dest" ] && return 0
        fi
    fi

    log_error "Cannot download $url - no download tool available"
    return 1
}

# Extract tarball using available tools
extract_tarball() {
    archive="$1"
    dest="$2"

    log_info "Extracting $(basename "$archive")..."

    if [ "$HAS_TAR" -eq 1 ]; then
        # Try different compression formats
        if tar -xzf "$archive" -C "$dest" 2>/dev/null; then
            return 0
        fi
        if tar -xjf "$archive" -C "$dest" 2>/dev/null; then
            return 0
        fi
        if tar -xf "$archive" -C "$dest" 2>/dev/null; then
            return 0
        fi
    fi

    # Try gunzip + tar
    if [ "$HAS_GZIP" -eq 1 ] && [ "$HAS_TAR" -eq 1 ]; then
        if gunzip -c "$archive" 2>/dev/null | tar -xf - -C "$dest" 2>/dev/null; then
            return 0
        fi
    fi

    log_error "Cannot extract $archive"
    return 1
}

# Build Python from source
build_python() {
    log_info "Building Python ${PYTHON_VERSION} from source..."

    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"

    # Download Python source
    python_archive="Python-${PYTHON_VERSION}.tgz"
    if [ ! -f "$python_archive" ]; then
        if ! download_file "$PYTHON_URL" "$python_archive"; then
            log_error "Failed to download Python source"
            return 1
        fi
    fi

    # Extract
    python_src="Python-${PYTHON_VERSION}"
    if [ ! -d "$python_src" ]; then
        mkdir -p "$python_src"
        if ! extract_tarball "$python_archive" "$WORK_DIR"; then
            log_error "Failed to extract Python source"
            return 1
        fi
    fi

    cd "$python_src"

    # Configure Python
    log_info "Configuring Python..."
    if ! ./configure --prefix="$PREFIX" --with-ensurepip=no --disable-test-modules 2>&1; then
        log_warn "Standard configure failed, trying minimal configuration..."
        if ! ./configure --prefix="$PREFIX" --without-ensurepip 2>&1; then
            log_error "Python configuration failed"
            return 1
        fi
    fi

    # Build Python
    log_info "Building Python (this may take a while)..."
    jobs=1
    if command_exists nproc; then
        jobs=$(nproc)
    elif [ -f /proc/cpuinfo ]; then
        jobs=$(grep -c processor /proc/cpuinfo 2>/dev/null || echo 1)
    fi
    [ "$jobs" -gt 4 ] && jobs=4

    if ! make -j"$jobs" 2>&1; then
        log_warn "Parallel build failed, trying single-threaded build..."
        if ! make 2>&1; then
            log_error "Python build failed"
            return 1
        fi
    fi

    # Install Python
    log_info "Installing Python..."
    if ! make install; then
        log_error "Python installation failed"
        return 1
    fi

    # Verify
    if [ -x "$PREFIX/bin/python3" ]; then
        PYTHON_CMD="$PREFIX/bin/python3"
        PYTHON_VERSION_OK=1
        log_info "Python installed successfully: $($PYTHON_CMD --version 2>&1)"
        return 0
    else
        log_error "Python installation verification failed"
        return 1
    fi
}

# Bootstrap setuptools
bootstrap_setuptools() {
    python_cmd="$1"
    log_info "Bootstrapping setuptools..."

    # Try ensurepip first
    if "$python_cmd" -m ensurepip --version >/dev/null 2>&1; then
        log_info "Using ensurepip..."
        if "$python_cmd" -m ensurepip --upgrade --default-pip 2>&1; then
            if "$python_cmd" -m pip install --upgrade setuptools wheel 2>&1; then
                HAS_SETUPTOOLS=1
                return 0
            fi
        fi
    fi

    # Manual setuptools installation
    if [ "$CAN_DOWNLOAD" -eq 0 ]; then
        log_warn "Cannot download setuptools, TSI will be installed manually"
        return 0
    fi

    log_info "Downloading setuptools..."
    mkdir -p "$WORK_DIR/setuptools"
    cd "$WORK_DIR/setuptools"

    setuptools_url="https://files.pythonhosted.org/packages/source/s/setuptools/setuptools-69.0.0.tar.gz"
    if [ ! -f "setuptools.tar.gz" ]; then
        if ! download_file "$setuptools_url" "setuptools.tar.gz"; then
            log_warn "Could not download setuptools"
            return 0
        fi
    fi

    if [ ! -d "setuptools" ]; then
        mkdir -p setuptools
        if ! extract_tarball "setuptools.tar.gz" "$WORK_DIR/setuptools"; then
            log_warn "Could not extract setuptools"
            return 0
        fi
    fi

    # Find setuptools directory
    setuptools_dir=""
    for dir in setuptools-*; do
        if [ -d "$dir" ] && [ -f "$dir/setup.py" ]; then
            setuptools_dir="$dir"
            break
        fi
    done

    if [ -n "$setuptools_dir" ]; then
        cd "$setuptools_dir"
        if "$python_cmd" setup.py install --prefix="$PREFIX" 2>&1; then
            HAS_SETUPTOOLS=1
            return 0
        fi
    fi

    log_warn "setuptools installation failed, TSI will be installed manually"
    return 0
}

# Install TSI
install_tsi() {
    python_cmd="$1"
    tsi_dir="$2"

    log_info "Installing TSI..."
    cd "$tsi_dir"

    # Determine Python version for site-packages
    py_version=$("$python_cmd" -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')" 2>/dev/null || echo "3")
    site_packages="$PREFIX/lib/python$py_version/site-packages"
    mkdir -p "$site_packages"

    # Try setuptools installation first
    if [ "$HAS_SETUPTOOLS" -eq 1 ]; then
        log_info "Using setuptools to install TSI..."
        if "$python_cmd" setup.py install --prefix="$PREFIX" 2>&1; then
            # Verify installation
            if [ -x "$PREFIX/bin/tsi" ] || "$python_cmd" -m tsi.cli --help >/dev/null 2>&1; then
                return 0
            fi
        fi
        log_warn "setuptools installation failed, falling back to manual installation"
    fi

    # Manual installation
    log_info "Installing TSI manually..."
    cp -r tsi "$site_packages/"

    # Create wrapper script
    mkdir -p "$PREFIX/bin"
    cat > "$PREFIX/bin/tsi" <<EOF
#!/bin/sh
exec "$python_cmd" -c "
import sys
import os
sys.path.insert(0, os.path.expanduser('$site_packages'))
from tsi.cli import main
if __name__ == '__main__':
    main()
" "\$@"
EOF
    chmod +x "$PREFIX/bin/tsi"

    return 0
}

# Main installation process
main() {
    log_info "TSI Intelligent Bootstrap Installer"
    log_info "===================================="
    log_info "Installation prefix: $PREFIX"
    log_info "Work directory: $WORK_DIR"
    log_info ""

    # Determine TSI source directory
    tsi_dir=$(cd "$(dirname "$0")" && pwd)
    if [ ! -d "$tsi_dir/tsi" ]; then
        log_error "TSI source directory not found: $tsi_dir/tsi"
        log_error "Please run this script from the TSI source directory"
        exit 1
    fi

    # Detect available tools
    detect_tools

    # Determine installation strategy
    log_info "Determining installation strategy..."

    # Strategy 1: Python available, just install TSI
    if [ "$PYTHON_VERSION_OK" -eq 1 ]; then
        log_info "Strategy: Use existing Python and install TSI"

        # Bootstrap setuptools if needed
        if [ "$HAS_SETUPTOOLS" -eq 0 ]; then
            bootstrap_setuptools "$PYTHON_CMD"
        fi

        # Install TSI
        if install_tsi "$PYTHON_CMD" "$tsi_dir"; then
            log_info "TSI installed successfully!"
        else
            log_error "TSI installation failed"
            exit 1
        fi

    # Strategy 2: Can build Python, bootstrap everything
    elif [ "$CAN_BUILD_PYTHON" -eq 1 ]; then
        log_info "Strategy: Bootstrap Python from source, then install TSI"

        if [ "$CAN_DOWNLOAD" -eq 0 ]; then
            log_error "Cannot download Python source - no download tool available"
            log_error "Please install wget, curl, or ensure Python is available for urllib"
            exit 1
        fi

        if ! build_python; then
            log_error "Failed to build Python"
            exit 1
        fi

        # Bootstrap setuptools
        if [ "$HAS_SETUPTOOLS" -eq 0 ]; then
            bootstrap_setuptools "$PYTHON_CMD"
        fi

        # Install TSI
        if install_tsi "$PYTHON_CMD" "$tsi_dir"; then
            log_info "TSI installed successfully!"
        else
            log_error "TSI installation failed"
            exit 1
        fi

    # Strategy 3: No Python, cannot build
    else
        log_error "Cannot install TSI:"
        log_error "  - No Python 3.8+ found"
        log_error "  - Cannot build Python (missing: compiler or make)"
        log_error ""
        log_error "Options:"
        log_error "  1. Install Python 3.8+ using your system package manager"
        log_error "  2. Install build tools (gcc/clang, make) and run this script again"
        log_error "  3. Build Python manually and then run this script"
        log_error ""
        log_error "To build Python manually:"
        log_error "  - Download: $PYTHON_URL"
        log_error "  - Extract and run: ./configure --prefix=$PREFIX && make && make install"
        exit 1
    fi

    # Success message
    log_info ""
    log_info "=========================================="
    log_info "TSI installed successfully!"
    log_info "=========================================="
    log_info ""
    log_info "Add to your PATH:"
    log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
    log_info ""
    log_info "Or add to your shell profile:"
    log_info "  echo 'export PATH=\"$PREFIX/bin:\\\$PATH\"' >> ~/.profile"
    log_info ""

    # Test installation
    if [ -x "$PREFIX/bin/tsi" ]; then
        log_info "Testing installation..."
        if "$PREFIX/bin/tsi" --help >/dev/null 2>&1; then
            log_info "✓ TSI is working correctly!"
        else
            log_warn "TSI installed but test failed"
        fi
    elif "$PYTHON_CMD" -m tsi.cli --help >/dev/null 2>&1; then
        log_info "TSI is available via: $PYTHON_CMD -m tsi.cli"
    fi
    log_info ""
}

# Run main function
main "$@"

