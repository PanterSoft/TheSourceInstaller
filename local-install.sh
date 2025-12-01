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
echo "  Compiling source files..."
OBJECTS=""
for c_file in *.c; do
    if [ -f "$c_file" ]; then
        OBJECTS="$OBJECTS build/${c_file%.c}.o"
        if ! $CC $CFLAGS -c "$c_file" -o "build/${c_file%.c}.o"; then
            echo -e "${RED}✗ Compilation failed: $c_file${RESET}"
            exit 1
        fi
    fi
done

# Link
echo "  Linking binary..."
if ! $CC $LDFLAGS $OBJECTS -o bin/tsi; then
    echo -e "${RED}✗ Linking failed${RESET}"
    exit 1
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
mkdir -p "$TSI_PREFIX/repos"

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

# Copy local packages to repository (for testing local package changes)
if [ -d "packages" ]; then
    PACKAGE_COUNT=$(find packages -name "*.json" 2>/dev/null | wc -l | tr -d ' ')
    if [ "$PACKAGE_COUNT" -gt 0 ]; then
        cp packages/*.json "$TSI_PREFIX/repos/" 2>/dev/null || true
        echo -e "  ${GREEN}✓${RESET} Synced $PACKAGE_COUNT package definitions to repository"
    fi
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

