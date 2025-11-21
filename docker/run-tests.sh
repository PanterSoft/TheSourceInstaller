#!/bin/bash
# Run TSI installation tests in Docker containers
# Tests different minimal system configurations

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "TSI Docker Test Suite"
echo "=========================================="
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Test scenarios
declare -a SCENARIOS=(
    "alpine-minimal:Alpine Linux - Absolutely minimal (no tools)"
    "alpine-python:Alpine Linux - Python available, no build tools"
    "alpine-build:Alpine Linux - Build tools available, no Python"
    "ubuntu-minimal:Ubuntu - Minimal system (no tools)"
    "ubuntu-python:Ubuntu - Python available, no build tools"
    "ubuntu-build:Ubuntu - Build tools available, no Python"
)

# Function to run test
run_test() {
    local service=$1
    local description=$2

    echo ""
    echo "=========================================="
    echo "Testing: $description"
    echo "=========================================="
    echo ""

    # Build the container
    echo "Building container..."
    if docker-compose build "$service" >/dev/null 2>&1; then
        echo "✓ Container built"
    else
        echo "✗ Container build failed"
        return 1
    fi

    # Run test script
    echo "Running installation test..."
    if docker-compose run --rm "$service" /bin/sh /root/tsi-source/docker/test-install.sh 2>&1 | tee "/tmp/tsi-test-${service}.log"; then
        echo ""
        echo "${GREEN}✓ Test passed: $description${NC}"
        return 0
    else
        echo ""
        echo "${RED}✗ Test failed: $description${NC}"
        echo "Log saved to: /tmp/tsi-test-${service}.log"
        return 1
    fi
}

# Main test execution
PASSED=0
FAILED=0

for scenario in "${SCENARIOS[@]}"; do
    IFS=':' read -r service description <<< "$scenario"

    if run_test "$service" "$description"; then
        ((PASSED++))
    else
        ((FAILED++))
    fi

    # Clean up
    docker-compose down >/dev/null 2>&1 || true
done

# Summary
echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo ""
echo "Passed: ${GREEN}$PASSED${NC}"
echo "Failed: ${RED}$FAILED${NC}"
echo "Total:  $((PASSED + FAILED))"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo "${RED}Some tests failed. Check logs in /tmp/tsi-test-*.log${NC}"
    exit 1
fi

