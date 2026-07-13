#!/bin/sh
# End-to-end smoke test for a prebuilt, statically-linked tsi binary.
#
# Runs inside a bare distro container that ships nothing but its package
# manager. Installs a C compiler + make (nothing else), points tsi at the
# tsi-packages package definitions checked out alongside this repo, and
# drives a full install -> list -> info -> uninstall cycle against a real
# source package build.
#
# POSIX sh only -- this must run unmodified on Alpine (busybox ash),
# Debian/Ubuntu (dash), and Fedora (bash-as-sh). No bashisms.
#
# Usage:
#   e2e-test.sh [path-to-tsi-binary]
#   TSI_BIN=/path/to/tsi e2e-test.sh
#
# Env overrides:
#   TSI_BIN            path to the tsi binary (default: $1, then /work/tsi)
#   TSI_PACKAGES_DIR   path to package definitions (default: /work/tsi-packages/packages)
#   TSI_PREFIX         tsi install prefix (default: $HOME/.tsi, tsi's own default)
#
# Package choice: bzip2, not zlib. zlib.json's build_commands run an inline
# `python3` heredoc to patch zutil.h, but python3 is not declared in zlib's
# build_dependencies and is not installed by this script (see docker/README.md
# for details). bzip2 has build_dependencies: [] and build_system "make", so a
# bare cc+make toolchain is genuinely sufficient to build it -- and it is
# literally one of TSI's own BOOTSTRAP_PACKAGES (src/core/bootstrap.rs), so
# it's a representative real-world source package rather than a toy example.

set -eu

step_num=0

ok() {
  step_num=$((step_num + 1))
  printf '  \342\234\223 [%d] %s\n' "$step_num" "$1"
}

fail() {
  step_num=$((step_num + 1))
  printf '  \342\234\227 [%d] %s\n' "$step_num" "$1"
}

# Runs "$@", printing a ok/fail progress line. On failure, dumps captured
# output and aborts the whole script immediately (fail-fast, non-zero exit).
run_step() {
  desc="$1"
  shift
  if "$@" >/tmp/e2e-step.log 2>&1; then
    ok "$desc"
  else
    fail "$desc"
    echo "---- output ----"
    cat /tmp/e2e-step.log
    echo "-----------------"
    exit 1
  fi
}

install_toolchain() {
  if command -v apk >/dev/null 2>&1; then
    apk add --no-cache gcc make musl-dev
  elif command -v apt-get >/dev/null 2>&1; then
    apt-get update -qq
    # libc6-dev is only a Recommends of gcc, so with --no-install-recommends
    # it must be named explicitly or every #include <stdlib.h> fails.
    apt-get install -y --no-install-recommends gcc make libc6-dev
  elif command -v dnf >/dev/null 2>&1; then
    dnf install -y gcc make
  else
    echo "No supported package manager found (need apk, apt-get, or dnf)" >&2
    return 1
  fi

  # Some distros (notably Alpine) don't ship a `cc` alias for gcc, but both
  # tsi's own `doctor` check and generated Makefiles invoke `cc`/`$(CC)`
  # directly. Normalize it so behavior doesn't depend on distro packaging.
  if ! command -v cc >/dev/null 2>&1; then
    gcc_path="$(command -v gcc)"
    ln -sf "$gcc_path" /usr/local/bin/cc 2>/dev/null || ln -sf "$gcc_path" /usr/bin/cc
  fi
}

# Only look under $PREFIX/install (installed artifacts + symlink farm).
# $PREFIX/sources is a deliberate source/download cache that survives
# uninstall, and the bzip2 source tree contains libbz2.a build artifacts
# that would make a prefix-wide find false-fail (and false-pass the
# install check before anything is linked).
find_libbz2() {
  find "$PREFIX/install" -name 'libbz2*' 2>/dev/null | grep -q .
}

list_has_bzip2() {
  "$TSI_BIN" list 2>&1 | grep -q 'bzip2'
}

verify_removed() {
  ! find "$PREFIX/install" -name 'libbz2*' 2>/dev/null | grep -q .
}

TSI_BIN="${TSI_BIN:-${1:-/work/tsi}}"
if [ ! -x "$TSI_BIN" ]; then
  chmod +x "$TSI_BIN" 2>/dev/null || true
fi
if [ ! -x "$TSI_BIN" ]; then
  echo "tsi binary not found or not executable at: $TSI_BIN" >&2
  echo "Set TSI_BIN or pass the path as \$1." >&2
  exit 1
fi

HOME="${HOME:-/root}"
export HOME
PREFIX="${TSI_PREFIX:-$HOME/.tsi}"
PKG_DEFS_DIR="${TSI_PACKAGES_DIR:-/work/tsi-packages/packages}"

echo "=========================================="
echo "TSI Docker E2E Test"
echo "=========================================="
echo "  distro:   $(cat /etc/os-release 2>/dev/null | grep -m1 '^PRETTY_NAME=' | cut -d= -f2- | tr -d '\"' || uname -s)"
echo "  arch:     $(uname -m)"
echo "  tsi bin:  $TSI_BIN"
echo "  prefix:   $PREFIX"
echo "  packages: $PKG_DEFS_DIR"
echo ""

run_step "tsi --version" "$TSI_BIN" --version
run_step "tsi --help" "$TSI_BIN" --help
run_step "tsi doctor" "$TSI_BIN" doctor
run_step "install C toolchain (cc + make)" install_toolchain
run_step "tsi update --local $PKG_DEFS_DIR" "$TSI_BIN" update --local "$PKG_DEFS_DIR"
run_step "tsi install bzip2 (real source build)" "$TSI_BIN" install bzip2
run_step "verify libbz2 artifact under $PREFIX/install" find_libbz2
run_step "tsi list shows bzip2" list_has_bzip2
run_step "tsi info bzip2" "$TSI_BIN" info bzip2
run_step "tsi uninstall bzip2" "$TSI_BIN" uninstall bzip2
run_step "verify bzip2 files removed" verify_removed

echo ""
echo "=========================================="
echo "All $step_num checks passed"
echo "=========================================="
exit 0
