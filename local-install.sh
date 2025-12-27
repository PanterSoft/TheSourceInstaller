#!/bin/bash
# TSI Local Install Script
# Builds TSI from source and installs it to ~/.tsi for testing
# This allows testing changes without pushing to git
# No make required - compiles directly using C compiler

set -e

# Colors for output
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    CYAN='\033[0;36m'
    MAGENTA='\033[0;35m'
    RED='\033[0;31m'
    RESET='\033[0m'
    BOLD='\033[1m'
else
    GREEN=''
    YELLOW=''
    BLUE=''
    CYAN=''
    MAGENTA=''
    RED=''
    RESET=''
    BOLD=''
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Default TSI prefix
TSI_PREFIX="${TSI_PREFIX:-$HOME/.tsi}"

echo -e "${CYAN}${BOLD}═══>${RESET} ${MAGENTA}${BOLD}TSI Local Install${RESET}"
echo ""

# Check for C compiler
CC="${CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo -e "${RED}Error: C compiler ($CC) not found${RESET}"
    exit 1
fi

echo -e "${BLUE}Found C compiler:${RESET} $CC ($($CC --version 2>&1 | head -1))"
echo ""

# Build TSI
echo -e "${YELLOW}${BOLD}═══>${RESET} ${YELLOW}🔨 Building TSI${RESET}"
cd src

# Clean previous build
if [ -d "build" ]; then
    rm -rf build
    echo "  Cleaned previous build"
fi
if [ -d "bin" ]; then
    rm -rf bin
fi

# Create build directories
mkdir -p build bin

# Detect C compiler and include paths
CC="${CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    # Try alternatives
    for alt_cc in clang cc; do
        if command -v "$alt_cc" >/dev/null 2>&1; then
            CC="$alt_cc"
            echo -e "${BLUE}Using C compiler:${RESET} $CC"
            break
        fi
    done
    if ! command -v "$CC" >/dev/null 2>&1; then
        echo -e "${RED}Error: No C compiler found (tried: gcc, clang, cc)${RESET}"
        exit 1
    fi
fi

# Try to find standard include directories
INCLUDE_FLAGS=""
if ! echo '#include <stdio.h>' | $CC -E -x c - 2>/dev/null >/dev/null; then
    echo -e "${YELLOW}⚠${RESET} stdio.h not found in default include path, searching..."
    # Try common include locations
    for inc_dir in /usr/include /usr/local/include /opt/include; do
        if [ -d "$inc_dir" ] && [ -f "$inc_dir/stdio.h" ]; then
            INCLUDE_FLAGS="-I$inc_dir"
            echo -e "${GREEN}✓${RESET} Found stdio.h in $inc_dir"
            break
        fi
    done
    # If still not found, try to get include paths from compiler
    if [ -z "$INCLUDE_FLAGS" ]; then
        echo "Querying compiler for include paths..."
        COMPILER_INCLUDES=$($CC -E -v -x c - 2>&1 | grep -E '^ /' | head -5 | tr '\n' ' ' || true)
        if [ -n "$COMPILER_INCLUDES" ]; then
            # Extract first include directory
            FIRST_INC=$(echo "$COMPILER_INCLUDES" | awk '{print $1}')
            if [ -n "$FIRST_INC" ] && [ -d "$FIRST_INC" ]; then
                INCLUDE_FLAGS="-I$FIRST_INC"
                echo -e "${GREEN}✓${RESET} Using compiler include path: $FIRST_INC"
            fi
        fi
    fi
    if [ -z "$INCLUDE_FLAGS" ]; then
        echo -e "${RED}✗${RESET} Cannot find stdio.h. Please install C development headers."
        echo -e "${RED}  On Debian/Ubuntu:${RESET} apt-get install build-essential"
        echo -e "${RED}  On RedHat/CentOS:${RESET} yum install gcc glibc-devel"
        echo -e "${RED}  On Alpine:${RESET} apk add gcc musl-dev"
        exit 1
    fi
fi

# CFLAGS (suppress warnings, only show errors)
CFLAGS="-w -O2 -std=c11 -D_POSIX_C_SOURCE=200809L $INCLUDE_FLAGS"

# Detect OS for static linking (only works on Linux)
# Note: We'll try static linking first and fall back to dynamic if it fails
UNAME_S=$(uname -s 2>/dev/null || echo "Unknown")

# Compile all C source files (exclude TUI components)
echo "  Compiling source files..."
OBJECTS=""
COMPILED_COUNT=0
# Count files excluding tui_interactive.c and tui_style.c
TOTAL_FILES=0
for c_file in *.c; do
    if [ -f "$c_file" ] && [ "$c_file" != "tui_interactive.c" ] && [ "$c_file" != "tui_style.c" ]; then
        TOTAL_FILES=$((TOTAL_FILES + 1))
    fi
done

if [ "$TOTAL_FILES" -eq 0 ]; then
    echo -e "${RED}✗ No C source files found${RESET}"
    exit 1
fi

for c_file in *.c; do
    if [ -f "$c_file" ] && [ "$c_file" != "tui_interactive.c" ] && [ "$c_file" != "tui_style.c" ]; then
        COMPILED_COUNT=$((COMPILED_COUNT + 1))
        echo -n "    [$COMPILED_COUNT/$TOTAL_FILES] Compiling $c_file... "
        OBJECTS="$OBJECTS build/${c_file%.c}.o"
        # Capture compiler output (warnings suppressed by -w, only errors shown)
        COMPILE_OUTPUT=$($CC $CFLAGS -c "$c_file" -o "build/${c_file%.c}.o" 2>&1)
        COMPILE_EXIT=$?

        # Check if compilation actually succeeded
        if [ $COMPILE_EXIT -ne 0 ] || [ ! -f "build/${c_file%.c}.o" ]; then
            echo -e "${RED}✗${RESET}"
            echo "$COMPILE_OUTPUT" | head -10
            echo -e "${RED}✗ Compilation failed: $c_file${RESET}"
            exit 1
        fi

        # Show errors if any, otherwise show success (warnings are suppressed)
        if echo "$COMPILE_OUTPUT" | grep -q .; then
            echo ""
            echo "$COMPILE_OUTPUT" | head -5 | sed 's/^/      /'
        else
            echo -e "${GREEN}✓${RESET}"
        fi
    fi
done
echo "  Compiled $COMPILED_COUNT source files"

# Link (try static first, fall back to dynamic if static fails)
echo "  Linking binary..."
UNAME_S=$(uname -s 2>/dev/null || echo "Unknown")
LINK_SUCCESS=false
if [ "$UNAME_S" = "Linux" ]; then
    # Try static linking first (suppress warnings, only show errors)
    if $CC $OBJECTS -o bin/tsi -static -w 2>&1; then
        LINK_SUCCESS=true
    else
        # Static linking failed, try dynamic linking
        echo "    Static linking failed, trying dynamic linking..."
        rm -f bin/tsi
    fi
fi

# If static linking didn't work or we're not on Linux, try dynamic linking
if [ "$LINK_SUCCESS" = false ]; then
    if ! $CC $OBJECTS -o bin/tsi -w 2>&1; then
    echo -e "${RED}✗ Linking failed${RESET}"
    exit 1
    fi
fi

if [ -f "bin/tsi" ]; then
    echo -e "${GREEN}✓ TSI built successfully${RESET}"
else
    echo -e "${RED}✗ Binary not created${RESET}"
    exit 1
fi

cd ..

# Install to TSI prefix
echo ""
echo -e "${GREEN}${BOLD}═══>${RESET} ${GREEN}📥 Installing TSI to $TSI_PREFIX${RESET}"

# Create directories
mkdir -p "$TSI_PREFIX/bin"
mkdir -p "$TSI_PREFIX/share/completions"
mkdir -p "$TSI_PREFIX/packages"

# Copy binary
cp src/bin/tsi "$TSI_PREFIX/bin/tsi"
chmod +x "$TSI_PREFIX/bin/tsi"
echo -e "  ${GREEN}✓${RESET} Installed binary: $TSI_PREFIX/bin/tsi"

# Copy completion scripts if they exist
if [ -f "completions/tsi.bash" ]; then
    cp completions/tsi.bash "$TSI_PREFIX/share/completions/tsi.bash"
    chmod 644 "$TSI_PREFIX/share/completions/tsi.bash"
    echo -e "  ${GREEN}✓${RESET} Installed bash completion"
fi

if [ -f "completions/tsi.zsh" ]; then
    cp completions/tsi.zsh "$TSI_PREFIX/share/completions/tsi.zsh"
    chmod 644 "$TSI_PREFIX/share/completions/tsi.zsh"
    echo -e "  ${GREEN}✓${RESET} Installed zsh completion"
fi

# Create default config file (only if it doesn't exist)
# IMPORTANT: Never overwrites existing config file - user modifications are preserved
echo -e "  ${BLUE}Creating default configuration...${RESET}"
if [ ! -f "$TSI_PREFIX/tsi.cfg" ]; then
    cat > "$TSI_PREFIX/tsi.cfg" << 'EOF'
# TSI Configuration File
# This file controls TSI behavior
#
# Strict Isolation Mode
# When enabled, TSI will only use TSI-installed packages after bootstrap
# During bootstrap, minimal system tools (gcc, /bin/sh) are still used
# Set to 'true' to enable strict isolation, 'false' to disable (default)
strict_isolation=false
EOF
    echo -e "  ${GREEN}✓${RESET} Created default config file: $TSI_PREFIX/tsi.cfg"
else
    echo -e "  ${BLUE}✓${RESET} Config file already exists: $TSI_PREFIX/tsi.cfg (preserving user configuration)"
fi

# Copy local packages to repository (for testing local package changes)
echo -e "  ${BLUE}Setting up package repository...${RESET}"
if [ -d "packages" ]; then
    PACKAGE_COUNT=0
    for pkg_file in packages/*.json; do
        if [ -f "$pkg_file" ]; then
            pkg_name=$(basename "$pkg_file")
            cp "$pkg_file" "$TSI_PREFIX/packages/$pkg_name" 2>/dev/null || true
            PACKAGE_COUNT=$((PACKAGE_COUNT + 1))
        fi
    done
    if [ "$PACKAGE_COUNT" -gt 0 ]; then
        echo -e "  ${GREEN}✓${RESET} Installed $PACKAGE_COUNT package definitions"
        echo -e "  ${GREEN}✓${RESET} Package repository is ready to use!"
    else
        echo -e "  ${YELLOW}⚠${RESET} No package definitions found in packages/"
    fi
else
    echo -e "  ${YELLOW}⚠${RESET} Packages directory not found, repository will be empty"
fi

echo ""
echo -e "${CYAN}${BOLD}=========================================${RESET}"
echo -e "${GREEN}${BOLD}TSI local install complete!${RESET}"
echo -e "${CYAN}${BOLD}=========================================${RESET}"
echo ""

# Check if TSI is in PATH
if command -v tsi >/dev/null 2>&1; then
    echo -e "${GREEN}✓${RESET} TSI is available in PATH"
    echo -e "  Location: $(command -v tsi)"
else
    echo -e "${YELLOW}⚠${RESET} TSI is not in your PATH"
    echo ""
    echo "Add to your PATH:"
    echo -e "  ${CYAN}export PATH=\"$TSI_PREFIX/bin:\$PATH\"${RESET}"
    echo ""
    if [ -n "$ZSH_VERSION" ]; then
        echo "Or add to ~/.zshrc:"
        echo -e "  ${CYAN}echo 'export PATH=\"$TSI_PREFIX/bin:\\\$PATH\"' >> ~/.zshrc${RESET}"
    else
        echo "Or add to ~/.bashrc:"
        echo -e "  ${CYAN}echo 'export PATH=\"$TSI_PREFIX/bin:\\\$PATH\"' >> ~/.bashrc${RESET}"
    fi
fi

echo ""
echo -e "${BLUE}You can now test your changes with:${RESET}"
echo -e "  ${CYAN}tsi --version${RESET}"
echo -e "  ${CYAN}tsi install <package>${RESET}"
echo ""

