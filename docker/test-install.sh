#!/bin/sh
# Test script for TSI Rust version
# Tests build, installation, and basic functionality

set +e

echo "Cleaning up any previous TSI installations..."
rm -rf /root/.tsi
rm -rf /root/.tsi-test
rm -rf /tmp/tsi-build
echo "✓ Cleanup complete"
echo ""

echo "=========================================="
echo "TSI Rust Installation Test"
echo "=========================================="
echo ""
echo "System Information:"
echo "  OS: $(uname -a)"
echo "  Shell: $SHELL"
echo ""

echo "Available Tools:"
command -v cargo >/dev/null 2>&1 && echo "  ✓ cargo: $(cargo --version 2>&1)" || echo "  ✗ cargo: not found"
command -v rustc >/dev/null 2>&1 && echo "  ✓ rustc: $(rustc --version 2>&1)" || echo "  ✗ rustc: not found"
command -v gcc >/dev/null 2>&1 && echo "  ✓ gcc: $(gcc --version 2>&1 | head -1)" || echo "  ✗ gcc: not found"
command -v make >/dev/null 2>&1 && echo "  ✓ make: $(make --version 2>&1 | head -1)" || echo "  ✗ make: not found"
echo ""

echo "=========================================="
echo "Building TSI Rust Version"
echo "=========================================="
echo ""

SOURCE_DIR="/root/tsi-source"
if [ ! -d "$SOURCE_DIR" ]; then
    echo "ERROR: TSI source directory not found!"
    exit 1
fi

cd "$SOURCE_DIR"

if ! command -v cargo >/dev/null 2>&1; then
    echo "✗ cargo not found (Rust toolchain required)"
    exit 1
fi

echo "Building TSI with cargo..."
if ! cargo build --release 2>&1; then
    echo "✗ Build failed"
    exit 1
fi

TSI_BIN="target/release/tsi"
if [ ! -f "$TSI_BIN" ]; then
    echo "✗ Binary not found after build"
    exit 1
fi

echo "✓ Binary created: $TSI_BIN"
ls -lh "$TSI_BIN"
echo ""

echo "=========================================="
echo "Testing TSI Binary"
echo "=========================================="
echo ""

FAILED=0
run_test() {
    local name="$1"
    local cmd="$2"
    echo "Testing $name..."
    if eval "$cmd" >/dev/null 2>&1; then
        echo "✓ $name works"
        return 0
    else
        echo "✗ $name failed"
        eval "$cmd" 2>&1
        FAILED=1
        return 1
    fi
}

run_test "--help" "./$TSI_BIN --help"
run_test "--version" "./$TSI_BIN --version"
run_test "list" "./$TSI_BIN list"
run_test "doctor" "./$TSI_BIN doctor"

echo ""
echo "Setting up package repository..."
mkdir -p /root/.tsi/packages
cp packages/*.json /root/.tsi/packages/ 2>/dev/null || true

if [ -d "packages" ] && [ -n "$(ls packages/*.json 2>/dev/null)" ]; then
    run_test "info" "./$TSI_BIN info zlib"
    run_test "search" "./$TSI_BIN search zlib"
    run_test "update" "./$TSI_BIN update --local /root/tsi-source/packages"
fi

if [ "$FAILED" -eq 1 ]; then
    echo ""
    echo "Some tests failed!"
    exit 1
fi

echo ""
echo "=========================================="
echo "Test Completed Successfully!"
echo "=========================================="
exit 0
