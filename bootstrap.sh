#!/bin/sh
# POSIX-compliant bootstrap installer for TSI
# Works on minimal systems (e.g., Xilinx) with no pre-installed tools
# Bootstraps Python and TSI from source

set -e

# Configuration
PREFIX="${PREFIX:-$HOME/.tsi-bootstrap}"
PYTHON_VERSION="${PYTHON_VERSION:-3.11.7}"
PYTHON_URL="https://www.python.org/ftp/python/${PYTHON_VERSION}/Python-${PYTHON_VERSION}.tgz"
WORK_DIR="${WORK_DIR:-$HOME/tsi-bootstrap-work}"

# Colors for output (optional, works without if terminal doesn't support)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    NC='\033[0m' # No Color
else
    RED=''
    GREEN=''
    YELLOW=''
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

# Check if command exists (POSIX-compliant)
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Download file using available tool (POSIX-compliant)
download_file() {
    url="$1"
    dest="$2"

    log_info "Downloading $url..."

    # Try wget first
    if command_exists wget; then
        wget -O "$dest" "$url" && return 0
    fi

    # Try curl
    if command_exists curl; then
        curl -L -o "$dest" "$url" && return 0
    fi

    # Try ftp (if available)
    if command_exists ftp; then
        # Extract host and path from URL
        host=$(echo "$url" | sed 's|https\?://\([^/]*\).*|\1|')
        path=$(echo "$url" | sed 's|https\?://[^/]*\(.*\)|\1|')
        ftp -n "$host" <<EOF
binary
get $path $dest
quit
EOF
        [ -f "$dest" ] && return 0
    fi

    # Last resort: try Python urllib if Python 2.x or 3.x is available
    if command_exists python; then
        python -c "
import urllib2
import sys
try:
    f = urllib2.urlopen('$url')
    with open('$dest', 'wb') as out:
        out.write(f.read())
    sys.exit(0)
except:
    sys.exit(1)
" 2>/dev/null && return 0
    fi

    if command_exists python3; then
        python3 -c "
import urllib.request
try:
    urllib.request.urlretrieve('$url', '$dest')
except:
    exit(1)
" 2>/dev/null && return 0
    fi

    log_error "Cannot download $url - no download tool available"
    log_error "Please install wget, curl, or ensure Python is available"
    return 1
}

# Extract tarball (POSIX-compliant)
extract_tarball() {
    archive="$1"
    dest="$2"

    log_info "Extracting $archive..."

    # Try tar with different compression options
    if command_exists tar; then
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

    # Try gunzip + tar if available
    if command_exists gunzip && command_exists tar; then
        gunzip -c "$archive" | tar -xf - -C "$dest" 2>/dev/null && return 0
    fi

    log_error "Cannot extract $archive - tar not available or archive format not supported"
    return 1
}

# Check for minimal build requirements
check_build_tools() {
    missing=""

    if ! command_exists cc && ! command_exists gcc && ! command_exists clang; then
        missing="$missing C compiler (cc/gcc/clang)"
    fi

    if ! command_exists make; then
        missing="$missing make"
    fi

    if [ -n "$missing" ]; then
        log_error "Missing required build tools:$missing"
        log_error "Cannot bootstrap Python without a C compiler and make"
        log_error "Please install basic build tools first"
        return 1
    fi

    return 0
}

# Build Python from source
bootstrap_python() {
    log_info "Bootstrapping Python ${PYTHON_VERSION} from source..."

    # Check build tools
    if ! check_build_tools; then
        return 1
    fi

    # Create work directory
    mkdir -p "$WORK_DIR"
    cd "$WORK_DIR"

    # Download Python source
    python_archive="Python-${PYTHON_VERSION}.tgz"
    if [ ! -f "$python_archive" ]; then
        download_file "$PYTHON_URL" "$python_archive"
    fi

    # Extract
    python_src="Python-${PYTHON_VERSION}"
    if [ ! -d "$python_src" ]; then
        mkdir -p "$python_src"
        extract_tarball "$python_archive" "$WORK_DIR"
    fi

    cd "$python_src"

    # Configure Python (minimal build)
    log_info "Configuring Python..."
    # Try minimal configuration first
    if ! ./configure \
        --prefix="$PREFIX" \
        --with-ensurepip=no \
        --disable-test-modules \
        --without-ensurepip 2>&1; then
        # Fallback: even more minimal
        log_warn "Standard configure failed, trying minimal configuration..."
        ./configure --prefix="$PREFIX" --without-ensurepip || {
            log_error "Python configuration failed"
            return 1
        }
    fi

    # Build Python
    log_info "Building Python (this may take a while)..."
    # Determine number of jobs (POSIX-compliant)
    if command_exists nproc; then
        jobs=$(nproc)
    elif [ -f /proc/cpuinfo ]; then
        jobs=$(grep -c processor /proc/cpuinfo 2>/dev/null || echo 1)
    else
        jobs=1
    fi
    [ "$jobs" -gt 4 ] && jobs=4  # Limit to 4 to avoid overwhelming system

    if ! make -j"$jobs" 2>&1; then
        log_warn "Parallel build failed, trying single-threaded build..."
        make 2>&1 || {
            log_error "Python build failed"
            return 1
        }
    fi

    # Install Python
    log_info "Installing Python..."
    make install

    # Verify installation
    if [ -x "$PREFIX/bin/python3" ]; then
        log_info "Python installed successfully"
        "$PREFIX/bin/python3" --version
        return 0
    else
        log_error "Python installation failed"
        return 1
    fi
}

# Install setuptools bootstrap
bootstrap_setuptools() {
    python_cmd="$1"
    log_info "Bootstrapping setuptools..."

    # Try to use ensurepip if available
    if "$python_cmd" -m ensurepip --version >/dev/null 2>&1; then
        log_info "Using ensurepip to install setuptools..."
        "$python_cmd" -m ensurepip --upgrade --default-pip
        "$python_cmd" -m pip install --upgrade setuptools wheel
        return 0
    fi

    # Fallback: download and install setuptools manually
    log_info "Installing setuptools manually..."
    mkdir -p "$WORK_DIR/setuptools"
    cd "$WORK_DIR/setuptools"

    # Download setuptools (try latest version)
    setuptools_url="https://files.pythonhosted.org/packages/source/s/setuptools/setuptools-69.0.0.tar.gz"
    if [ ! -f "setuptools.tar.gz" ]; then
        if ! download_file "$setuptools_url" "setuptools.tar.gz"; then
            log_warn "Could not download setuptools, TSI will be installed without it"
            return 0
        fi
    fi

    if [ ! -d "setuptools" ]; then
        mkdir -p setuptools
        if ! extract_tarball "setuptools.tar.gz" "$WORK_DIR/setuptools"; then
            log_warn "Could not extract setuptools, TSI will be installed without it"
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
        "$python_cmd" setup.py install --prefix="$PREFIX" || {
            log_warn "setuptools installation failed, TSI will be installed manually"
            return 0
        }
    else
        log_warn "Could not find setuptools source, TSI will be installed manually"
        return 0
    fi
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

    # Try using setuptools
    if "$python_cmd" -c "import setuptools" 2>/dev/null; then
        log_info "Using setuptools to install TSI..."
        if "$python_cmd" setup.py install --prefix="$PREFIX" 2>&1; then
            # Verify setuptools installation worked
            if [ -x "$PREFIX/bin/tsi" ] || "$python_cmd" -m tsi.cli --help >/dev/null 2>&1; then
                return 0
            fi
        fi
        log_warn "setuptools installation failed, falling back to manual installation"
    fi

    # Manual installation: copy files
    log_info "Installing TSI manually (no setuptools)..."
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

    # Verify installation
    if [ -x "$PREFIX/bin/tsi" ] || "$python_cmd" -m tsi.cli --help >/dev/null 2>&1; then
        log_info "TSI installed successfully"
        return 0
    else
        log_error "TSI installation verification failed"
        return 1
    fi
}

# Main installation process
main() {
    log_info "TSI Bootstrap Installer"
    log_info "========================"
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

    # Find or bootstrap Python
    python_cmd=""

    # Check for existing Python 3.8+
    if command_exists python3; then
        if python3 -c "import sys; exit(0 if sys.version_info >= (3, 8) else 1)" 2>/dev/null; then
            python_cmd="python3"
            log_info "Found suitable Python: $(python3 --version 2>&1)"
        fi
    fi

    if [ -z "$python_cmd" ] && [ -x "$PREFIX/bin/python3" ]; then
        if "$PREFIX/bin/python3" -c "import sys; exit(0 if sys.version_info >= (3, 8) else 1)" 2>/dev/null; then
            python_cmd="$PREFIX/bin/python3"
            log_info "Found previously installed Python: $($python_cmd --version 2>&1)"
        fi
    fi

    # Bootstrap Python if needed
    if [ -z "$python_cmd" ]; then
        log_warn "No suitable Python found, bootstrapping from source..."
        if ! bootstrap_python; then
            log_error "Failed to bootstrap Python"
            exit 1
        fi
        python_cmd="$PREFIX/bin/python3"
    fi

    # Bootstrap setuptools if needed
    if ! "$python_cmd" -c "import setuptools" 2>/dev/null; then
        log_warn "setuptools not found, bootstrapping..."
        bootstrap_setuptools "$python_cmd"
    fi

    # Install TSI
    if ! install_tsi "$python_cmd" "$tsi_dir"; then
        log_error "Failed to install TSI"
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
    log_info "Then run: tsi --help"
    log_info ""

    # Test installation
    if [ -x "$PREFIX/bin/tsi" ]; then
        log_info "Testing installation..."
        "$PREFIX/bin/tsi" --help >/dev/null 2>&1 && log_info "TSI is working correctly!" || log_warn "TSI installed but test failed"
    elif "$python_cmd" -m tsi.cli --help >/dev/null 2>&1; then
        log_info "TSI is available via: $python_cmd -m tsi.cli"
    fi
}

# Run main function
main "$@"

