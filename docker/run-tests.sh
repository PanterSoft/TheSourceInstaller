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
# Format: "service:description:expected_result"
# expected_result: "pass" or "fail" (fail means expected to fail gracefully)
declare -a SCENARIOS=(
    "alpine-minimal:Alpine Linux - Absolutely minimal (no tools):fail"
    "alpine-c-only:Alpine Linux - C compiler only:pass"
    "ubuntu-minimal:Ubuntu - Minimal system (no tools):fail"
)

# Function to run test
run_test() {
    local service=$1
    local description=$2
    local expected=$3

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

    # Use C version test script
    TEST_SCRIPT="test-install-c.sh"

    # Run test script
    echo "Running installation test..."
    TEST_OUTPUT=$(docker-compose run --rm "$service" /bin/sh /root/tsi-source/docker/$TEST_SCRIPT 2>&1)
    TEST_EXIT_CODE=$?
    echo "$TEST_OUTPUT" | tee "/tmp/tsi-test-${service}.log"

    # Check result based on expected outcome
    if [ "$expected" = "fail" ]; then
        # Expected to fail - check if it failed gracefully
        if [ "$TEST_EXIT_CODE" -ne 0 ]; then
            echo ""
            echo "${GREEN}✓ Test passed (expected failure): $description${NC}"
            return 0
        else
            echo ""
            echo "${YELLOW}⚠ Test unexpectedly succeeded: $description${NC}"
            echo "Log saved to: /tmp/tsi-test-${service}.log"
            return 1
        fi
    else
        # Expected to pass
        if [ "$TEST_EXIT_CODE" -eq 0 ]; then
            echo ""
            echo "${GREEN}✓ Test passed: $description${NC}"
            return 0
        else
            echo ""
            echo "${RED}✗ Test failed: $description${NC}"
            echo "Log saved to: /tmp/tsi-test-${service}.log"
            return 1
        fi
    fi
}

# Main test execution
PASSED=0
FAILED=0

for scenario in "${SCENARIOS[@]}"; do
    IFS=':' read -r service description expected <<< "$scenario"

    if run_test "$service" "$description" "$expected"; then
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

