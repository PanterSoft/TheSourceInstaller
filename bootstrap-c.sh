#!/bin/sh
# Bootstrap installer for TSI C/C++ version
# Works on systems with only a C compiler

set -e

PREFIX="${PREFIX:-$HOME/.tsi}"
WORK_DIR="${WORK_DIR:-$HOME/tsi-bootstrap-work}"

log_info() {
    echo "[INFO] $*"
}

log_error() {
    echo "[ERROR] $*" >&2
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

main() {
    log_info "TSI C/C++ Bootstrap Installer"
    log_info "=============================="
    log_info ""

    # Check for C compiler
    CC=""
    if command_exists gcc; then
        CC="gcc"
    elif command_exists clang; then
        CC="clang"
    elif command_exists cc; then
        CC="cc"
    else
        log_error "No C compiler found (gcc, clang, or cc required)"
        exit 1
    fi

    log_info "Found C compiler: $CC ($($CC --version 2>&1 | head -1))"

    # Check for make
    if ! command_exists make; then
        log_error "make is required but not found"
        exit 1
    fi

    log_info "Found make: $(make --version 2>&1 | head -1)"

    # Determine TSI source directory
    tsi_dir=$(cd "$(dirname "$0")" && pwd)
    if [ ! -d "$tsi_dir/src" ]; then
        log_error "TSI source directory not found: $tsi_dir/src"
        exit 1
    fi

    # Build TSI
    log_info "Building TSI..."
    cd "$tsi_dir/src"

    # Use custom CC if specified
    if [ -n "$CC" ]; then
        export CC
    fi

    if make clean 2>/dev/null; then
        log_info "Cleaned previous build"
    fi

    if make; then
        log_info "TSI built successfully"
    else
        log_error "Build failed"
        exit 1
    fi

    # Install
    log_info "Installing TSI to $PREFIX..."
    mkdir -p "$PREFIX/bin"
    cp bin/tsi "$PREFIX/bin/tsi"
    chmod +x "$PREFIX/bin/tsi"

    log_info ""
    log_info "TSI installed successfully!"
    log_info ""
    log_info "Add to your PATH:"
    log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
    log_info ""
    log_info "Then run: tsi --help"
}

main "$@"

