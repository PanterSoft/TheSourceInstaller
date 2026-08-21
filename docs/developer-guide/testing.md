# Testing

```bash
make test          # cargo test
make lint          # clippy
make fmt           # rustfmt --check
```

The whole suite runs offline in under a second — no network, no compiler toolchain, no
writes outside a temp dir. Anything that would need a real source build belongs in the
Docker e2e matrix instead (see below).

## Layout

| File | Covers |
|------|--------|
| `tests/cli_help.rs` | `--help`/`--version` and every subcommand's help |
| `tests/cli_scenarios.rs` | End-to-end binary runs against a throwaway `--prefix` |
| `tests/database.rs` | Installed-package database: persistence, reverse deps, corruption |
| `tests/registry.rs` | Loading definitions, version selection, malformed files, search |
| `tests/resolver.rs` | Dependency resolution and topological build order |
| `tests/package_parse.rs` | Package JSON parsing and per-OS/arch overrides |
| `tests/platform.rs` | Prefix resolution and platform naming |
| `src/**/mod tests` | Unit tests next to the code (see below) |

Unit tests inside `src/` cover the parts with no public API to drive from a test binary:

- `src/ops/fetch.rs` — archive extraction for every supported compression, magic-byte
  fallback for misnamed downloads, and the zip loop's refusal to write outside the
  destination. Archives are built in-memory, so nothing is downloaded.
- `src/cli/ui/` — filter/navigation/batch logic, plus render smoke tests that draw every
  tab and overlay into a `TestBackend` at sizes down to 1×1. Layout math that underflows
  panics inside `draw`, so a successful render *is* the assertion.
- `src/util/sha256.rs`, `src/core/registry.rs` — hashing and version comparison.

`tests/fixtures/packages/` holds a tiny three-package registry (`curl` → `zlib`,
`openssl`) used where a realistic definition is needed.

## Writing a test

**Pure logic** (resolver, registry, parsing) — build definitions inline as JSON strings in
a `tempfile::tempdir()` and load a `Registry` from it. `registry_of()` in
`tests/resolver.rs` and `registry_from()` in `tests/registry.rs` do exactly that; copy the
shape rather than reaching for a fixture file, so the test's input is readable next to its
assertion.

**A whole command** — add to `tests/cli_scenarios.rs`. It runs the real binary via
`env!("CARGO_BIN_EXE_tsi")` with `--prefix <tempdir>`, so every filesystem effect lands in
the temp dir. `seed_db()` writes a `db/installed.json` directly, which is how you get to a
"packages already installed" state without building anything.

Two things to know when asserting on output:

- All human-facing output goes to **stderr**; only `--json` output goes to stdout. Use the
  `combined()` helper unless you are specifically testing machine-readable output.
- Commands that refuse to act exit non-zero (e.g. a blocked `uninstall`). Assert on the
  exit status as well as the message.

## System scenarios

`tests/cli_scenarios.rs` deliberately exercises the states a real machine lands in:

- fresh, empty prefix
- prefix with no package definitions (must point the user at `tsi update`)
- prefix populated from a local definitions directory (`tsi update --local`)
- database with dependency relationships (uninstall guard)
- a concurrently held install lock

When you fix a bug that only shows up in one of those states, add the state here.

## Docker e2e

Real source builds across distros live in `docker/` and run in the `docker-tests`
workflow:

```bash
cd docker && ./run-tests.sh
```

That matrix (Alpine/Ubuntu, minimal and no-tools images, x86_64 and arm64) is where
toolchain-dependent behavior is verified. Keep it for things that genuinely need a
compiler; everything else should be a `cargo test`.

## Cross-architecture package validation

"It builds on my machine" is not a package passing. A package definition must install on
every architecture it claims to support, so validate it on more than the host you wrote it
on:

```bash
make validate PKGS="zlib bzip2"   # linux/arm64 + linux/amd64
make validate                     # the whole catalogue (hours)
./docker/validate-packages.sh --platform linux/amd64 rsync
```

The script builds `tsi` inside a container per platform and runs `tsi install` for each
package. On an Apple Silicon host `linux/amd64` runs under emulation — correct, just slow.
Results land in `.validate-logs/<arch>/results.tsv` in the same format CI produces, and
failing build logs stay next to them.

A package that genuinely cannot build somewhere declares it with `platforms` (see
[OS-specific configuration](os-specific-config.md#restricting-a-package-to-some-platforms))
instead of quietly failing there.

In the tsi-packages repository, `test-build-packages.yml` builds every changed package on
Linux-x86_64, Linux-aarch64 and macOS-aarch64, and `validate-all-packages.yml` rebuilds the
whole catalogue weekly on all three and regenerates `PACKAGES_STATUS.md` from the results.

## CI

`rust-ci.yml` runs fmt, clippy and the test suite on push and PR; `docker-tests.yml` runs
the e2e matrix. A change is expected to be green on both before merge.
