# Workflow Trigger Configuration

This document explains when each workflow runs and what triggers them.

## TSI Tests Workflow

**File:** `.github/workflows/test.yml`

**Purpose:** Tests the TSI source code (Rust implementation, builds, linting)

**Triggers:**
- ✅ **Runs when:**
  - `src/**` - Source code files
  - `Cargo.toml` - Build configuration
  - `packages/**` - Package files
  - `.github/workflows/test.yml` - The workflow file itself

- ❌ **Does NOT run when:**
  - Documentation changes (`docs/**`, `README.md`)
  - Only other workflow files change

**Jobs:**
- `test`: Calls the reusable [Rust CI](.github/workflows/rust-ci.yml) workflow (matrix: ubuntu-latest, macos-latest, windows-latest); runs build, test, clippy, fmt with Cargo caching.

**Manual Trigger:** Yes, can be triggered manually via `workflow_dispatch`

## Documentation Workflow

**File:** `.github/workflows/docs.yml`

**Purpose:** Builds MkDocs documentation so doc issues are caught on PRs before release.

**Triggers:**
- ✅ **Runs when:**
  - `docs/**` - Documentation source
  - `mkdocs.yml` - MkDocs config
  - `requirements-docs.txt` - Doc dependencies
  - `.github/workflows/docs.yml` - The workflow file itself

**Jobs:**
- `build`: Sets up Python, installs doc dependencies, runs `mkdocs build --strict`.

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

## Release Workflow

**File:** `.github/workflows/release.yml`

**Purpose:** Builds release binaries and documentation, creates the GitHub Release, and deploys docs to GitHub Pages

**Triggers:**
- **Tag push:** When a tag matching `v*` is pushed (e.g. `v0.2.0`, `v1.0.0`)

**Jobs:**
- `build`: Builds TSI binaries for all platforms (linux, macos, windows; x86_64 and aarch64)
- `docs`: Builds MkDocs documentation and uploads the site artifact
- `release`: Creates the GitHub Release with the binary artifacts and generated release notes
- `deploy-docs`: Deploys the built documentation to GitHub Pages

**Note:** There is no manual trigger. To cut a release (binaries + docs), push a tag.

## Summary

| Workflow | Triggers on Source Code | Triggers on Packages | Triggers on Docs | Triggers on Tag | Scheduled |
|----------|-------------------------|---------------------|------------------|-----------------|-----------|
| TSI Tests | ✅ Yes | ✅ Yes | ❌ No | ❌ No | ❌ No |
| Documentation | ❌ No | ❌ No | ✅ Yes | ❌ No | ❌ No |
| Package Validation | ❌ No | ✅ Yes | ❌ No | ❌ No | ❌ No |
| Release (binaries + docs) | ❌ No | ❌ No | ❌ No | ✅ Yes | ❌ No |
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

