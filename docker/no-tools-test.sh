#!/bin/sh
# Proves the static tsi binary needs zero runtime dependencies.
#
# Runs against a bare distro container with NOTHING installed beyond the
# base image (no compiler, no make, no package definitions). Confirms
# `tsi --version`, `tsi --help`, and `tsi doctor` all work unaided, and
# that `tsi install <pkg>` fails GRACEFULLY (non-zero exit, no Rust panic)
# rather than crashing when there is no toolchain and no package registry.
#
# POSIX sh only -- runs unmodified on Alpine (busybox ash) and
# Debian/Ubuntu (dash). No bashisms.
#
# Usage: no-tools-test.sh [path-to-tsi-binary]

set -u

TSI_BIN="${1:-/artifacts/tsi}"
FAILED=0

ok()   { printf '  \342\234\223 %s\n' "$1"; }
fail() { printf '  \342\234\227 %s\n' "$1"; FAILED=1; }

check() {
  desc="$1"
  shift
  if "$@" >/tmp/no-tools-step.log 2>&1; then
    ok "$desc"
  else
    fail "$desc"
    echo "---- output ----"
    cat /tmp/no-tools-step.log
    echo "-----------------"
  fi
}

echo "=========================================="
echo "TSI Zero-Dependency Test"
echo "=========================================="
echo "  distro:  $(cat /etc/os-release 2>/dev/null | grep -m1 '^PRETTY_NAME=' | cut -d= -f2- | tr -d '\"' || uname -s)"
echo "  arch:    $(uname -m)"
echo "  tsi bin: $TSI_BIN"
echo ""

if [ ! -x "$TSI_BIN" ]; then
  chmod +x "$TSI_BIN" 2>/dev/null || true
fi
if [ ! -x "$TSI_BIN" ]; then
  echo "tsi binary not found or not executable at: $TSI_BIN" >&2
  exit 1
fi

check "tsi --version (no libc/toolchain installed)" "$TSI_BIN" --version
check "tsi --help" "$TSI_BIN" --help
check "tsi doctor" "$TSI_BIN" doctor

echo ""
echo "Checking graceful failure of 'tsi install zlib' (no toolchain, no registry)..."
if "$TSI_BIN" install zlib >/tmp/no-tools-install.log 2>&1; then
  fail "tsi install zlib unexpectedly succeeded with nothing installed"
  cat /tmp/no-tools-install.log
else
  ok "tsi install zlib exited non-zero, as expected"
  if grep -qi "panicked" /tmp/no-tools-install.log; then
    fail "output contains a Rust panic (should be a clean error message)"
    cat /tmp/no-tools-install.log
  else
    ok "no panic in output"
  fi
fi

echo ""
echo "=========================================="
if [ "$FAILED" -ne 0 ]; then
  echo "FAILURES DETECTED"
  echo "=========================================="
  exit 1
fi
echo "All checks passed"
echo "=========================================="
exit 0
