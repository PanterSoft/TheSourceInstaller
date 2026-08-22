#!/usr/bin/env bash
# Smoke-test that named packages install on Linux, on every architecture.
#
#   docker/validate-packages.sh [--platform linux/arm64,linux/amd64] PKG [PKG...]
#   docker/validate-packages.sh --fresh PKG    # discard the prefix first
#   docker/validate-packages.sh --all          # CI only; see below
#
# Scope: this is for sanity-checking a definition you are editing. Building the
# catalogue is CI's job -- test-build-packages.yml builds changed packages on
# three platforms per PR, and validate-all-packages.yml rebuilds everything
# weekly and regenerates PACKAGES_STATUS.md. Results from a laptop do not go in
# that table.
#
# Builds tsi inside a container once per platform (cached in a named volume),
# then runs `tsi install` for each package. On an arm64 host linux/amd64 runs
# under emulation -- correct, just slow.
#
# Results go to .validate-logs/<platform>/results.tsv, in the same format
# tsi-packages/scripts/merge-status.py consumes, so a local run and a CI run
# produce the same table.
#
# Disk: a full-catalogue run needs tens of gigabytes for source trees and build
# artifacts, per architecture, inside the named docker volumes. Check free space
# before --all, and `docker volume rm tsi-prefix-<arch> tsi-target-<arch>` after.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLATFORMS="linux/arm64,linux/amd64"
IMAGE="rust:1-trixie"
LOG_ROOT="$REPO_ROOT/.validate-logs"
ALL=false
FRESH=false
PKGS=()

while [ $# -gt 0 ]; do
  case "$1" in
    --platform) PLATFORMS="$2"; shift 2 ;;
    --all)      ALL=true; shift ;;
    --fresh)    FRESH=true; shift ;;
    --image)    IMAGE="$2"; shift 2 ;;
    -h|--help)  sed -n '2,16p' "$0"; exit 0 ;;
    *)          PKGS+=("$1"); shift ;;
  esac
done

if [ "$ALL" = false ] && [ ${#PKGS[@]} -eq 0 ]; then
  echo "Error: name at least one package to smoke-test." >&2
  echo "Full-catalogue runs belong in CI (validate-all-packages.yml)." >&2
  exit 1
fi

# --all needs tens of gigabytes per architecture and hours of build time. It
# filled this repo's development machine once. CI has both; a laptop usually
# does not, so make the caller say so out loud.
if [ "$ALL" = true ] && [ "${TSI_VALIDATE_ALL_I_MEAN_IT:-}" != "1" ]; then
  echo "Refusing --all: it needs tens of GB per architecture and hours to run," >&2
  echo "and CI already does it (validate-all-packages.yml, weekly)." >&2
  echo "Set TSI_VALIDATE_ALL_I_MEAN_IT=1 to override." >&2
  exit 1
fi

if ! docker info >/dev/null 2>&1; then
  echo "Error: docker daemon not reachable." >&2
  exit 1
fi

if [ ! -d "$REPO_ROOT/tsi-packages/packages" ]; then
  echo "Error: tsi-packages submodule not checked out. Run: git submodule update --init" >&2
  exit 1
fi

IFS=',' read -r -a PLATFORM_LIST <<< "$PLATFORMS"

for platform in "${PLATFORM_LIST[@]}"; do
  arch="${platform##*/}"
  out="$LOG_ROOT/$arch"
  # Start clean: a failure log left by an earlier run reads exactly like one
  # this run produced.
  rm -rf "$out"
  mkdir -p "$out"

  # A warm prefix makes a pass mean less than it looks: a package can build
  # only because something an earlier run installed happened to be there, which
  # is how nano passed against a broken ncurses. --fresh throws the prefix away
  # so the run stands on its own declared dependencies. The cargo target volume
  # is kept -- it changes build time, not the result.
  if [ "$FRESH" = true ]; then
    docker volume rm "tsi-prefix-$arch" >/dev/null 2>&1 || true
    echo "==> Discarded the $arch prefix; this run builds its dependencies itself"
  fi
  echo "==> Validating on $platform"

  # Named per-arch volumes so the cargo build and the tsi prefix survive
  # between runs; a cold run compiles tsi from scratch, a warm one doesn't.
  docker run --rm --platform "$platform" \
    -v "$REPO_ROOT":/src \
    -v "tsi-target-$arch":/target \
    -v "tsi-prefix-$arch":/root/.tsi \
    -v "$out":/out \
    -e CARGO_TARGET_DIR=/target \
    -e TSI_VALIDATE_ALL="$ALL" \
    -e TSI_VALIDATE_PKGS="${PKGS[*]:-}" \
    "$IMAGE" bash -euo pipefail -c '
      apt-get update -qq
      apt-get install -y -qq --no-install-recommends build-essential python3 pkg-config >/dev/null
      cargo build --release --manifest-path /src/Cargo.toml
      export PATH="/target/release:$PATH"
      tsi --version
      tsi update --local /src/tsi-packages/packages

      if [ "$TSI_VALIDATE_ALL" = "true" ]; then
        # Work from a copy so .build-logs never lands in the mounted tree.
        cp -r /src/tsi-packages /tmp/pkgs
        cd /tmp/pkgs
        bash scripts/build-all-packages.sh --exclude-slow || true
        cp .build-logs/results.tsv /out/results.tsv
        cp -r .build-logs/*.log /out/ 2>/dev/null || true
      else
        : > /out/results.tsv
        for pkg in $TSI_VALIDATE_PKGS; do
          f="/src/tsi-packages/packages/$pkg.json"
          if [ -f "$f" ] && ! python3 /src/tsi-packages/scripts/platform_id.py --supports "$f"; then
            plats=$(python3 /src/tsi-packages/scripts/platform_id.py --platforms "$f")
            echo "--- $pkg: unsupported here (${plats}-only)"
            printf "%s\tunsupported\t%s-only\n" "$pkg" "$plats" >> /out/results.tsv
            continue
          fi
          echo "--- installing $pkg"
          if tsi install "$pkg" > "/out/$pkg.log" 2>&1; then
            echo "    OK"
            rm -f "/out/$pkg.log"
            printf "%s\tok\t\n" "$pkg" >> /out/results.tsv
          else
            echo "    FAILED (see .validate-logs/'"$arch"'/$pkg.log)"
            tail -30 "/out/$pkg.log"
            printf "%s\tfail\t\n" "$pkg" >> /out/results.tsv
          fi
        done

        # The same gate CI applies: a package that installs but whose binaries
        # cannot load is not a pass. Reported, not fatal -- the per-package
        # results above are what this script exists to produce.
        bash /src/tsi-packages/scripts/check-linkage.sh /root/.tsi $TSI_VALIDATE_PKGS \
          || echo "WARNING: unresolved dynamic dependencies (see above)"
      fi
    '
  echo "==> $platform results: $out/results.tsv"
done

echo
echo "Summary"
for platform in "${PLATFORM_LIST[@]}"; do
  arch="${platform##*/}"
  printf '  %s:\n' "$platform"
  sed 's/^/    /' "$LOG_ROOT/$arch/results.tsv" 2>/dev/null || echo "    (no results)"
done

# Non-zero if anything failed on an architecture *this run* covered. Globbing
# $LOG_ROOT/*/ instead swept in results from previous runs of other platforms,
# so a clean single-platform smoke test reported failures it had not produced.
failed=false
for platform in "${PLATFORM_LIST[@]}"; do
  results="$LOG_ROOT/${platform##*/}/results.tsv"
  [ -f "$results" ] || continue
  if grep -q $'\tfail\t' "$results"; then
    failed=true
  fi
done

if [ "$failed" = true ]; then
  echo "FAILURES on at least one architecture." >&2
  exit 1
fi
echo "All validated packages install on every requested architecture."
