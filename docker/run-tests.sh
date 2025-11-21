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

# Colors - check if terminal supports colors
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
    GREEN=$(tput setaf 2)
    RED=$(tput setaf 1)
    YELLOW=$(tput setaf 3)
    NC=$(tput sgr0)
else
    # Fallback to ANSI codes if tput not available but terminal might support it
    if [ -t 1 ]; then
        GREEN='\033[0;32m'
        RED='\033[0;31m'
        YELLOW='\033[1;33m'
        NC='\033[0m'
    else
        # No colors if output is redirected
        GREEN=''
        RED=''
        YELLOW=''
        NC=''
    fi
fi

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

    # Clean up any existing containers and volumes for this service
    echo "Cleaning up any existing containers..."
    docker compose rm -f -v "$service" >/dev/null 2>&1 || docker-compose rm -f -v "$service" >/dev/null 2>&1 || true
    
    # Build the container (use cache for base image, but TSI will be fresh due to test script cleanup)
    echo "Building container (using cache for base image)..."
    if (docker compose build "$service" >/dev/null 2>&1 || docker-compose build "$service" >/dev/null 2>&1); then
        echo "✓ Container built"
    else
        echo "✗ Container build failed"
        return 1
    fi

    # Use C version test script
    TEST_SCRIPT="test-install-c.sh"

    # Run test script in a fresh container (--rm removes container after run)
    echo "Running installation test..."
    set +e  # Temporarily disable exit on error for command substitution
    # Determine which docker compose command to use
    DOCKER_COMPOSE_CMD=""
    if command -v docker >/dev/null 2>&1 && docker compose version >/dev/null 2>&1; then
        DOCKER_COMPOSE_CMD="docker compose"
    elif command -v docker-compose >/dev/null 2>&1; then
        DOCKER_COMPOSE_CMD="docker-compose"
    else
        echo "Error: Neither 'docker compose' nor 'docker-compose' found"
        set -e
        return 1
    fi
    
    # Run the test
    $DOCKER_COMPOSE_CMD run --rm --no-deps "$service" /bin/sh /root/tsi-source/docker/$TEST_SCRIPT >/tmp/test-output-$$.log 2>&1
    TEST_EXIT_CODE=$?
    TEST_OUTPUT=$(cat /tmp/test-output-$$.log)
    rm -f /tmp/test-output-$$.log
    set -e  # Re-enable exit on error
    echo "$TEST_OUTPUT" | tee "/tmp/tsi-test-${service}.log"

    # Check result based on expected outcome
    if [ "$expected" = "fail" ]; then
        # Expected to fail - check if it failed gracefully
        if [ "$TEST_EXIT_CODE" -ne 0 ]; then
            echo ""
            echo -e "${GREEN}✓ Test passed (expected failure): $description${NC}"
            return 0
        else
            echo ""
            echo -e "${YELLOW}⚠ Test unexpectedly succeeded: $description${NC}"
            echo "Log saved to: /tmp/tsi-test-${service}.log"
            return 1
        fi
    else
        # Expected to pass
        if [ "$TEST_EXIT_CODE" -eq 0 ]; then
            echo ""
            echo -e "${GREEN}✓ Test passed: $description${NC}"
            return 0
        else
            echo ""
            echo -e "${RED}✗ Test failed: $description${NC}"
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

    # Clean up containers and volumes
    echo "Cleaning up containers and volumes..."
    set +e  # Disable exit on error for cleanup
    docker compose down -v >/dev/null 2>&1 || docker-compose down -v >/dev/null 2>&1 || true
    docker compose rm -f -v >/dev/null 2>&1 || docker-compose rm -f -v >/dev/null 2>&1 || true
    set -e  # Re-enable exit on error
done

# Summary
echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo ""
echo -e "Passed: ${GREEN}$PASSED${NC}"
echo -e "Failed: ${RED}$FAILED${NC}"
echo "Total:  $((PASSED + FAILED))"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed. Check logs in /tmp/tsi-test-*.log${NC}"
    exit 1
fi

