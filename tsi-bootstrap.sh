#!/bin/sh
# TSI One-Line Bootstrap Installer
# Downloads TSI source and installs it
# Works on systems with only a C compiler (no make required)

set -e

PREFIX="${PREFIX:-$HOME/.tsi}"
TSI_REPO="${TSI_REPO:-https://github.com/PanterSoft/tsi.git}"
TSI_BRANCH="${TSI_BRANCH:-main}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/tsi-install}"
REPAIR_MODE=false

log_info() {
    echo "[INFO] $*"
}

log_warn() {
    echo "[WARN] $*"
}

log_error() {
    echo "[ERROR] $*" >&2
}

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

download_tarball() {
    local url="$1"
    local output="$2"

    if command_exists curl; then
        curl -fsSL "$url" -o "$output"
    elif command_exists wget; then
        wget -q "$url" -O "$output"
    else
        return 1
    fi
}

check_tsi_installed() {
    local tsi_bin="$PREFIX/bin/tsi"
    if [ -f "$tsi_bin" ] && [ -x "$tsi_bin" ]; then
        # Just check if file exists and is executable, don't try to run it
        # (running --version might hang)
        return 0
    fi
    return 1
}

check_tsi_outdated() {
    local tsi_bin="$PREFIX/bin/tsi"
    if [ -f "$tsi_bin" ]; then
        # Check if binary is older than 7 days (simple heuristic)
        if [ -n "$(find "$tsi_bin" -mtime +7 2>/dev/null)" ]; then
            return 0
        fi
        # Or check if source is newer
        if [ -d "$INSTALL_DIR/tsi" ] && [ "$INSTALL_DIR/tsi/src/main.c" -nt "$tsi_bin" ] 2>/dev/null; then
            return 0
        fi
    fi
    return 1
}

main() {
    # Parse command line arguments
    while [ $# -gt 0 ]; do
        case "$1" in
            --repair)
                REPAIR_MODE=true
                shift
                ;;
            --prefix)
                PREFIX="$2"
                shift 2
                ;;
            --help|-h)
                echo "TSI Bootstrap Installer"
                echo ""
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  --repair          Repair/update existing TSI installation"
                echo "  --prefix PATH     Installation prefix (default: \$HOME/.tsi)"
                echo "  --help, -h        Show this help"
                echo ""
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                echo "Use --help for usage information"
                exit 1
                ;;
        esac
    done
    if [ "$REPAIR_MODE" = true ]; then
        log_info "TSI Repair/Update Mode"
        log_info "======================"
        log_info ""

        if check_tsi_installed; then
            log_info "TSI is installed at: $PREFIX/bin/tsi"
            if check_tsi_outdated; then
                log_info "TSI appears to be outdated, will rebuild..."
            else
                log_info "TSI installation found, will rebuild to repair..."
            fi
        else
            log_info "TSI binary not found or broken, will install..."
        fi
        log_info ""
    else
        log_info "TSI One-Line Bootstrap Installer"
        log_info "=================================="
        log_info ""

        # Check if already installed
        if check_tsi_installed; then
            log_warn "TSI is already installed at: $PREFIX/bin/tsi"
            log_warn "Use --repair to update or repair the installation"
            log_warn "Continuing with fresh installation..."
            log_info ""
        fi
    fi

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
        log_error "TSI requires a C compiler to build"
        exit 1
    fi

    log_info "Found C compiler: $CC ($($CC --version 2>&1 | head -1))"

    # Create install directory
    mkdir -p "$INSTALL_DIR"
    cd "$INSTALL_DIR"

    # Check if we should update source in repair mode
    UPDATE_SOURCE=false
    if [ "$REPAIR_MODE" = true ] && [ -d "tsi" ] && [ -f "tsi/src/main.c" ]; then
        if command_exists git && [ -d "tsi/.git" ]; then
            log_info "Checking for source updates..."
            cd tsi
            # Set up remote if not already set
            if ! git remote get-url origin >/dev/null 2>&1; then
                git remote add origin "$TSI_REPO" 2>/dev/null || true
            fi

            # Fetch latest changes
            if git fetch origin "$TSI_BRANCH" >/dev/null 2>&1; then
                LOCAL_COMMIT=$(git rev-parse HEAD 2>/dev/null)
                REMOTE_COMMIT=$(git rev-parse "origin/$TSI_BRANCH" 2>/dev/null)
                if [ "$LOCAL_COMMIT" != "$REMOTE_COMMIT" ] && [ -n "$REMOTE_COMMIT" ]; then
                    log_info "Source has changed (local: ${LOCAL_COMMIT:0:8}, remote: ${REMOTE_COMMIT:0:8})"
                    log_info "Updating source..."
                    if git pull origin "$TSI_BRANCH" >/dev/null 2>&1; then
                        log_info "✓ Source updated successfully"
                        cd ..
                    else
                        log_warn "Git pull failed, will re-clone..."
                        cd ..
                        UPDATE_SOURCE=true
                    fi
                else
                    log_info "Source is up to date"
                    cd ..
                fi
            else
                log_warn "Cannot fetch updates (checking internet connection), will re-download..."
                cd ..
                UPDATE_SOURCE=true
            fi
        else
            # Not a git repo or git not available, re-download to be safe
            log_info "Source is not a git repository, will re-download..."
            UPDATE_SOURCE=true
        fi
    fi

    # Remove existing source if we need to re-download
    if [ "$UPDATE_SOURCE" = true ]; then
        log_info "Removing old source..."
        rm -rf tsi
    fi

    # Check if TSI is already downloaded
    if [ -d "tsi" ] && [ -f "tsi/src/main.c" ]; then
        if [ "$REPAIR_MODE" = false ]; then
            log_info "TSI source already exists, using existing copy"
        fi
        cd tsi
    else
        log_info "Downloading TSI source code..."

        # Try git clone first (if git is available)
        GIT_CLONE_SUCCESS=false
        if command_exists git; then
            log_info "Cloning TSI repository..."
            if [ -d "tsi" ]; then
                rm -rf tsi
            fi
            if git clone --depth 1 --branch "$TSI_BRANCH" "$TSI_REPO" tsi 2>&1; then
                log_info "Repository cloned successfully"
                GIT_CLONE_SUCCESS=true
            else
                log_warn "Git clone failed, trying tarball download..."
                rm -rf tsi
            fi
        fi

        # Fallback: Download as tarball (only if git clone didn't succeed)
        if [ "$GIT_CLONE_SUCCESS" = false ]; then
            if [ ! -d "tsi" ] || [ ! -f "tsi/src/main.c" ]; then
                log_info "Downloading TSI as tarball..."
                tarball_url="https://github.com/PanterSoft/tsi/archive/refs/heads/${TSI_BRANCH}.tar.gz"
                tarball="tsi-${TSI_BRANCH}.tar.gz"

                if download_tarball "$tarball_url" "$tarball"; then
                    log_info "Extracting tarball..."
                    if command_exists tar; then
                        tar -xzf "$tarball" 2>/dev/null || tar -xf "$tarball" 2>/dev/null
                        # Find and rename extracted directory (GitHub tarballs can have various names)
                        # Common patterns: tsi-main, tsi-${TSI_BRANCH}, TheSourceInstaller-main, etc.
                        EXTRACTED_DIR=""
                        for possible_name in "tsi-${TSI_BRANCH}" "tsi-main" "TheSourceInstaller-${TSI_BRANCH}" "TheSourceInstaller-main" "tsi"; do
                            if [ -d "$possible_name" ] && [ -f "$possible_name/src/main.c" ]; then
                                EXTRACTED_DIR="$possible_name"
                                break
                            fi
                        done
                        # If no exact match, find any directory that contains src/main.c
                        if [ -z "$EXTRACTED_DIR" ]; then
                            for dir in */; do
                                if [ -d "$dir/src" ] && [ -f "$dir/src/main.c" ]; then
                                    EXTRACTED_DIR="${dir%/}"
                                    break
                                fi
                            done
                        fi
                        if [ -n "$EXTRACTED_DIR" ] && [ "$EXTRACTED_DIR" != "tsi" ]; then
                            log_info "Renaming extracted directory: $EXTRACTED_DIR -> tsi"
                            mv "$EXTRACTED_DIR" tsi
                        elif [ -z "$EXTRACTED_DIR" ]; then
                            log_error "Could not find TSI source directory after extraction"
                            log_error "Extracted contents:"
                            ls -la
                            exit 1
                        fi
                        rm -f "$tarball"
                        log_info "Tarball extracted successfully"
                    else
                        log_error "tar not found, cannot extract tarball"
                        exit 1
                    fi
                else
                    log_error "Failed to download TSI source"
                    log_error "Please check your internet connection and try again"
                    exit 1
                fi
            fi
        fi

        # Change into tsi directory (after either git clone or tarball extraction)
        if [ -d "tsi" ] && [ -f "tsi/src/main.c" ]; then
            cd tsi
        else
            log_error "TSI source directory not found after download"
            exit 1
        fi
    fi

    # Verify source directory exists
    if [ ! -d "src" ] || [ ! -f "src/main.c" ]; then
        log_error "TSI source directory not found: src/"
        exit 1
    fi

    # Build TSI
    log_info ""
    log_info "Building TSI..."
    cd src

    # Use custom CC if specified
    if [ -n "$CC" ]; then
        export CC
    fi

    # Clean previous build
    log_info "Cleaning previous build..."
    rm -rf build bin
    mkdir -p build bin

    # Compile TSI directly without make
    log_info "Compiling TSI source files..."

    # CFLAGS
    CFLAGS="-Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L"

    # Detect OS for static linking (only works on Linux)
    UNAME_S=$(uname -s 2>/dev/null || echo "Unknown")
    if [ "$UNAME_S" = "Linux" ]; then
        LDFLAGS="-static"
    else
        LDFLAGS=""
    fi

    # Compile all C source files
    OBJECTS=""
    for c_file in *.c; do
        if [ -f "$c_file" ]; then
            obj_file="build/${c_file%.c}.o"
            log_info "  Compiling $c_file..."
            if ! $CC $CFLAGS -c "$c_file" -o "$obj_file"; then
                log_error "Failed to compile $c_file"
                exit 1
            fi
            OBJECTS="$OBJECTS $obj_file"
        fi
    done

    # Link
    log_info "Linking TSI binary..."
    if ! $CC $OBJECTS -o "bin/tsi" $LDFLAGS; then
        log_error "Failed to link TSI binary"
        exit 1
    fi

    log_info "TSI built successfully"

    # Verify binary was created
    if [ ! -f "bin/tsi" ]; then
        log_error "Binary not found after build: bin/tsi"
        exit 1
    fi

    # Make binary executable
    chmod +x bin/tsi

    # Install
    log_info ""
    log_info "Installing TSI to $PREFIX..."
    mkdir -p "$PREFIX/bin"
    cp bin/tsi "$PREFIX/bin/tsi"
    chmod +x "$PREFIX/bin/tsi"

    # Install completion scripts
    log_info "Installing shell completion scripts..."
    mkdir -p "$PREFIX/share/completions"

    # Find completions directory (could be in tsi/ or parent)
    COMPLETIONS_DIR=""
    if [ -d "../completions" ]; then
        COMPLETIONS_DIR="../completions"
    elif [ -d "completions" ]; then
        COMPLETIONS_DIR="completions"
    elif [ -d "../../completions" ]; then
        COMPLETIONS_DIR="../../completions"
    fi

    if [ -n "$COMPLETIONS_DIR" ]; then
        if [ -f "$COMPLETIONS_DIR/tsi.bash" ]; then
            cp "$COMPLETIONS_DIR/tsi.bash" "$PREFIX/share/completions/tsi.bash"
            chmod 644 "$PREFIX/share/completions/tsi.bash"
            log_info "  Installed bash completion"
        fi
        if [ -f "$COMPLETIONS_DIR/tsi.zsh" ]; then
            cp "$COMPLETIONS_DIR/tsi.zsh" "$PREFIX/share/completions/tsi.zsh"
            chmod 644 "$PREFIX/share/completions/tsi.zsh"
            log_info "  Installed zsh completion"
        fi
    else
        log_warn "  Completion scripts not found (optional)"
    fi

    # Initialize package repository (only for fresh installs, not repair mode)
    # Skip automatic update to avoid hanging - user can run 'tsi update' manually
    if [ "$REPAIR_MODE" != true ]; then
        log_info ""
        log_info "Creating package repository directory..."
        mkdir -p "$PREFIX/repos"
        log_info "✓ Package repository directory created"
        log_info ""
        log_info "Note: Run 'tsi update' after installation to download package definitions"
    fi

    log_info ""
    log_info "========================================="
    log_info "TSI installed successfully!"
    log_info "========================================="
    log_info ""
    log_info "Add to your PATH:"
    log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
    log_info ""
    log_info "Or add to your shell profile:"
    if [ -n "$ZSH_VERSION" ]; then
        log_info "  echo 'export PATH=\"$PREFIX/bin:\\\$PATH\"' >> ~/.zshrc"
        log_info ""
        log_info "Enable autocomplete (zsh):"
        log_info "  echo 'source $PREFIX/share/completions/tsi.zsh' >> ~/.zshrc"
    else
        log_info "  echo 'export PATH=\"$PREFIX/bin:\\\$PATH\"' >> ~/.bashrc"
        log_info ""
        log_info "Enable autocomplete (bash):"
        log_info "  echo 'source $PREFIX/share/completions/tsi.bash' >> ~/.bashrc"
    fi
    log_info ""
    if [ "$REPAIR_MODE" != true ]; then
        log_info "Package repository is ready! Try: tsi list"
    else
        log_info "Then run: tsi --help"
    fi
    log_info ""
}

main "$@"

