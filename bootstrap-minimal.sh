#!/bin/sh
# Ultra-minimal bootstrap installer for TSI
# For systems with absolutely minimal tools (e.g., Xilinx embedded systems)
# This version makes minimal assumptions and provides manual fallbacks

set -e

# Configuration
PREFIX="${PREFIX:-$HOME/.tsi}"
WORK_DIR="${WORK_DIR:-$HOME/tsi-work}"

log_info() {
    echo "[INFO] $*"
}

log_warn() {
    echo "[WARN] $*"
}

log_error() {
    echo "[ERROR] $*" >&2
}

# Check if command exists (POSIX-compliant)
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Main installation - minimal version
main() {
    log_info "TSI Minimal Bootstrap Installer"
    log_info "================================"
    log_info ""

    # Determine TSI source directory
    tsi_dir=$(cd "$(dirname "$0")" && pwd)
    if [ ! -d "$tsi_dir/tsi" ]; then
        log_error "TSI source directory not found"
        exit 1
    fi

    # Check for Python
    python_cmd=""
    if command_exists python3; then
        if python3 -c "import sys; exit(0 if sys.version_info >= (3, 8) else 1)" 2>/dev/null; then
            python_cmd="python3"
            log_info "Found Python: $(python3 --version 2>&1)"
        fi
    fi

    if [ -z "$python_cmd" ] && command_exists python; then
        if python -c "import sys; exit(0 if sys.version_info >= (3, 8) else 1)" 2>/dev/null; then
            python_cmd="python"
            log_info "Found Python: $(python --version 2>&1)"
        fi
    fi

    if [ -z "$python_cmd" ]; then
        log_error "Python 3.8+ is required but not found"
        log_error ""
        log_error "Options:"
        log_error "1. Install Python 3.8+ using your system package manager"
        log_error "2. Build Python from source (requires C compiler and make)"
        log_error "3. Use the full bootstrap.sh script to bootstrap Python"
        log_error ""
        log_error "To bootstrap Python manually:"
        log_error "  - Download Python source from https://www.python.org/downloads/"
        log_error "  - Extract and run: ./configure --prefix=$PREFIX && make && make install"
        exit 1
    fi

    # Install TSI manually (no setuptools required)
    log_info "Installing TSI to $PREFIX..."
    mkdir -p "$PREFIX/bin"
    mkdir -p "$PREFIX/lib"

    # Determine Python version for site-packages
    py_version=$("$python_cmd" -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')" 2>/dev/null || echo "3")
    site_packages="$PREFIX/lib/python$py_version/site-packages"
    mkdir -p "$site_packages"

    # Copy TSI package
    log_info "Copying TSI files..."
    cp -r "$tsi_dir/tsi" "$site_packages/"

    # Create wrapper script
    log_info "Creating tsi command..."
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
    log_info "Verifying installation..."
    if "$PREFIX/bin/tsi" --help >/dev/null 2>&1; then
        log_info "TSI installed successfully!"
    else
        log_warn "Installation completed but verification failed"
        log_warn "You can try running: $python_cmd -c 'import sys; sys.path.insert(0, \"$site_packages\"); from tsi.cli import main; main()' --help"
    fi

    log_info ""
    log_info "Add to PATH:"
    log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
    log_info ""
    log_info "Then run: tsi --help"
}

main "$@"

