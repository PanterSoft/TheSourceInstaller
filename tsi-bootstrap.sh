#!/bin/sh
# TSI One-Line Bootstrap Installer
# Downloads TSI source and builds the Rust binary
# Requires: curl or wget, and either a pre-built binary or Rust toolchain (cargo)
# POSIX-compliant shell script

set -e

PREFIX="${PREFIX:-$HOME/.tsi}"
TSI_REPO="${TSI_REPO:-https://github.com/PanterSoft/tsi.git}"
TSI_BRANCH="${TSI_BRANCH:-main}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/tsi-install}"
REPAIR_MODE="${REPAIR:-false}"
if [ "$REPAIR_MODE" = "true" ] || [ "$REPAIR_MODE" = "1" ] || [ "$REPAIR_MODE" = "yes" ]; then
    REPAIR_MODE=true
else
    REPAIR_MODE=false
fi

NON_INTERACTIVE="${NON_INTERACTIVE:-false}"
if [ "$NON_INTERACTIVE" = "true" ] || [ "$NON_INTERACTIVE" = "1" ] || [ "$NON_INTERACTIVE" = "yes" ]; then
    NON_INTERACTIVE=true
else
    NON_INTERACTIVE=false
fi

if [ -d "${PREFIX}/bin" ]; then
    export PATH="${PREFIX}/bin:${PATH}"
fi

log_info() { echo "[INFO] $*"; }
log_warn() { echo "[WARN] $*"; }
log_error() { echo "[ERROR] $*" >&2; }

command_exists() {
    if [ -d "${PREFIX}/bin" ] && [ -x "${PREFIX}/bin/$1" ]; then return 0; fi
    command -v "$1" >/dev/null 2>&1
}

get_command_path() {
    if [ -d "${PREFIX}/bin" ] && [ -x "${PREFIX}/bin/$1" ]; then
        echo "${PREFIX}/bin/$1"
    else
        command -v "$1" 2>/dev/null || echo "$1"
    fi
}

download_file() {
    url="$1"
    output="$2"
    if command_exists curl; then
        "$(get_command_path curl)" -fsSL "$url" -o "$output" || return 1
    elif command_exists wget; then
        "$(get_command_path wget)" -q "$url" -O "$output" || return 1
    else
        return 1
    fi
}

check_tsi_installed() {
    tsi_bin="${PREFIX}/bin/tsi"
    [ -f "$tsi_bin" ] && [ -x "$tsi_bin" ] && return 0
    return 1
}

detect_arch() {
    UNAME_S=$(uname -s 2>/dev/null || echo "Unknown")
    UNAME_M=$(uname -m 2>/dev/null || echo "x86_64")
    case "$UNAME_S" in
        Darwin)  OS="darwin" ;;
        Linux)   OS="linux" ;;
        MINGW*|MSYS*|CYGWIN*) OS="windows" ;;
        *)       OS="" ;;
    esac
    case "$UNAME_M" in
        x86_64|amd64) ARCH="x86_64" ;;
        aarch64|arm64) ARCH="aarch64" ;;
        armv7l) ARCH="armv7" ;;
        *) ARCH="" ;;
    esac
    echo "${OS}-${ARCH}"
}

main() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --repair|repair) REPAIR_MODE=true; shift ;;
            --non-interactive|--yes|-y) NON_INTERACTIVE=true; shift ;;
            --prefix)
                [ $# -lt 2 ] && { log_error "--prefix requires a path"; exit 1; }
                PREFIX="$2"; shift 2
                ;;
            --help|-h|help)
                echo "TSI Bootstrap Installer"
                echo ""
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  --repair          Repair/update existing TSI installation"
                echo "  --prefix PATH     Installation prefix (default: ~/.tsi)"
                echo "  --non-interactive Run without prompts"
                echo "  --help, -h        Show this help"
                echo ""
                echo "Examples:"
                echo "  PREFIX=~/.tsi curl -fsSL .../tsi-bootstrap.sh | sh"
                echo "  REPAIR=1 curl -fsSL .../tsi-bootstrap.sh | sh"
                exit 0
                ;;
            *) log_error "Unknown option: $1"; exit 1 ;;
        esac
    done

    if [ "$REPAIR_MODE" = true ]; then
        log_info "TSI Repair/Update Mode"
    else
        log_info "TSI One-Line Bootstrap Installer"
        log_info "Installation prefix: $PREFIX"
        if check_tsi_installed; then
            if [ "$NON_INTERACTIVE" = true ]; then
                log_error "TSI already installed. Use REPAIR=1 to update."
                exit 1
            fi
            if [ -t 1 ] && [ -c /dev/tty ] 2>/dev/null; then
                { printf "[INFO] TSI already installed. Proceed with fresh install? (yes to continue): " > /dev/tty
                  read -r r < /dev/tty; printf "\n" > /dev/tty; }
                [ "$r" != "yes" ] && { log_info "Cancelled."; exit 0; }
            else
                log_error "TSI already installed. Use REPAIR=1 to update."
                exit 1
            fi
        fi
    fi

    mkdir -p "$INSTALL_DIR"
    cd "$INSTALL_DIR"

    UPDATE_SOURCE=false
    if [ "$REPAIR_MODE" = true ] && [ -d "tsi" ] && [ -f "tsi/Cargo.toml" ]; then
        if command_exists git && [ -d "tsi/.git" ]; then
            log_info "Checking for source updates..."
            cd tsi
            git fetch origin "$TSI_BRANCH" >/dev/null 2>&1 || true
            LOCAL=$(git rev-parse HEAD 2>/dev/null)
            REMOTE=$(git rev-parse "origin/$TSI_BRANCH" 2>/dev/null || true)
            cd ..
            if [ -n "$LOCAL" ] && [ -n "$REMOTE" ] && [ "$LOCAL" != "$REMOTE" ]; then
                log_info "Updating source..."
                cd tsi && git pull origin "$TSI_BRANCH" >/dev/null 2>&1 && cd .. || { cd ..; UPDATE_SOURCE=true; }
            fi
        else
            UPDATE_SOURCE=true
        fi
    fi

    [ "$UPDATE_SOURCE" = true ] && rm -rf tsi

    if [ ! -d "tsi" ] || [ ! -f "tsi/Cargo.toml" ]; then
        log_info "Downloading TSI source..."
        if command_exists git; then
            rm -rf tsi
            if "$(get_command_path git)" clone --depth 1 --branch "$TSI_BRANCH" "$TSI_REPO" tsi 2>&1; then
                log_info "Repository cloned successfully"
            else
                log_error "Git clone failed"
                exit 1
            fi
        else
            log_info "Downloading tarball..."
            tarball_url="https://github.com/PanterSoft/tsi/archive/refs/heads/${TSI_BRANCH}.tar.gz"
            tarball="tsi-${TSI_BRANCH}.tar.gz"
            if ! download_file "$tarball_url" "$tarball"; then
                log_error "Failed to download. Install git or check network."
                exit 1
            fi
            tar -xzf "$tarball" 2>/dev/null || tar -xf "$tarball" 2>/dev/null
            for d in tsi-"$TSI_BRANCH" tsi-main TheSourceInstaller-"$TSI_BRANCH" TheSourceInstaller-main; do
                if [ -d "$d" ] && [ -f "$d/Cargo.toml" ]; then
                    mv "$d" tsi
                    break
                fi
            done
            rm -f "$tarball"
        fi
    fi

    if [ ! -f "tsi/Cargo.toml" ]; then
        log_error "TSI source not found (expected Cargo.toml)"
        exit 1
    fi

    cd tsi

    TSI_BINARY=""
    PLATFORM=$(detect_arch)
    RELEASE_URL="https://github.com/PanterSoft/tsi/releases/latest/download/tsi-${PLATFORM}"

    if [ -n "$PLATFORM" ] && [ "$PLATFORM" != "-" ]; then
        log_info "Trying pre-built binary for $PLATFORM..."
        if download_file "$RELEASE_URL" "tsi-binary" 2>/dev/null; then
            chmod +x tsi-binary 2>/dev/null || true
            if [ -x "tsi-binary" ]; then
                TSI_BINARY="tsi-binary"
                log_info "Using pre-built binary"
            fi
        fi
    fi

    if [ -z "$TSI_BINARY" ] && command_exists cargo; then
        log_info "Building TSI with cargo..."
        if cargo build --release 2>&1; then
            if [ -f "target/release/tsi.exe" ]; then
                TSI_BINARY="target/release/tsi.exe"
            else
                TSI_BINARY="target/release/tsi"
            fi
        else
            log_error "Cargo build failed"
            exit 1
        fi
    fi

    if [ -z "$TSI_BINARY" ]; then
        log_error "Could not obtain TSI binary."
        if [ -n "$PLATFORM" ] && [ "$PLATFORM" != "-" ]; then
            log_error "No pre-built binary available for $PLATFORM."
        fi
        log_error ""
        log_error "Install Rust to build from source:"
        log_error "  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh"
        log_error ""
        log_error "Then run this installer again."
        exit 1
    fi

    if [ ! -f "$TSI_BINARY" ]; then
        log_error "TSI binary not found"
        exit 1
    fi

    log_info "Installing TSI to $PREFIX..."
    mkdir -p "$PREFIX/bin"
    cp "$TSI_BINARY" "$PREFIX/bin/tsi"
    chmod +x "$PREFIX/bin/tsi"

    log_info "Installing shell completions..."
    mkdir -p "$PREFIX/share/completions"
    [ -f "completions/tsi.bash" ] && cp completions/tsi.bash "$PREFIX/share/completions/" && chmod 644 "$PREFIX/share/completions/tsi.bash"
    [ -f "completions/tsi.zsh" ] && cp completions/tsi.zsh "$PREFIX/share/completions/" && chmod 644 "$PREFIX/share/completions/tsi.zsh"

    log_info "Setting up package repository..."
    mkdir -p "$PREFIX/packages"
    if [ -d "packages" ]; then
        count=0
        for f in packages/*.json; do
            [ -f "$f" ] && cp "$f" "$PREFIX/packages/" && count=$((count + 1))
        done
        log_info "  Copied $count package definitions"
    fi

    log_info ""
    log_info "TSI installed successfully!"
    log_info ""
    log_info "Add to PATH:"
    log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
    log_info ""
    log_info "Then run: tsi update   # fetch latest package definitions"
    log_info "         tsi install curl   # install a package"
    log_info ""
}

main "$@"
