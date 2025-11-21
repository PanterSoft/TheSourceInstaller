#!/bin/sh
# Simple installation script for TSI
# Works with minimal requirements - only needs Python 3.8+

set -e

# Default installation prefix
PREFIX="${PREFIX:-$HOME/.local}"

echo "Installing TSI (TheSourceInstaller)..."
echo "Installation prefix: $PREFIX"

# Check Python version
if ! command -v python3 >/dev/null 2>&1; then
    echo "Error: python3 is required but not found"
    exit 1
fi

PYTHON_VERSION=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
echo "Found Python $PYTHON_VERSION"

# Check if Python version is sufficient (3.8+)
if ! python3 -c "import sys; exit(0 if sys.version_info >= (3, 8) else 1)"; then
    echo "Error: Python 3.8 or higher is required"
    exit 1
fi

# Create installation directory
INSTALL_DIR="$PREFIX/bin"
mkdir -p "$INSTALL_DIR"

# Install TSI
echo "Installing TSI to $INSTALL_DIR..."
python3 setup.py install --prefix="$PREFIX" --user

# Create wrapper script if needed
TSI_BIN="$INSTALL_DIR/tsi"
if [ ! -f "$TSI_BIN" ]; then
    # Try to find the installed script
    PYTHON_BIN_DIR=$(python3 -c "import site; print(site.USER_BASE + '/bin')" 2>/dev/null || echo "$INSTALL_DIR")
    if [ -f "$PYTHON_BIN_DIR/tsi" ]; then
        ln -sf "$PYTHON_BIN_DIR/tsi" "$TSI_BIN" 2>/dev/null || cp "$PYTHON_BIN_DIR/tsi" "$TSI_BIN"
    else
        # Create a simple wrapper
        cat > "$TSI_BIN" << 'EOF'
#!/bin/sh
exec python3 -m tsi.cli "$@"
EOF
        chmod +x "$TSI_BIN"
    fi
fi

echo ""
echo "TSI installed successfully!"
echo ""
echo "Add to your PATH:"
echo "  export PATH=\"$INSTALL_DIR:\$PATH\""
echo ""
echo "Or add to your shell profile (~/.bashrc, ~/.zshrc, etc.):"
echo "  echo 'export PATH=\"$INSTALL_DIR:\\\$PATH\"' >> ~/.bashrc"
echo ""
echo "Then run: tsi --help"

