# Workflow Trigger Configuration

This document explains when each workflow runs and what triggers them.

## CI and Release Workflow

**File:** `.github/workflows/ci.yml`

**Purpose:** The single pipeline for source changes: lint, build, end-to-end tests, and release.

**Triggers:**
- Push to `main`/`dev` or a pull request touching `src/**`, `Cargo.toml`, `Cargo.lock`, `build.rs`, `tests/**`, `*.sh`, `docker/**`, `tsi-packages` or `.github/workflows/**`
- **Manual:** `workflow_dispatch` with `bump` = `none` (just run CI) or `major` (release, see below)

**Stages:**
1. **Lint:** clippy and fmt (Linux, macOS, Windows), actionlint, shellcheck
2. **Build:** every release platform via the reusable `build-binaries.yml` (Linux x86_64, aarch64, i686, armv7, armv6, riscv64, ppc64le; macOS and Windows on x86_64 and aarch64), with tests and smoke tests
3. **E2E:** real installs in distro containers via the reusable `e2e.yml`, using the stage-2 binaries
4. **Release:** `main` only, after everything above is green. Tags and publishes the binaries that were just built.

**Versioning:** every push to `main` releases the next **minor** version (`v0.2.2` becomes `v0.3.0`). A **major** release is manual: run the CI workflow on `main` with `bump = major`. The latest `v*` git tag is the source of truth; the build overrides the version in `Cargo.toml` with the new one. Pull requests and `dev` never release. Runs on `main` queue rather than cancel each other.

## Documentation Workflow

**File:** `.github/workflows/docs.yml`

**Purpose:** Builds MkDocs documentation so doc issues are caught on PRs, and deploys it to GitHub Pages from `main`.

**Triggers:**
- ✅ **Runs when:**
  - `docs/**` - Documentation source
  - `mkdocs.yml` - MkDocs config
  - `requirements-docs.txt` - Doc dependencies
  - `.github/workflows/docs.yml` - The workflow file itself

**Jobs:**
- `build`: Sets up Python, installs doc dependencies, runs `mkdocs build --strict`; on `main` it also uploads the site as the Pages artifact.
- `deploy`: On pushes and manual runs on `main` only, deploys the site to GitHub Pages.

**Manual Trigger:** Yes, via `workflow_dispatch`

## Package Validation Workflow

**File:** `.github/workflows/Package Validation.yml`

**Purpose:** Validates package JSON files and ensures TSI can parse them

**Triggers:**
- ✅ **Only runs when package files change:**
  - `packages/**/*.json` - Package definition files
  - `.github/workflows/Package Validation.yml` - The workflow file itself

- ❌ **Does NOT run when:**
  - TSI source code changes
  - Documentation changes
  - Other workflow files change

**Jobs:**
- `validate-format`: Validates JSON syntax and structure
- `validate-tsi-parsing`: Tests that TSI can parse all packages
- `validate-dependencies`: Validates package dependencies
- `test-package-install`: Smoke tests TSI commands (info, list, search, doctor, install)

**Manual Trigger:** Yes, can be triggered manually via `workflow_dispatch`

## Discover Versions Workflow

**File:** `.github/workflows/discover-versions.yml`

**Purpose:** Automatically discovers and updates package versions

**Triggers:**
- **Scheduled:** Weekly on Mondays at 00:00 UTC
- **Manual:** Via `workflow_dispatch`

**Note:** This workflow doesn't use path filters because it needs to read all package files to discover versions.

## Sync External Packages Workflow

**File:** `.github/workflows/sync-external-packages.yml`

**Purpose:** Syncs package definitions from external repositories

**Triggers:**
- **Manual:** Via `workflow_dispatch`
- **Webhook:** Via `repository_dispatch` (for external triggers)

## Summary

| Workflow | Triggers on Source Code | Triggers on Packages | Triggers on Docs | Triggers on Tag | Scheduled |
|----------|-------------------------|---------------------|------------------|-----------------|-----------|
| CI + Release | ✅ Yes | ❌ No | ❌ No | ❌ No | ❌ No |
| Documentation | ❌ No | ❌ No | ✅ Yes | ❌ No | ❌ No |
| Package Validation | ❌ No | ✅ Yes | ❌ No | ❌ No | ❌ No |
| Discover Versions | ❌ No | ❌ No | ❌ No | ❌ No | ✅ Weekly |
| Sync External | ❌ No | ❌ No | ❌ No | ❌ No | ❌ No |

## Benefits

1. **Faster CI/CD**: Tests only run when relevant code changes
2. **Reduced costs**: Fewer unnecessary workflow runs
3. **Clear separation**: Source code tests vs package validation
4. **Better feedback**: Developers get faster feedback on their changes

## Testing the Configuration

### Test 1: Source Code Change

```bash
# Make a change to source code
echo "// test" >> src/main.rs
git commit -am "test: source code change"
git push
```

**Expected:** TSI Tests workflow runs, Package Validation does NOT run

### Test 2: Package File Change

```bash
# Make a change to a package file
echo '{"test": true}' >> packages/test.json
git commit -am "test: package change"
git push
```

**Expected:** Both TSI Tests and Package Validation workflows run (both trigger on `packages/**`)

### Test 3: Documentation Change

```bash
# Make a change to documentation
echo "# test" >> README.md
git commit -am "test: documentation change"
git push
```

**Expected:** Neither workflow runs (unless workflow files themselves changed)

## Manual Override

Both workflows support `workflow_dispatch` for manual triggering when needed:

1. Go to **Actions** tab
2. Select the workflow
3. Click **Run workflow**
4. Choose branch and click **Run workflow**

This is useful for:
- Testing after fixing issues
- Running tests on demand
- Debugging workflow issues

## See Also

- [Workflow Automation](automation.md)
- [Version Discovery](version-discovery.md)
- [Trigger Workflow](trigger-workflow.md)

