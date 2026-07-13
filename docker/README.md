# TSI Docker Testing Environment

Docker-based tests for TSI: from "does the binary even run on a bare system"
up to "does `tsi install <pkg>` actually build real software from source
with nothing but a C compiler and make."

**CI (`.github/workflows/docker-tests.yml`) is the source of truth.** The
scripts and compose services in this directory are a local convenience for
iterating before you push; they mirror CI but are not required by it.

## What CI does

`.github/workflows/docker-tests.yml` runs on every push/PR to `main`/`dev`
that touches `src/**`, `Cargo.*`, `docker/**`, `tsi-bootstrap.sh`, the
workflow file itself, or the `tsi-packages` submodule pointer (plus
`workflow_dispatch`). Three jobs, each matrixed over `amd64`
(`ubuntu-latest`) and `arm64` (`ubuntu-24.04-arm`):

1. **`build-binary`** -- builds a static release binary
   (`x86_64-unknown-linux-musl` / `aarch64-unknown-linux-musl`,
   `LZMA_API_STATIC=1`), asserts it's actually statically linked (`ldd`
   reports "not a dynamic executable"), and uploads it as
   `tsi-static-amd64` / `tsi-static-arm64`.
2. **`container-tests`** -- for each of `alpine:latest`,
   `debian:stable-slim`, `ubuntu:24.04`, `fedora:latest` (8 legs total:
   4 distros x 2 arches), downloads the matching static binary and runs
   [`docker/e2e-test.sh`](e2e-test.sh) inside a bare container. The script
   installs *only* a C compiler and `make` via the distro's own package
   manager, then drives a full `tsi update` -> `tsi install bzip2` (a real
   source build) -> `tsi list` -> `tsi info` -> `tsi uninstall` cycle,
   asserting the built artifact appears and disappears on disk. This is
   the real end-to-end coverage the old `test-install.sh` never had: it
   built `tsi` with `cargo` inside the container but never ran a real
   `tsi install`.
3. **`no-tools-test`** -- runs the static binary in bare `alpine` and
   `debian` containers with **nothing** installed (no compiler, no
   package definitions). Proves `tsi --version` / `--help` / `doctor`
   work with zero runtime dependencies, and that `tsi install zlib`
   fails gracefully (non-zero exit, no Rust panic) rather than crashing.

Every matrix leg (`fail-fast: false` throughout) can fail independently
without blocking the others.

### Why bzip2, not zlib, for the real build

`zlib.json`'s `build_commands` run an inline `python3` heredoc to patch
`zutil.h`, but `python3` is not declared in zlib's `build_dependencies` and
is never installed by `e2e-test.sh` (which installs only cc + make, per
the whole point of this test). `bzip2.json` has `build_dependencies: []`
and `build_system: "make"` -- a bare cc+make toolchain is genuinely
sufficient to build it, and it's literally one of TSI's own
`BOOTSTRAP_PACKAGES` (`src/core/bootstrap.rs`), making it a representative
real package rather than a toy example. `no-tools-test` still uses `zlib`
for its graceful-failure check, since that path never reaches the build
step (it fails earlier, at "no package definitions found").

## Local Quick Start

### Minimal / no-tools scenarios (fast, no binary needed)

```bash
cd docker
./run-tests.sh
```

Builds and runs `alpine-minimal`, `alpine-c-only`, and `ubuntu-minimal`
via `test-install.sh` (which builds `tsi` with `cargo` inside the
container -- these are the legacy "does it build and run basic commands"
scenarios, not the CI e2e flow).

### Four-distro e2e scenarios (mirrors CI, needs a local binary)

```bash
cargo build --release          # from the repo root
cd docker
./run-tests.sh                 # now also runs the e2e-* services
# or run one directly:
docker compose run --rm e2e-alpine
```

`run-tests.sh` skips the e2e services with a warning if
`target/release/tsi` doesn't exist yet. These `docker-compose.yml`
services (`e2e-alpine`, `e2e-debian`, `e2e-ubuntu`, `e2e-fedora`) mount
the repo read-only at `/work`, set `TSI_BIN=/work/target/release/tsi`,
and run `docker/e2e-test.sh` -- the exact same script CI runs, just
against a locally built binary instead of a downloaded artifact.

### Test Individual Scenarios

```bash
cd docker
docker compose build alpine-c-only
docker compose run --rm alpine-c-only /bin/sh /root/tsi-source/docker/test-install.sh

# Or enter the container interactively
docker compose run --rm alpine-c-only /bin/sh
```

## Scripts

- **`e2e-test.sh`** (POSIX sh) -- the real end-to-end install test. Takes
  the tsi binary path as `$1` or `$TSI_BIN`. Installs cc+make via
  whichever of `apk`/`apt-get`/`dnf` it finds, then
  `update` -> `install bzip2` -> verify artifact -> `list` -> `info` ->
  `uninstall` -> verify removed. Prints a ✓/✗ line per step and exits
  non-zero (fail-fast) on the first failure.
- **`no-tools-test.sh`** (POSIX sh) -- zero-dependency proof. Takes the
  tsi binary path as `$1`. No package manager step at all: checks
  `--version`/`--help`/`doctor` work unaided, and that
  `tsi install zlib` fails cleanly (non-zero exit, no panic) with no
  toolchain or package registry present.
- **`test-install.sh`** (POSIX sh) -- legacy in-container `cargo build`
  and basic CLI smoke test, used by the `alpine-minimal` /
  `alpine-c-only` / `ubuntu-minimal` scenarios. Package definitions are
  read from `tsi-packages/packages` (the submodule), not a top-level
  `packages/` directory.
- **`run-tests.sh`** (bash) -- local convenience runner; may use bash
  features (unlike the scripts above, which run *inside* containers and
  must stay POSIX sh).

## Container / Compose Details

### Minimal containers (`alpine-minimal`, `ubuntu-minimal`)

Package managers removed after build to simulate a truly minimal system:
no C compiler, no build tools. Expected to fail gracefully.

### `alpine-c-only`

gcc, make, musl-dev, and a Rust toolchain (for building `tsi` itself with
`cargo` inside the container).

### `e2e-alpine` / `e2e-debian` / `e2e-ubuntu` / `e2e-fedora`

Bare upstream images (`alpine:latest`, `debian:stable-slim`,
`ubuntu:24.04`, `fedora:latest`) with no custom Dockerfile -- exactly what
CI's `container-tests` job uses via `docker run` directly. `e2e-test.sh`
installs cc+make itself at runtime.

## Continuous Integration

- **`.github/workflows/docker-tests.yml`** -- the workflow described
  above; this is the one that matters.
- **`.github/workflows/test.yml`** / **`rust-ci.yml`** -- plain
  `cargo build`/`test`/`clippy`/`fmt` across platforms, unrelated to
  Docker.

## Troubleshooting

### Container build fails

```bash
docker compose down
docker compose build --no-cache
```

### Test fails

```bash
cat /tmp/tsi-test-<scenario>.log
```

### Permission issues

```bash
chmod +x docker/run-tests.sh docker/test-install.sh docker/e2e-test.sh docker/no-tools-test.sh
```

### `e2e-*` services skipped / fail to find the binary

Run `cargo build --release` from the repo root first -- these services
reuse `target/release/tsi` rather than building `tsi` inside the
container.

## Adding a New Distro to the E2E Matrix

1. Add the image to the `container-tests` and (optionally) `no-tools-test`
   matrices in `.github/workflows/docker-tests.yml`.
2. Add a matching `e2e-<name>` service to `docker-compose.yml` (image +
   `TSI_BIN=/work/target/release/tsi` + `/work` volume + the same
   `e2e-test.sh` command -- no new script needed, `e2e-test.sh` already
   detects `apk`/`apt-get`/`dnf`).
3. Add it to `E2E_SERVICES` in `run-tests.sh` if you want it covered
   locally too.
