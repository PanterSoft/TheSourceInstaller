#!/bin/sh
# TSI One-Line Bootstrap Installer
# Installs the pre-built tsi binary (or builds it with cargo), fetches package
# definitions and puts tsi on PATH. Running it again updates the installation.
# Requires: curl or wget, and either a pre-built binary or Rust toolchain (cargo)
# POSIX-compliant shell script

set -e

PREFIX="${PREFIX:-$HOME/.tsi}"
TSI_REPO="${TSI_REPO:-https://github.com/PanterSoft/TheSourceInstaller.git}"
TSI_BRANCH="${TSI_BRANCH:-main}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/tsi-install}"
REPAIR_MODE="${REPAIR:-false}"
if [ "$REPAIR_MODE" = "true" ] || [ "$REPAIR_MODE" = "1" ] || [ "$REPAIR_MODE" = "yes" ]; then
    REPAIR_MODE=true
else
    REPAIR_MODE=false
fi

UNINSTALL_MODE="${UNINSTALL:-false}"
if [ "$UNINSTALL_MODE" = "true" ] || [ "$UNINSTALL_MODE" = "1" ] || [ "$UNINSTALL_MODE" = "yes" ]; then
    UNINSTALL_MODE=true
else
    UNINSTALL_MODE=false
fi

NON_INTERACTIVE="${NON_INTERACTIVE:-false}"
if [ "$NON_INTERACTIVE" = "true" ] || [ "$NON_INTERACTIVE" = "1" ] || [ "$NON_INTERACTIVE" = "yes" ]; then
    NON_INTERACTIVE=true
else
    NON_INTERACTIVE=false
fi

# The user's own PATH, before this script adds the prefix to it: tells whether
# TSI is already reachable from their shells.
USER_PATH="$PATH"

if [ -d "${PREFIX}/bin" ]; then
    export PATH="${PREFIX}/bin:${PATH}"
fi

# TSI palette (docs/brand.md): brand Terminal Red for info, yellow for
# warnings, bright bold red for errors. Only on a terminal, and never when
# NO_COLOR is set (https://no-color.org).
C_BRAND='' C_WARN='' C_ERR='' C_RESET=''
if [ -z "${NO_COLOR:-}" ] && [ -t 1 ]; then
    C_BRAND=$(printf '\033[38;5;167m')
    C_WARN=$(printf '\033[33m')
    C_RESET=$(printf '\033[0m')
fi
if [ -z "${NO_COLOR:-}" ] && [ -t 2 ]; then
    C_ERR=$(printf '\033[1;91m')
    C_ERR_RESET=$(printf '\033[0m')
else
    C_ERR_RESET=''
fi

log_info() { printf '%s[INFO]%s %s\n' "$C_BRAND" "$C_RESET" "$*"; }
log_warn() { printf '%s[WARN]%s %s\n' "$C_WARN" "$C_RESET" "$*"; }
log_error() { printf '%s[ERROR]%s %s\n' "$C_ERR" "$C_ERR_RESET" "$*" >&2; }

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

# Every line the installer writes into a shell profile ends with this, so
# uninstall can find and remove exactly those lines.
PATH_MARKER="# added by the TSI installer"

# The startup file of the user's shell, where a PATH line takes effect.
shell_profile() {
    case "$(basename "${SHELL:-sh}")" in
        zsh) echo "${ZDOTDIR:-$HOME}/.zshrc" ;;
        bash)
            # macOS Terminal opens login shells, which read .bash_profile.
            if [ "$(uname -s 2>/dev/null)" = Darwin ]; then
                echo "$HOME/.bash_profile"
            else
                echo "$HOME/.bashrc"
            fi
            ;;
        fish) echo "$HOME/.config/fish/conf.d/tsi.fish" ;;
        *) echo "$HOME/.profile" ;;
    esac
}

# Puts $1 on PATH for new shells by adding one marked line to the shell
# profile. Returns 0 when TSI is (or will be) on PATH, 1 when the user has to
# do it. Skips the edit when $1 is already on PATH, already in the profile, or
# TSI_NO_MODIFY_PATH is set.
setup_path() {
    bin="$1"
    case ":$USER_PATH:" in *":$bin:"*) return 0 ;; esac
    case "${TSI_NO_MODIFY_PATH:-}" in 1|true|yes) return 1 ;; esac
    # Write $HOME rather than the expanded path, so the line reads as usual.
    case "$bin" in
        "$HOME"/*) shown="\$HOME${bin#"$HOME"}" ;;
        *) shown="$bin" ;;
    esac
    profile=$(shell_profile)
    if [ -f "$profile" ] && grep -F "$PATH_MARKER" "$profile" | grep -qF "\"$shown"; then
        return 0
    fi
    mkdir -p "$(dirname "$profile")" 2>/dev/null || true
    case "$profile" in
        *.fish) line="fish_add_path \"$shown\"  $PATH_MARKER" ;;
        *) line="export PATH=\"$shown:\$PATH\"  $PATH_MARKER" ;;
    esac
    printf '\n%s\n' "$line" >> "$profile" 2>/dev/null || return 1
    PATH_PROFILE="$profile"
    return 0
}

# Removes the lines setup_path added, from every profile it may have used.
remove_path_setup() {
    for f in "${ZDOTDIR:-$HOME}/.zshrc" "$HOME/.bashrc" "$HOME/.bash_profile" "$HOME/.profile"; do
        [ -f "$f" ] && grep -qF "$PATH_MARKER" "$f" || continue
        { grep -vF "$PATH_MARKER" "$f" || true; } > "$f.tsi-tmp" && cat "$f.tsi-tmp" > "$f"
        rm -f "$f.tsi-tmp"
        log_info "Removed TSI from PATH in $f"
    done
    rm -f "$HOME/.config/fish/conf.d/tsi.fish"
}

check_tsi_installed() {
    tsi_bin="${PREFIX}/bin/tsi"
    [ -f "$tsi_bin" ] && [ -x "$tsi_bin" ] && return 0
    [ -f "${PREFIX}/bin/tsi.exe" ] && [ -x "${PREFIX}/bin/tsi.exe" ] && return 0
    return 1
}

run_uninstall() {
    PREFIX_ABS="$PREFIX"
    firstchar="${PREFIX_ABS%"${PREFIX_ABS#?}"}"
    if [ "$firstchar" = "~" ]; then
        if [ "$PREFIX_ABS" = "~" ]; then
            PREFIX_ABS="$HOME"
        else
            PREFIX_ABS="$HOME/${PREFIX_ABS#?}"
        fi
    fi
    if [ ! -d "$PREFIX_ABS" ]; then
        log_error "No TSI installation found at $PREFIX_ABS"
        exit 1
    fi
    # Strip trailing slashes so "/usr/" matches "/usr" below.
    while [ "${#PREFIX_ABS}" -gt 1 ] && [ "${PREFIX_ABS%/}" != "$PREFIX_ABS" ]; do
        PREFIX_ABS="${PREFIX_ABS%/}"
    done
    case "$PREFIX_ABS" in
        /|/usr|/usr/local|/opt|/bin|/sbin|/lib|/lib64|/etc|/var|/var/lib|/home|/root|/tmp|/srv|/Users|/Applications|/Library|/System|/opt/homebrew|"$HOME")
            log_error "$PREFIX_ABS is a system directory, not a TSI prefix. Refusing to remove."
            exit 1 ;;
    esac
    if [ ! -f "$PREFIX_ABS/bin/tsi" ] && [ ! -f "$PREFIX_ABS/bin/tsi.exe" ]; then
        log_error "No TSI binary under $PREFIX_ABS. Refusing to remove (not a TSI install?)."
        exit 1
    fi
    if [ "$NON_INTERACTIVE" != true ]; then
        if [ -t 1 ] && [ -c /dev/tty ] 2>/dev/null; then
            { printf "Remove TSI and all data at %s? (yes to continue): " "$PREFIX_ABS" > /dev/tty
              read -r r < /dev/tty; printf "\n" > /dev/tty; }
            [ "$r" != "yes" ] && { log_info "Cancelled."; exit 0; }
        else
            log_error "Uninstall requires confirmation. Use --non-interactive and UNINSTALL=1 to skip prompt."
            exit 1
        fi
    fi
    log_info "Removing TSI from $PREFIX_ABS..."
    # Only what TSI created; the prefix itself goes only if that leaves it empty.
    for d in packages sources build db install tmp-repo-update tmp-self-update; do
        rm -rf "${PREFIX_ABS:?}/$d"
    done
    rm -f "$PREFIX_ABS/bin/tsi" "$PREFIX_ABS/bin/tsi.exe" \
        "$PREFIX_ABS/share/completions/tsi.bash" "$PREFIX_ABS/share/completions/tsi.zsh" \
        "$PREFIX_ABS/tsi.toml" "$PREFIX_ABS/.tsi-install.lock" "$PREFIX_ABS/.tsi-prefix"
    for d in share/completions share bin; do
        rmdir "$PREFIX_ABS/$d" 2>/dev/null || true
    done
    rmdir "$PREFIX_ABS" 2>/dev/null || log_info "Kept $PREFIX_ABS: it holds files TSI did not create."
    remove_path_setup
    log_info "TSI uninstalled."
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
        i686) ARCH="i686" ;;
        # armv8l is a 32-bit userland on a 64-bit ARM kernel: it runs armv7 code.
        armv7l|armv8l) ARCH="armv7" ;;
        armv6l) ARCH="armv6" ;;
        riscv64) ARCH="riscv64" ;;
        ppc64le) ARCH="ppc64le" ;;
        *) ARCH="" ;;
    esac
    echo "${OS}-${ARCH}"
}

main() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --repair|repair) REPAIR_MODE=true; shift ;;
            --uninstall) UNINSTALL_MODE=true; shift ;;
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
                echo "Running it again updates an existing installation."
                echo "It adds TSI to PATH in your shell profile (TSI_NO_MODIFY_PATH=1 skips that)."
                echo ""
                echo "Options:"
                echo "  --repair          Repair/update existing TSI installation"
                echo "  --uninstall       Remove TSI completely from the system"
                echo "  --prefix PATH     Installation prefix (default: ~/.tsi)"
                echo "  --non-interactive Run without prompts"
                echo "  --help, -h        Show this help"
                echo ""
                echo "Examples:"
                echo "  curl -fsSL .../tsi-bootstrap.sh | sh"
                echo "  curl -fsSL .../tsi-bootstrap.sh | sh -s -- --prefix /opt/tsi"
                echo "  curl -fsSL .../tsi-bootstrap.sh | TSI_NO_MODIFY_PATH=1 sh"
                echo "  curl -fsSL .../tsi-bootstrap.sh | sh -s -- --uninstall"
                exit 0
                ;;
            *) log_error "Unknown option: $1"; exit 1 ;;
        esac
    done

    if [ "$UNINSTALL_MODE" = true ]; then
        run_uninstall
        return
    fi

    # Running the installer again is how you update: an existing install is
    # upgraded in place (same as --repair), keeping packages and data.
    if [ "$REPAIR_MODE" != true ] && check_tsi_installed; then
        REPAIR_MODE=true
    fi
    if [ "$REPAIR_MODE" = true ]; then
        log_info "Updating TSI in $PREFIX"
    else
        log_info "Installing TSI to $PREFIX"
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
            if "$(get_command_path git)" clone --depth 1 --shallow-submodules --recurse-submodules \
                --branch "$TSI_BRANCH" "$TSI_REPO" tsi 2>&1; then
                log_info "Repository cloned successfully"
            else
                log_error "Git clone failed"
                exit 1
            fi
        else
            log_info "Downloading tarball..."
            tarball_url="https://github.com/PanterSoft/TheSourceInstaller/archive/refs/heads/${TSI_BRANCH}.tar.gz"
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
    RELEASE_URL="https://github.com/PanterSoft/TheSourceInstaller/releases/latest/download/tsi-${PLATFORM}"

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
    # Marks the directory as a TSI prefix, so tsi knows where its data lives.
    [ -f "$PREFIX/.tsi-prefix" ] || echo "This directory is a TSI (The Source Installer) prefix." > "$PREFIX/.tsi-prefix"
    cp "$TSI_BINARY" "$PREFIX/bin/tsi"
    chmod +x "$PREFIX/bin/tsi"

    log_info "Installing shell completions..."
    mkdir -p "$PREFIX/share/completions"
    [ -f "completions/tsi.bash" ] && cp completions/tsi.bash "$PREFIX/share/completions/" && chmod 644 "$PREFIX/share/completions/tsi.bash"
    [ -f "completions/tsi.zsh" ] && cp completions/tsi.zsh "$PREFIX/share/completions/" && chmod 644 "$PREFIX/share/completions/tsi.zsh"

    log_info "Setting up package repository..."
    mkdir -p "$PREFIX/packages"
    # Definitions live in the tsi-packages submodule; older checkouts kept them
    # at the top level. A tarball download has no submodule, so finding neither
    # is fine -- `tsi update` fetches them.
    PKG_SRC=""
    for d in "tsi-packages/packages" "packages"; do
        [ -d "$d" ] && PKG_SRC="$d" && break
    done
    if [ -n "$PKG_SRC" ]; then
        count=0
        for f in "$PKG_SRC"/*.json; do
            [ -f "$f" ] && cp "$f" "$PREFIX/packages/" && count=$((count + 1))
        done
        log_info "  Copied $count package definitions"
    else
        # No bundled definitions (tarball download): fetch them now, so
        # `tsi install` works right away.
        if "$PREFIX/bin/tsi" update --prefix "$PREFIX" >/dev/null 2>&1; then
            log_info "  Fetched the latest package definitions"
        else
            log_warn "  Could not fetch package definitions; run 'tsi update' later"
        fi
    fi

    PATH_PROFILE=""
    log_info ""
    if setup_path "$PREFIX/bin"; then
        log_info "TSI installed successfully!"
        if [ -n "$PATH_PROFILE" ]; then
            log_info "Added $PREFIX/bin to PATH in $PATH_PROFILE."
            log_info "Open a new terminal, then try:  tsi install curl"
        else
            log_info "Try:  tsi install curl"
        fi
    else
        log_info "TSI installed successfully!"
        log_info "Add it to your PATH:  export PATH=\"$PREFIX/bin:\$PATH\""
        log_info "Then try:  tsi install curl"
    fi
}

main "$@"
