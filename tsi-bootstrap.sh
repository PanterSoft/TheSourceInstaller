#!/bin/sh
# TSI One-Line Bootstrap Installer
# Downloads TSI source and installs it
# Works on systems with only a C compiler (no make required)
# POSIX-compliant shell script

set -e
# Note: We don't use 'set -u' because some shells handle $@ differently
# and we use parameter expansion with defaults like ${VAR:-default}

PREFIX="${PREFIX:-/opt/tsi}"
TSI_REPO="${TSI_REPO:-https://github.com/PanterSoft/tsi.git}"
TSI_BRANCH="${TSI_BRANCH:-main}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/tsi-install}"
# Support REPAIR environment variable as alternative to --repair flag
REPAIR_MODE="${REPAIR:-false}"
# Convert string "true"/"1" to boolean
if [ "$REPAIR_MODE" = "true" ] || [ "$REPAIR_MODE" = "1" ] || [ "$REPAIR_MODE" = "yes" ]; then
    REPAIR_MODE=true
else
REPAIR_MODE=false
fi

# Non-interactive mode (default: false, meaning interactive by default)
# Can be set via --non-interactive flag or NON_INTERACTIVE environment variable
NON_INTERACTIVE="${NON_INTERACTIVE:-false}"
if [ "$NON_INTERACTIVE" = "true" ] || [ "$NON_INTERACTIVE" = "1" ] || [ "$NON_INTERACTIVE" = "yes" ]; then
    NON_INTERACTIVE=true
else
    NON_INTERACTIVE=false
fi

# Isolate TSI: prioritize TSI's bin directory in PATH
# This ensures TSI uses its own installed tools when available
# Only add TSI to PATH, don't modify existing PATH
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
# Never fails - always returns 0 to avoid breaking script with set -e
get_source_version() {
    source_dir="$1"
    if [ ! -d "$source_dir" ] || [ ! -f "$source_dir/src/main.c" ]; then
        return 0  # Return 0 to avoid breaking script with set -e
    fi

    # Check if it's a git repository
    if command_exists git && [ -d "$source_dir/.git" ]; then
        git_cmd=$(get_command_path git)
        cd "$source_dir" 2>/dev/null || return 0
        # Get current commit hash
        COMMIT=$("$git_cmd" rev-parse HEAD 2>/dev/null || echo "")
        cd - >/dev/null 2>&1 || true
        if [ -n "$COMMIT" ]; then
            echo "$COMMIT"
        fi
    fi

    # Not a git repo or git not available - return 0 (success) to avoid breaking script
    return 0
}

# Check if existing source matches the target version
# Returns 0 if source should be used, 1 if it should be re-downloaded
# Always returns 1 (re-download) if git is not available, since we can't verify version
check_source_version() {
    source_dir="$1"
    target_branch="$2"

    if [ ! -d "$source_dir" ] || [ ! -f "$source_dir/src/main.c" ]; then
        return 1  # No source, need to download
    fi

    # Without git, we cannot verify version - always re-download to be safe
    if ! command_exists git; then
        log_info "Git not available, cannot verify source version - will re-download"
        return 1
    fi

    # If it's a git repo, check if it matches the target branch
    if [ -d "$source_dir/.git" ]; then
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
        # Not a git repo - we can't verify version, so re-download to be safe
        log_info "Source is not a git repository, cannot verify version - will re-download"
        return 1
    fi
}

main() {
    # Parse command line arguments
    # Handle both direct execution and piped execution
    # When piped via stdin, arguments come after 'sh -s' and may need '--' separator
    # We also support arguments without '--' by checking if they look like our options
    while [ $# -gt 0 ]; do
        case "$1" in
            --repair|repair)
                REPAIR_MODE=true
                shift
                ;;
            --non-interactive|--yes|-y)
                NON_INTERACTIVE=true
                shift
                ;;
            --prefix)
                if [ $# -lt 2 ]; then
                    log_error "--prefix requires a path argument"
                    exit 1
                fi
                PREFIX="$2"
                shift 2
                ;;
            --help|-h|help)
                echo "TSI Bootstrap Installer"
                echo ""
                echo "Usage: $0 [options]"
                echo ""
                echo "Options:"
                echo "  --repair          Repair/update existing TSI installation"
                echo "  --prefix PATH     Installation prefix (default: /opt/tsi)"
                echo "                    Examples:"
                echo "                      --prefix /opt/tsi     (system-wide, requires root)"
                echo "                      --prefix ~/.tsi       (user-local, default)"
                echo "  --non-interactive, --yes, -y"
                echo "                    Run in non-interactive mode (skip prompts)"
                echo "  --help, -h        Show this help"
                echo ""
                echo "Examples:"
                echo "  # Install to default location (/opt/tsi, requires root)"
                echo "  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sudo sh"
                echo ""
                echo "  # Install to user location using environment variable (recommended, no '--' needed)"
                echo "  PREFIX=~/.tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh"
                echo ""
                echo "  # Repair using environment variable (recommended, no '--' needed)"
                echo "  REPAIR=1 curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh"
                echo ""
                echo "  # Or use command-line arguments (no '--' needed for 'repair')"
                echo "  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s repair"
                echo "  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s --prefix ~/.tsi"
                echo ""
                echo "  # Non-interactive mode (skip prompts)"
                echo "  NON_INTERACTIVE=1 curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh"
                echo "  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s --non-interactive"
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
        log_info "Installation prefix: $PREFIX"
        # Check if prefix requires root permissions
        case "$PREFIX" in
            /opt/*|/usr/local/*|/usr/*)
                log_info "Note: Installing to system directory (may require root permissions)"
                ;;
        esac
        log_info ""

        # Check if already installed
        if check_tsi_installed; then
            log_warn "TSI is already installed at: $PREFIX/bin/tsi"
            log_info ""

            # Ask user if they want to proceed (interactive by default)
            # Only skip prompt if explicitly set to non-interactive mode
            if [ "$NON_INTERACTIVE" = true ]; then
                # Non-interactive mode - can't ask for confirmation, so exit
                log_error "TSI is already installed. Cannot proceed in non-interactive mode."
                log_error ""
                log_error "To update your existing installation, use:"
                log_error "  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s repair"
                log_error ""
                log_error "Or use environment variable (recommended):"
                log_error "  REPAIR=1 curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh"
                exit 1
            else
                # Interactive mode - check if we can read from terminal
                # When piping (curl ... | sh), stdin is the pipe, but we can read from /dev/tty
                if [ -t 1 ] && [ -c /dev/tty ] 2>/dev/null; then
                    # We have a terminal - read from /dev/tty to bypass stdin pipe
                    # Write prompt to /dev/tty so it appears on the terminal
                    # Use a single printf to avoid multiple history entries
                    { printf "[INFO] Do you want to proceed with a fresh installation? (this will rebuild TSI)\n[INFO] Type 'yes' to continue, or press Ctrl+C to cancel: " > /dev/tty; read -r user_response < /dev/tty; printf "\n" > /dev/tty; }
                    if [ "$user_response" != "yes" ]; then
                        log_info "Installation cancelled."
                        log_info ""
                        log_info "To update your existing installation, use:"
                        log_info "  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s repair"
                        log_info "  # Or use environment variable:"
                        log_info "  REPAIR=1 curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh"
                        exit 0
                    fi
                else
                    # No terminal available - can't be interactive
                    log_error "TSI is already installed. Cannot proceed in non-interactive mode."
                    log_error ""
                    log_error "To update your existing installation, use:"
                    log_error "  curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s repair"
                    log_error ""
                    log_error "Or use environment variable (recommended):"
                    log_error "  REPAIR=1 curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh"
                    exit 1
                fi
            fi
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
            cd tsi || {
                log_error "Failed to change into tsi directory"
                exit 1
            }
            # Log the version we downloaded (non-fatal if git not available)
            DOWNLOADED_VERSION=$(get_source_version "." 2>/dev/null || echo "")
            if [ -n "$DOWNLOADED_VERSION" ]; then
                log_info "Downloaded source (version: ${DOWNLOADED_VERSION:0:8})"
            fi
        else
            log_error "TSI source directory not found after download"
            log_error "Expected: tsi/ directory with tsi/src/main.c"
            log_error "Current directory: $(pwd)"
            log_error "Contents:"
            ls -la 2>&1 || true
            exit 1
        fi
    fi

    # Verify source directory exists
    if [ ! -d "src" ] || [ ! -f "src/main.c" ]; then
        log_error "TSI source directory not found: src/"
        log_error "Current directory: $(pwd)"
        log_error "Contents:"
        ls -la 2>&1 || true
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
    STATIC_ERROR=""
    if [ "$UNAME_S" = "Linux" ]; then
        # Try static linking first (suppress error output during attempt)
        set +e  # Temporarily disable exit on error for static linking attempt
        STATIC_ERROR=$($CC $OBJECTS -o "bin/tsi" -static -w 2>&1)
        STATIC_EXIT=$?
        set -e  # Re-enable exit on error
        if [ $STATIC_EXIT -eq 0 ] && [ -f "bin/tsi" ]; then
            LINK_SUCCESS=true
        else
            # Static linking failed, try dynamic linking
            log_info "Static linking failed, trying dynamic linking..."
            rm -f "bin/tsi"
        fi
    fi

    # If static linking didn't work or we're not on Linux, try dynamic linking
    if [ "$LINK_SUCCESS" = false ]; then
        set +e  # Temporarily disable exit on error for dynamic linking attempt
        DYNAMIC_ERROR=$($CC $OBJECTS -o "bin/tsi" -w 2>&1)
        DYNAMIC_EXIT=$?
        set -e  # Re-enable exit on error
        if [ $DYNAMIC_EXIT -ne 0 ] || [ ! -f "bin/tsi" ]; then
        log_error "Failed to link TSI binary"
            if [ -n "$STATIC_ERROR" ]; then
                log_error "Static linking error:"
                echo "$STATIC_ERROR" | head -5 | sed 's/^/  /' >&2
            fi
            if [ -n "$DYNAMIC_ERROR" ]; then
                log_error "Dynamic linking error:"
                echo "$DYNAMIC_ERROR" | head -5 | sed 's/^/  /' >&2
            fi
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

    # Initialize package repository
    # Copy basic set of packages so TSI works immediately without git
    # Do this for both fresh installs and repairs (to ensure packages are available)
        log_info ""
    log_info "Setting up package repository..."
    mkdir -p "$PREFIX/packages"

        # Find packages directory (could be in tsi/ or parent)
        # We're currently in src/ directory, so packages could be:
        # - ../packages (from tsi/packages)
        # - ../../packages (if we're deeper)
        # - packages (if in root)
        PACKAGES_DIR=""
        # Try relative to current directory (src/)
        if [ -d "../packages" ]; then
            PACKAGES_DIR="../packages"
        # Try from install directory root
        elif [ -d "../../packages" ]; then
            PACKAGES_DIR="../../packages"
        # Try from tsi root (if we're in tsi/src)
        elif [ -d "packages" ]; then
            PACKAGES_DIR="packages"
        # Try absolute path from INSTALL_DIR
        elif [ -d "$INSTALL_DIR/tsi/packages" ]; then
            PACKAGES_DIR="$INSTALL_DIR/tsi/packages"
        fi

        if [ -n "$PACKAGES_DIR" ] && [ -d "$PACKAGES_DIR" ]; then
            # Copy all package JSON files to repository
            PACKAGE_COUNT=0
            for pkg_file in "$PACKAGES_DIR"/*.json; do
                if [ -f "$pkg_file" ]; then
                    pkg_name=$(basename "$pkg_file")
                    cp "$pkg_file" "$PREFIX/packages/$pkg_name" 2>/dev/null || true
                    PACKAGE_COUNT=$((PACKAGE_COUNT + 1))
                fi
            done
            if [ "$PACKAGE_COUNT" -gt 0 ]; then
                log_info "✓ Installed $PACKAGE_COUNT package definitions"
                log_info "  Package repository is ready to use!"
            else
                log_warn "  No package definitions found in $PACKAGES_DIR"
            fi
        else
            log_warn "  Packages directory not found, repository will be empty"
            log_warn "  Run 'tsi update' after installation to download package definitions"
        fi
        log_info ""

    # In repair mode, also update packages from remote repository (overwrite local changes)
    if [ "$REPAIR_MODE" = true ]; then
        log_info ""
        log_info "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        log_info "Updating Package Repository from Remote"
        log_info "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        log_info ""
        log_info "Fetching latest packages from remote repository..."

        # Ensure packages directory exists
        mkdir -p "$PREFIX/packages"

        # Use git to fetch packages from remote
        if command_exists git; then
            git_cmd=$(get_command_path git)
            TEMP_REPO_DIR="$INSTALL_DIR/tmp-repo-update"

            # Clone or update the repository
            if [ -d "$TEMP_REPO_DIR/.git" ]; then
                log_info "Updating existing repository clone..."
                cd "$TEMP_REPO_DIR"
                if "$git_cmd" fetch origin "$TSI_BRANCH" >/dev/null 2>&1 && \
                   "$git_cmd" reset --hard "origin/$TSI_BRANCH" >/dev/null 2>&1; then
                    log_info "✓ Repository updated"
                    cd "$INSTALL_DIR"
                else
                    log_warn "Failed to update repository, trying fresh clone..."
                    cd "$INSTALL_DIR"
                    rm -rf "$TEMP_REPO_DIR"
                fi
            fi

            if [ ! -d "$TEMP_REPO_DIR/.git" ]; then
                log_info "Cloning repository..."
                if "$git_cmd" clone --depth 1 --branch "$TSI_BRANCH" "$TSI_REPO" "$TEMP_REPO_DIR" >/dev/null 2>&1; then
                    log_info "✓ Repository cloned"
                else
                    log_warn "Failed to clone repository"
                    TEMP_REPO_DIR=""
                fi
            fi

            # Copy packages from repository (force overwrite local changes)
            if [ -n "$TEMP_REPO_DIR" ] && [ -d "$TEMP_REPO_DIR/packages" ]; then
                PACKAGE_COUNT=0
                for pkg_file in "$TEMP_REPO_DIR/packages"/*.json; do
                    if [ -f "$pkg_file" ]; then
                        pkg_name=$(basename "$pkg_file")
                        # Force overwrite local changes with -f flag
                        cp -f "$pkg_file" "$PREFIX/packages/$pkg_name" 2>/dev/null || true
                        PACKAGE_COUNT=$((PACKAGE_COUNT + 1))
                    fi
                done
                if [ "$PACKAGE_COUNT" -gt 0 ]; then
                    log_info "✓ Updated $PACKAGE_COUNT package definitions from remote"
                    log_info "  Local changes have been overwritten with remote versions"
                else
                    log_warn "  No package definitions found in repository"
                fi
            else
                log_warn "  Could not access repository packages directory"
            fi
        else
            log_warn "Git is not installed - cannot update packages from remote"
            log_warn "  Install git to enable automatic package updates during repair"
            log_warn "  Or manually copy packages to $PREFIX/packages/"
        fi
        log_info ""
    fi

    log_info ""
    log_info "========================================="
    log_info "TSI installed successfully!"
    log_info "========================================="
    log_info ""
    log_info "TSI is isolated: it uses its own bin directory ($PREFIX/bin)"
    log_info "and prefers TSI-installed tools over system tools."
    log_info ""

    # Handle PATH export and autocompletion setup
    if [ "$NON_INTERACTIVE" = true ]; then
        # Non-interactive mode - print instructions
        log_info "To use TSI in this terminal session, run:"
        log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
        log_info ""
        log_info "This command only adds TSI to your PATH and leaves everything else unchanged."
        log_info ""
    else
        # Interactive mode - prompt user
        log_info ""
        log_info "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        log_info "Setup Options"
        log_info "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        log_info ""

        # Prompt for current session PATH export
        # Write prompt to /dev/tty so it appears on the terminal
        # Use command grouping to prevent history recording
        { printf "[INFO] Add TSI to PATH for this terminal session? (y/n): " > /dev/tty; if [ -c /dev/tty ] 2>/dev/null; then read -r export_path_response < /dev/tty; else read -r export_path_response; fi; printf "\n" > /dev/tty; }
        if [ "$export_path_response" = "y" ] || [ "$export_path_response" = "Y" ] || [ "$export_path_response" = "yes" ]; then
            export PATH="$PREFIX/bin:$PATH"
            log_info "✓ Added TSI to PATH for current terminal session"
            log_info ""
        else
            log_info "Skipped PATH export for current session."
            log_info "You can add it manually with: export PATH=\"$PREFIX/bin:\$PATH\""
            log_info ""
        fi

        # Prompt for permanent PATH setup
        # Use command grouping to prevent history recording
        { printf "[INFO] Add TSI to PATH permanently in your shell config? (y/n): " > /dev/tty; if [ -c /dev/tty ] 2>/dev/null; then read -r permanent_path_response < /dev/tty; else read -r permanent_path_response; fi; printf "\n" > /dev/tty; }
        if [ "$permanent_path_response" = "y" ] || [ "$permanent_path_response" = "Y" ] || [ "$permanent_path_response" = "yes" ]; then
            if [ -n "$ZSH_VERSION" ] || [ -n "$ZSH" ]; then
                SHELL_CONFIG="$HOME/.zshrc"
            else
                SHELL_CONFIG="$HOME/.bashrc"
            fi

            # Check if already added
            if grep -q "export PATH=\"$PREFIX/bin:\$PATH\"" "$SHELL_CONFIG" 2>/dev/null; then
                log_info "PATH already configured in $SHELL_CONFIG"
            else
                echo "" >> "$SHELL_CONFIG"
                echo "# TSI package manager" >> "$SHELL_CONFIG"
                echo "export PATH=\"$PREFIX/bin:\$PATH\"" >> "$SHELL_CONFIG"
                # Mark that we added PATH so completion can be added under same comment later
                PATH_ADDED_IN_SESSION=true
                log_info "✓ Added PATH to $SHELL_CONFIG"
            fi
            log_info ""
        else
            log_info "Skipped permanent PATH setup."
            log_info ""
        fi

        # Prompt for autocompletion
        if [ -f "$PREFIX/share/completions/tsi.bash" ] || [ -f "$PREFIX/share/completions/tsi.zsh" ]; then
            # Use command grouping to prevent history recording
            { printf "[INFO] Enable shell autocompletion for TSI? (y/n): " > /dev/tty; if [ -c /dev/tty ] 2>/dev/null; then read -r autocomplete_response < /dev/tty; else read -r autocomplete_response; fi; printf "\n" > /dev/tty; }
            if [ "$autocomplete_response" = "y" ] || [ "$autocomplete_response" = "Y" ] || [ "$autocomplete_response" = "yes" ]; then
                if [ -n "$ZSH_VERSION" ] || [ -n "$ZSH" ]; then
                    SHELL_CONFIG="$HOME/.zshrc"
                    COMPLETION_FILE="$PREFIX/share/completions/tsi.zsh"
                else
                    SHELL_CONFIG="$HOME/.bashrc"
                    COMPLETION_FILE="$PREFIX/share/completions/tsi.bash"
                fi
                if [ -f "$COMPLETION_FILE" ]; then
                    if grep -q "source $COMPLETION_FILE" "$SHELL_CONFIG" 2>/dev/null; then
                        log_info "Autocompletion already configured in $SHELL_CONFIG"
                    else
                        # If PATH was added in this session, add completion under the same comment
                        if [ "${PATH_ADDED_IN_SESSION:-false}" = "true" ]; then
                            echo "source $COMPLETION_FILE" >> "$SHELL_CONFIG"
                            log_info "✓ Added autocompletion to $SHELL_CONFIG (under TSI package manager section)"
                        else
                            # PATH wasn't added, add completion with its own comment
                            echo "" >> "$SHELL_CONFIG"
                            echo "# TSI package manager" >> "$SHELL_CONFIG"
                            echo "source $COMPLETION_FILE" >> "$SHELL_CONFIG"
                            log_info "✓ Added autocompletion to $SHELL_CONFIG"
                        fi
                        log_info "  Run 'source $SHELL_CONFIG' to enable in current session"
                    fi
                fi
                log_info ""
            else
                log_info "Skipped autocompletion setup."
                log_info ""
            fi
        fi
    fi

    # Show manual setup instructions only in non-interactive mode
    if [ "$NON_INTERACTIVE" = true ]; then
        log_info "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        log_info "Manual Setup Instructions"
        log_info "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
        log_info ""
        log_info "The commands below only add TSI to your PATH and leave everything else unchanged."
        log_info ""
        if [ -n "$ZSH_VERSION" ] || [ -n "$ZSH" ]; then
            log_info "For zsh, add to ~/.zshrc:"
            log_info ""
            log_info "  echo 'export PATH=\"$PREFIX/bin:\\\$PATH\"' >> ~/.zshrc"
            log_info "  source ~/.zshrc"
            log_info ""
            log_info "Or manually edit ~/.zshrc and add:"
            log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
            log_info ""
            if [ -f "$PREFIX/share/completions/tsi.zsh" ]; then
                log_info "To enable autocomplete (zsh):"
                log_info "  echo 'source $PREFIX/share/completions/tsi.zsh' >> ~/.zshrc"
                log_info "  source ~/.zshrc"
            fi
        else
            log_info "For bash, add to ~/.bashrc:"
            log_info ""
        log_info "  echo 'export PATH=\"$PREFIX/bin:\\\$PATH\"' >> ~/.bashrc"
            log_info "  source ~/.bashrc"
            log_info ""
            log_info "Or manually edit ~/.bashrc and add:"
            log_info "  export PATH=\"$PREFIX/bin:\$PATH\""
            log_info ""
            if [ -f "$PREFIX/share/completions/tsi.bash" ]; then
                log_info "To enable autocomplete (bash):"
                log_info "  echo 'source $PREFIX/share/completions/tsi.bash' >> ~/.bashrc"
                log_info "  source ~/.bashrc"
            fi
        fi
        log_info ""
        log_info ""
    fi
    if [ "$REPAIR_MODE" != true ]; then
        log_info "Package repository is ready! Try: tsi list"
    else
        log_info "Then run: tsi --help"
    fi
    log_info ""
}

main "$@"

