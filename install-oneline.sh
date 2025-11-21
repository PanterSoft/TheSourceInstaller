#!/bin/sh
# One-line installer script for TSI
# Downloads TSI source and runs bootstrap installer

set -e

# Configuration
TSI_REPO="${TSI_REPO:-https://github.com/PanterSoft/tsi.git}"
TSI_BRANCH="${TSI_BRANCH:-main}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/tsi-install}"
PREFIX="${PREFIX:-$HOME/.tsi}"

# Colors
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    NC='\033[0m'
else
    GREEN=''
    YELLOW=''
    NC=''
fi

log_info() {
    echo "${GREEN}[TSI Installer]${NC} $*"
}

log_warn() {
    echo "${YELLOW}[TSI Installer]${NC} $*"
}

# Check for download tools
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

download_tarball() {
    url="$1"
    dest="$2"

    if command_exists curl; then
        curl -fsSL "$url" -o "$dest" && return 0
    fi

    if command_exists wget; then
        wget -q "$url" -O "$dest" && return 0
    fi

    if command_exists python3; then
        python3 -c "
import urllib.request
try:
    urllib.request.urlretrieve('$url', '$dest')
    exit(0)
except:
    exit(1)
" 2>/dev/null && return 0
    fi

    return 1
}

# Main installation
main() {
    log_info "TSI One-Line Installer"
    log_info "======================"
    log_info ""

    # Create install directory
    mkdir -p "$INSTALL_DIR"
    cd "$INSTALL_DIR"

    # Check if TSI is already downloaded
    if [ -d "tsi" ] && [ -f "tsi/bootstrap-default.sh" ]; then
        log_info "TSI source already exists, using existing copy"
        cd tsi
    else
        log_info "Downloading TSI source code..."

        # Try git clone first (if git is available)
        if command_exists git; then
            log_info "Cloning TSI repository..."
            if [ -d "tsi" ]; then
                rm -rf tsi
            fi
            if git clone --depth 1 --branch "$TSI_BRANCH" "$TSI_REPO" tsi 2>&1; then
                cd tsi
                log_info "Repository cloned successfully"
            else
                log_warn "Git clone failed, trying tarball download..."
                rm -rf tsi
            fi
        fi

            # Fallback: Download as tarball
        if [ ! -d "tsi" ] || [ ! -f "tsi/bootstrap-default.sh" ]; then
            log_info "Downloading TSI as tarball..."
            tarball_url="https://github.com/PanterSoft/tsi/archive/refs/heads/${TSI_BRANCH}.tar.gz"
            tarball="tsi-${TSI_BRANCH}.tar.gz"

            if download_tarball "$tarball_url" "$tarball"; then
                log_info "Extracting tarball..."
                if command_exists tar; then
                    if tar -xzf "$tarball" 2>/dev/null || tar -xf "$tarball" 2>/dev/null; then
                        # Rename extracted directory
                        if [ -d "tsi-${TSI_BRANCH}" ]; then
                            mv "tsi-${TSI_BRANCH}" tsi
                        elif [ -d "tsi-main" ]; then
                            mv "tsi-main" tsi
                        fi
                        rm -f "$tarball"
                        if [ -d "tsi" ]; then
                            cd tsi
                            log_info "Tarball extracted successfully"
                        else
                            log_warn "Extracted directory not found"
                            return 1
                        fi
                    else
                        log_warn "Failed to extract tarball"
                        rm -f "$tarball"
                        return 1
                    fi
                else
                    log_warn "tar not found, cannot extract tarball"
                    rm -f "$tarball"
                    return 1
                fi
            else
                log_warn "Failed to download TSI source"
                log_warn "Please ensure you have curl, wget, or python3 available"
                return 1
            fi
        fi
    fi

    # Verify bootstrap script exists
    if [ ! -f "bootstrap-default.sh" ]; then
        log_warn "bootstrap-default.sh not found in TSI source"
        return 1
    fi

    # Make bootstrap script executable
    chmod +x bootstrap-default.sh

    log_info ""
    log_info "Running TSI bootstrap installer..."
    log_info ""

    # Run bootstrap installer
    PREFIX="$PREFIX" ./bootstrap-default.sh

    log_info ""
    log_info "Installation complete!"
    log_info ""
    log_info "Add to your PATH:"
    log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
    log_info ""
    log_info "Or add to your shell profile:"
    log_info "  echo 'export PATH=\"$PREFIX/bin:\\\$PATH\"' >> ~/.bashrc"
    log_info ""
}

main "$@"

