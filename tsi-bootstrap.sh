#!/bin/sh
# TSI One-Line Bootstrap Installer
# Downloads TSI source and installs it
# Works on systems with only a C compiler (no make required)
# POSIX-compliant shell script

set -e
# Note: We don't use 'set -u' because some shells handle $@ differently
# and we use parameter expansion with defaults like ${VAR:-default}

PREFIX="${PREFIX:-$HOME/.tsi}"
TSI_REPO="${TSI_REPO:-https://github.com/PanterSoft/tsi.git}"
TSI_BRANCH="${TSI_BRANCH:-main}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/tsi-install}"
REPAIR_MODE=false

# Isolate TSI: prioritize TSI's bin directory in PATH
# This ensures TSI uses its own installed tools when available
if [ -d "${PREFIX}/bin" ]; then
    export PATH="${PREFIX}/bin:${PATH}"
fi

log_info() {
    echo "[INFO] $*"
}

log_warn() {
    echo "[WARN] $*"
}

log_error() {
    echo "[ERROR] $*" >&2
}

# Check if command exists, prioritizing TSI's bin directory
command_exists() {
    # First check TSI's bin directory if it exists
    if [ -d "${PREFIX}/bin" ] && [ -x "${PREFIX}/bin/$1" ]; then
        return 0
    fi
    # Fall back to system PATH
    command -v "$1" >/dev/null 2>&1
}

# Get full path to command, preferring TSI's bin
get_command_path() {
    cmd="$1"
    # First check TSI's bin directory if it exists
    if [ -d "${PREFIX}/bin" ] && [ -x "${PREFIX}/bin/$cmd" ]; then
        echo "${PREFIX}/bin/$cmd"
        return 0
    fi
    # Fall back to system PATH
    command -v "$cmd" 2>/dev/null || echo "$cmd"
}

download_tarball() {
    url="$1"
    output="$2"

    # Prefer TSI-installed tools
    if command_exists curl; then
        curl_cmd=$(get_command_path curl)
        "$curl_cmd" -fsSL "$url" -o "$output" || return 1
    elif command_exists wget; then
        wget_cmd=$(get_command_path wget)
        "$wget_cmd" -q "$url" -O "$output" || return 1
    else
        return 1
    fi
}

check_tsi_installed() {
    tsi_bin="${PREFIX}/bin/tsi"
    if [ -f "$tsi_bin" ] && [ -x "$tsi_bin" ]; then
        # Just check if file exists and is executable, don't try to run it
        # (running --version might hang)
        return 0
    fi
    return 1
}

check_tsi_outdated() {
    tsi_bin="${PREFIX}/bin/tsi"
    if [ -f "$tsi_bin" ]; then
        # Check if binary is older than 7 days (simple heuristic)
        # Use find if available, otherwise skip this check
        if command_exists find; then
            if [ -n "$(find "$tsi_bin" -mtime +7 2>/dev/null)" ]; then
                return 0
            fi
        fi
        # Or check if source is newer (POSIX-compliant test)
        if [ -d "$INSTALL_DIR/tsi" ] && [ -f "$INSTALL_DIR/tsi/src/main.c" ]; then
            if [ "$INSTALL_DIR/tsi/src/main.c" -nt "$tsi_bin" ] 2>/dev/null; then
                return 0
            fi
        fi
    fi
    return 1
}

# Get the current version/commit of existing source files
# Returns the commit hash if it's a git repo, empty string otherwise
get_source_version() {
    source_dir="$1"
    if [ ! -d "$source_dir" ] || [ ! -f "$source_dir/src/main.c" ]; then
        return 1
    fi

    # Check if it's a git repository
    if command_exists git && [ -d "$source_dir/.git" ]; then
        git_cmd=$(get_command_path git)
        cd "$source_dir"
        # Get current commit hash
        COMMIT=$("$git_cmd" rev-parse HEAD 2>/dev/null)
        cd - >/dev/null 2>&1
        if [ -n "$COMMIT" ]; then
            echo "$COMMIT"
            return 0
        fi
    fi

    # Not a git repo or git not available
    return 1
}

# Check if existing source matches the target version
# Returns 0 if source should be used, 1 if it should be re-downloaded
check_source_version() {
    source_dir="$1"
    target_branch="$2"

    if [ ! -d "$source_dir" ] || [ ! -f "$source_dir/src/main.c" ]; then
        return 1  # No source, need to download
    fi

    # If it's a git repo, check if it matches the target branch
    if command_exists git && [ -d "$source_dir/.git" ]; then
        git_cmd=$(get_command_path git)
        cd "$source_dir"

        # Check current branch
        CURRENT_BRANCH=$("$git_cmd" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "")

        # If we're on the target branch, check if we can fetch and compare
        if [ "$CURRENT_BRANCH" = "$target_branch" ]; then
            # Try to fetch latest (non-blocking, don't fail if network is down)
            "$git_cmd" fetch origin "$target_branch" >/dev/null 2>&1 || true

            # Compare local and remote commits
            LOCAL_COMMIT=$("$git_cmd" rev-parse HEAD 2>/dev/null)
            REMOTE_COMMIT=$("$git_cmd" rev-parse "origin/$target_branch" 2>/dev/null || echo "")

            cd - >/dev/null 2>&1

            if [ -n "$LOCAL_COMMIT" ] && [ -n "$REMOTE_COMMIT" ]; then
                if [ "$LOCAL_COMMIT" = "$REMOTE_COMMIT" ]; then
                    # Source matches target version
                    return 0
                else
                    # Source is different from target, need to update
                    log_info "Source version mismatch (local: ${LOCAL_COMMIT:0:8}, target: ${REMOTE_COMMIT:0:8})"
                    return 1
                fi
            elif [ -n "$LOCAL_COMMIT" ]; then
                # Can't fetch remote, but we have local source - use it
                log_info "Using existing source (commit: ${LOCAL_COMMIT:0:8}, cannot verify remote)"
                return 0
            fi
        else
            # On different branch, need to switch or re-download
            log_info "Source is on branch '$CURRENT_BRANCH', target is '$target_branch'"
            cd - >/dev/null 2>&1
            return 1
        fi
    else
        # Not a git repo - we can't verify version, so be conservative
        # If it's not a git repo and we want a specific branch, re-download to be safe
        log_info "Source is not a git repository, cannot verify version"
        return 1
    fi
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
            git_cmd=$(get_command_path git)
            cd tsi
            # Set up remote if not already set
            if ! "$git_cmd" remote get-url origin >/dev/null 2>&1; then
                "$git_cmd" remote add origin "$TSI_REPO" 2>/dev/null || true
            fi

            # Fetch latest changes
            if "$git_cmd" fetch origin "$TSI_BRANCH" >/dev/null 2>&1; then
                LOCAL_COMMIT=$("$git_cmd" rev-parse HEAD 2>/dev/null)
                REMOTE_COMMIT=$("$git_cmd" rev-parse "origin/$TSI_BRANCH" 2>/dev/null)
                if [ "$LOCAL_COMMIT" != "$REMOTE_COMMIT" ] && [ -n "$REMOTE_COMMIT" ]; then
                    log_info "Source has changed (local: ${LOCAL_COMMIT:0:8}, remote: ${REMOTE_COMMIT:0:8})"
                    log_info "Updating source..."
                    if "$git_cmd" pull origin "$TSI_BRANCH" >/dev/null 2>&1; then
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

    # Check if TSI is already downloaded and verify version
    if [ -d "tsi" ] && [ -f "tsi/src/main.c" ]; then
        if [ "$REPAIR_MODE" = false ]; then
            log_info "TSI source already exists, checking version..."
            if check_source_version "tsi" "$TSI_BRANCH"; then
                CURRENT_VERSION=$(get_source_version "tsi")
                if [ -n "$CURRENT_VERSION" ]; then
                    log_info "Using existing source (version: ${CURRENT_VERSION:0:8})"
                else
                    log_info "Using existing source"
                fi
                cd tsi
            else
                log_info "Existing source version doesn't match target, will re-download..."
                rm -rf tsi
                # Fall through to download section
            fi
        else
            # In repair mode, we already handled version checking above
            cd tsi
        fi
    fi

    # Download source if needed
    if [ ! -d "tsi" ] || [ ! -f "tsi/src/main.c" ]; then
        log_info "Downloading TSI source code..."

        # Try git clone first (if git is available)
        GIT_CLONE_SUCCESS=false
        if command_exists git; then
            log_info "Cloning TSI repository..."
            git_cmd=$(get_command_path git)
            if [ -d "tsi" ]; then
                rm -rf tsi
            fi
            if "$git_cmd" clone --depth 1 --branch "$TSI_BRANCH" "$TSI_REPO" tsi 2>&1; then
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
                        tar_cmd=$(get_command_path tar)
                        "$tar_cmd" -xzf "$tarball" 2>/dev/null || "$tar_cmd" -xf "$tarball" 2>/dev/null
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
                            for dir in *; do
                                # Skip if not a directory
                                [ ! -d "$dir" ] && continue
                                if [ -d "$dir/src" ] && [ -f "$dir/src/main.c" ]; then
                                    EXTRACTED_DIR="$dir"
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
            # Log the version we downloaded
            DOWNLOADED_VERSION=$(get_source_version ".")
            if [ -n "$DOWNLOADED_VERSION" ]; then
                log_info "Downloaded source (version: ${DOWNLOADED_VERSION:0:8})"
            fi
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

    # Detect C compiler and include paths
    CC="${CC:-gcc}"
    if ! command_exists "$CC"; then
        # Try alternatives
        for alt_cc in clang cc; do
            if command_exists "$alt_cc"; then
                CC="$alt_cc"
                log_info "Using C compiler: $CC"
                break
            fi
        done
        if ! command_exists "$CC"; then
            log_error "No C compiler found (tried: gcc, clang, cc)"
            exit 1
        fi
    fi

    # Try to find standard include directories
    # Test if stdio.h can be found
    INCLUDE_FLAGS=""
    if ! echo '#include <stdio.h>' | $CC -E -x c - 2>/dev/null >/dev/null; then
        log_warn "stdio.h not found in default include path, searching..."
        # Try common include locations
        for inc_dir in /usr/include /usr/local/include /opt/include; do
            if [ -d "$inc_dir" ] && [ -f "$inc_dir/stdio.h" ]; then
                INCLUDE_FLAGS="-I$inc_dir"
                log_info "Found stdio.h in $inc_dir"
                break
            fi
        done
        # If still not found, try to get include paths from compiler
        if [ -z "$INCLUDE_FLAGS" ]; then
            log_info "Querying compiler for include paths..."
            # Use POSIX-compliant tools (avoid awk if possible, use sed instead)
            if command_exists grep && command_exists head && command_exists tr; then
                COMPILER_INCLUDES=$($CC -E -v -x c - 2>&1 | grep '^ /' | head -5 | tr '\n' ' ' || true)
                if [ -n "$COMPILER_INCLUDES" ]; then
                    # Extract first include directory using sed (POSIX-compliant)
                    FIRST_INC=$(echo "$COMPILER_INCLUDES" | sed 's/^[[:space:]]*\([^[:space:]]*\).*/\1/')
                    if [ -n "$FIRST_INC" ] && [ -d "$FIRST_INC" ]; then
                        INCLUDE_FLAGS="-I$FIRST_INC"
                        log_info "Using compiler include path: $FIRST_INC"
                    fi
                fi
            fi
        fi
        if [ -z "$INCLUDE_FLAGS" ]; then
            log_error "Cannot find stdio.h. Please install C development headers."
            log_error "On Debian/Ubuntu: apt-get install build-essential"
            log_error "On RedHat/CentOS: yum install gcc glibc-devel"
            log_error "On Alpine: apk add gcc musl-dev"
            exit 1
        fi
    fi

    # CFLAGS (suppress warnings, only show errors)
    CFLAGS="-w -O2 -std=c11 -D_POSIX_C_SOURCE=200809L $INCLUDE_FLAGS"

    # Compile all C source files (exclude TUI components)
    OBJECTS=""
    COMPILED_COUNT=0
    # Count files in a POSIX-compliant way, excluding tui_interactive.c and tui_style.c
    TOTAL_FILES=0
    for c_file in *.c; do
        if [ -f "$c_file" ] && [ "$c_file" != "tui_interactive.c" ] && [ "$c_file" != "tui_style.c" ]; then
            TOTAL_FILES=$((TOTAL_FILES + 1))
        fi
    done

    if [ "$TOTAL_FILES" -eq 0 ]; then
        log_error "No C source files found in current directory"
        exit 1
    fi

    for c_file in *.c; do
        if [ -f "$c_file" ] && [ "$c_file" != "tui_interactive.c" ] && [ "$c_file" != "tui_style.c" ]; then
            COMPILED_COUNT=$((COMPILED_COUNT + 1))
            log_info "  [$COMPILED_COUNT/$TOTAL_FILES] Compiling $c_file..."
            # Use POSIX-compliant parameter expansion
            obj_file="build/$(echo "$c_file" | sed 's/\.c$/.o/')"
            # Suppress warnings, only show errors (warnings already suppressed by -w in CFLAGS)
            if ! $CC $CFLAGS -c "$c_file" -o "$obj_file" 2>&1; then
                log_error "Failed to compile $c_file"
                exit 1
            fi
            OBJECTS="$OBJECTS $obj_file"
        fi
    done
    log_info "Compiled $COMPILED_COUNT source files"

    # Link (try static first, fall back to dynamic if static fails)
    log_info "Linking TSI binary..."
    UNAME_S=$(uname -s 2>/dev/null || echo "Unknown")
    LINK_SUCCESS=false
    if [ "$UNAME_S" = "Linux" ]; then
        # Try static linking first (suppress warnings, only show errors)
        if $CC $OBJECTS -o "bin/tsi" -static -w 2>&1; then
            LINK_SUCCESS=true
        else
            # Static linking failed, try dynamic linking
            log_info "Static linking failed, trying dynamic linking..."
            rm -f "bin/tsi"
        fi
    fi

    # If static linking didn't work or we're not on Linux, try dynamic linking
    if [ "$LINK_SUCCESS" = false ]; then
        if ! $CC $OBJECTS -o "bin/tsi" -w 2>&1; then
            log_error "Failed to link TSI binary"
            exit 1
        fi
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
    log_info "TSI is isolated: it uses its own bin directory ($PREFIX/bin)"
    log_info "and prefers TSI-installed tools over system tools."
    log_info ""

    # Automatically add TSI to PATH for current terminal session
    # Check if we're in an interactive shell (not piped)
    if [ -t 0 ] && [ -t 1 ]; then
        # Interactive terminal - export PATH for current session
        export PATH="$PREFIX/bin:$PATH"
        log_info "✓ Added TSI to PATH for current terminal session"
        log_info ""
        log_info "You can now use 'tsi' command immediately!"
        log_info ""
    else
        # Piped execution (curl ... | sh) - can't modify parent shell
        # Output export command for user to run
        log_info "To use TSI in this terminal, run:"
        log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
        log_info ""
        log_info "Or use: eval \"\$(curl -fsSL ... | sh)\" to auto-configure"
        log_info ""
    fi

    log_info "To add permanently to your shell profile:"
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

