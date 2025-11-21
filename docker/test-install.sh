#!/bin/sh
# Test script to run inside Docker containers
# Tests the TSI bootstrap installation

set -e

echo "=========================================="
echo "TSI Installation Test"
echo "=========================================="
echo ""
echo "System Information:"
echo "  OS: $(uname -a)"
echo "  Shell: $SHELL"
echo ""

echo "Available Tools:"
command -v python3 >/dev/null 2>&1 && echo "  ✓ python3: $(python3 --version 2>&1)" || echo "  ✗ python3: not found"
command -v python >/dev/null 2>&1 && echo "  ✓ python: $(python --version 2>&1)" || echo "  ✗ python: not found"
command -v gcc >/dev/null 2>&1 && echo "  ✓ gcc: $(gcc --version 2>&1 | head -1)" || echo "  ✗ gcc: not found"
command -v make >/dev/null 2>&1 && echo "  ✓ make: $(make --version 2>&1 | head -1)" || echo "  ✗ make: not found"
command -v wget >/dev/null 2>&1 && echo "  ✓ wget: found" || echo "  ✗ wget: not found"
command -v curl >/dev/null 2>&1 && echo "  ✓ curl: found" || echo "  ✗ curl: not found"
command -v tar >/dev/null 2>&1 && echo "  ✓ tar: found" || echo "  ✗ tar: not found"
echo ""

echo "=========================================="
echo "Running Bootstrap Installer"
echo "=========================================="
echo ""

# Change to TSI directory (try both possible locations)
if [ -d /root/tsi ]; then
    cd /root/tsi
elif [ -d /root/tsi-source ]; then
    cd /root/tsi-source
else
    echo "ERROR: TSI directory not found!"
    echo "Current directory: $(pwd)"
    echo "Contents: $(ls -la /root/)"
    exit 1
fi

# Run the bootstrap installer
if [ -f bootstrap-default.sh ]; then
    echo "Running bootstrap-default.sh..."
    chmod +x bootstrap-default.sh
    PREFIX=/root/.tsi-test WORK_DIR=/tmp/tsi-work ./bootstrap-default.sh
else
    echo "ERROR: bootstrap-default.sh not found!"
    echo "Current directory: $(pwd)"
    echo "Contents: $(ls -la)"
    exit 1
fi

echo ""
echo "=========================================="
echo "Verifying Installation"
echo "=========================================="
echo ""

# Check if TSI was installed
TSI_BIN="/root/.tsi-test/bin/tsi"
if [ -x "$TSI_BIN" ]; then
    echo "✓ TSI binary found: $TSI_BIN"
    export PATH="/root/.tsi-test/bin:$PATH"
    if tsi --help >/dev/null 2>&1; then
        echo "✓ TSI command works"
        tsi --help | head -5
    else
        echo "✗ TSI command failed"
        exit 1
    fi
elif [ -x "$PREFIX/bin/tsi" ]; then
    echo "✓ TSI binary found: $PREFIX/bin/tsi"
    export PATH="$PREFIX/bin:$PATH"
    tsi --help >/dev/null 2>&1 && echo "✓ TSI command works" || echo "✗ TSI command failed"
elif command -v python3 >/dev/null 2>&1; then
    if python3 -m tsi.cli --help >/dev/null 2>&1; then
        echo "✓ TSI module works"
        python3 -m tsi.cli --help | head -5
    else
        echo "✗ TSI module failed"
        exit 1
    fi
else
    echo "✗ TSI installation not found"
    echo "Checking installation directory..."
    ls -la /root/.tsi-test/ 2>/dev/null || echo "Installation directory not found"
    exit 1
fi

echo ""
echo "=========================================="
echo "Test Completed Successfully!"
echo "=========================================="

