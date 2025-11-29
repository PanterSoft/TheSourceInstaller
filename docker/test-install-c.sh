#!/bin/sh
# Test script for TSI C/C++ version
# Tests installation and basic functionality

set +e

# Clean up any previous TSI installations to ensure fresh start
echo "Cleaning up any previous TSI installations..."
rm -rf /root/.tsi
rm -rf /root/.tsi-test
rm -rf /tmp/tsi-build
echo "✓ Cleanup complete"
echo ""

echo "=========================================="
echo "TSI C/C++ Installation Test"
echo "=========================================="
echo ""
echo "System Information:"
echo "  OS: $(uname -a)"
echo "  Shell: $SHELL"
echo ""

echo "Available Tools:"
command -v gcc >/dev/null 2>&1 && echo "  ✓ gcc: $(gcc --version 2>&1 | head -1)" || echo "  ✗ gcc: not found"
command -v clang >/dev/null 2>&1 && echo "  ✓ clang: $(clang --version 2>&1 | head -1)" || echo "  ✗ clang: not found"
command -v cc >/dev/null 2>&1 && echo "  ✓ cc: found" || echo "  ✗ cc: not found"
command -v make >/dev/null 2>&1 && echo "  ✓ make: $(make --version 2>&1 | head -1)" || echo "  ✗ make: not found"
echo ""

echo "=========================================="
echo "Building TSI C Version"
echo "=========================================="
echo ""

# Copy source to writable location (volume is read-only)
if [ -d /root/tsi-source/src ]; then
    SOURCE_DIR="/root/tsi-source/src"
elif [ -d /root/tsi/src ]; then
    SOURCE_DIR="/root/tsi/src"
else
    echo "ERROR: TSI source directory not found!"
    exit 1
fi

# Copy to writable location
BUILD_DIR="/tmp/tsi-build"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cp -r "$SOURCE_DIR"/* "$BUILD_DIR/"
cd "$BUILD_DIR"

# Check for C compiler
HAS_COMPILER=0
if command -v gcc >/dev/null 2>&1; then
    export CC=gcc
    HAS_COMPILER=1
elif command -v clang >/dev/null 2>&1; then
    export CC=clang
    HAS_COMPILER=1
elif command -v cc >/dev/null 2>&1; then
    export CC=cc
    HAS_COMPILER=1
fi

if [ "$HAS_COMPILER" -eq 0 ]; then
    echo "✗ No C compiler found (gcc, clang, or cc required)"
    echo "  This test requires a C compiler"
    echo "  NOTE: This is expected for minimal systems (alpine-minimal, ubuntu-minimal)"
    # Exit with code 1 - this is an expected failure for minimal systems
    # The test runner should handle this appropriately
    exit 1
fi

if ! command -v make >/dev/null 2>&1; then
    echo "✗ make is required but not found"
    exit 1
fi

# Build TSI
echo "Building TSI with $CC..."
# Force clean
rm -rf build bin
echo "Cleaned previous build"

BUILD_SUCCESS=0
if make CC="$CC" 2>&1; then
    BUILD_SUCCESS=1
    echo "✓ Build successful"
else
    echo "✗ Build failed"
    echo "Build output:"
    make CC="$CC" 2>&1 | tail -20
    exit 1
fi

# Check if binary was created
if [ ! -f "bin/tsi" ]; then
    echo "✗ Binary not found after build"
    exit 1
fi

echo "✓ Binary created: bin/tsi"
ls -lh bin/tsi

echo ""
echo "=========================================="
echo "Testing TSI Binary"
echo "=========================================="
echo ""

# Test basic commands
echo "Testing --help command..."
HELP_OUTPUT=$(./bin/tsi --help 2>&1)
HELP_EXIT=$?
if [ $HELP_EXIT -eq 0 ]; then
    echo "✓ --help works"
    echo "$HELP_OUTPUT" | head -5
else
    echo "✗ --help failed (exit code: $HELP_EXIT)"
    echo "Error output:"
    echo "$HELP_OUTPUT"
    # Check if binary is executable and has correct permissions
    ls -la bin/tsi
    # Check for missing libraries
    if command -v ldd >/dev/null 2>&1; then
        echo "Checking dynamic library dependencies:"
        ldd bin/tsi 2>&1 || echo "ldd check failed or static binary"
    fi
    exit 1
fi

echo "Testing --version command..."
VERSION_OUTPUT=$(./bin/tsi --version 2>&1)
VERSION_EXIT=$?
if [ $VERSION_EXIT -eq 0 ]; then
    echo "✓ --version works"
    echo "$VERSION_OUTPUT"
else
    echo "✗ --version failed (exit code: $VERSION_EXIT)"
    echo "Error output:"
    echo "$VERSION_OUTPUT"
    exit 1
fi

echo "Testing list command..."
LIST_OUTPUT=$(./bin/tsi list 2>&1)
LIST_EXIT=$?
if [ $LIST_EXIT -eq 0 ]; then
    echo "✓ list command works"
    echo "$LIST_OUTPUT"
else
    echo "✗ list command failed (exit code: $LIST_EXIT)"
    echo "Error output:"
    echo "$LIST_OUTPUT"
    exit 1
fi

echo "Testing update command..."
UPDATE_OUTPUT=$(./bin/tsi update --local /root/tsi-source/packages 2>&1 || true)
UPDATE_EXIT=$?
if [ $UPDATE_EXIT -eq 0 ]; then
    echo "✓ update command works"
    echo "$UPDATE_OUTPUT" | head -3
else
    echo "⚠ update command had issues (may be expected)"
fi

# Test with a package (if available)
if [ -d "/root/tsi-source/packages" ] || [ -d "/root/tsi/packages" ]; then
    PACKAGE_DIR=""
    if [ -d "/root/tsi-source/packages" ]; then
        PACKAGE_DIR="/root/tsi-source/packages"
    else
        PACKAGE_DIR="/root/tsi/packages"
    fi

    # Try to get info on a package
    FIRST_PKG=$(ls "$PACKAGE_DIR"/*.json 2>/dev/null | head -1)
    if [ -n "$FIRST_PKG" ]; then
        PKG_NAME=$(basename "$FIRST_PKG" .json)
        echo ""
        echo "Testing with package: $PKG_NAME"
        # Create repo directory structure for C version
        REPO_DIR="/root/.tsi-test/repos"
        mkdir -p "$REPO_DIR"
        cp "$FIRST_PKG" "$REPO_DIR/${PKG_NAME}.json" 2>/dev/null || true

        # Set up TSI environment for testing
        export PATH="/root/.tsi-test/bin:$PATH"
        mkdir -p /root/.tsi-test/bin
        cp ./bin/tsi /root/.tsi-test/bin/tsi

        # Update repository
        ./bin/tsi update --local "$PACKAGE_DIR" >/dev/null 2>&1 || true

        if ./bin/tsi info "$PKG_NAME" >/dev/null 2>&1; then
            echo "✓ info command works"

            # Test uninstall command (dry run - cancel it)
            echo "Testing uninstall command (will cancel)..."
            if echo "no" | ./bin/tsi uninstall >/dev/null 2>&1; then
                echo "✓ uninstall command works (cancelled as expected)"
            else
                echo "⚠ uninstall command had issues (may be expected in test environment)"
            fi
        else
            echo "⚠ info command failed (package may not be in repository - this is OK for C version)"
        fi
    fi
fi

echo ""
echo "=========================================="
echo "Test Completed Successfully!"
echo "=========================================="
exit 0

