# Workflow Trigger Configuration

This document explains when each workflow runs and what triggers them.

## TSI Tests Workflow

**File:** `.github/workflows/test.yml`

**Purpose:** Tests the TSI source code (C implementation, builds, linting)

**Triggers:**
- ✅ **Only runs when TSI source code changes:**
  - `src/**` - Source code files
  - `docker/**` - Docker test configurations
  - `scripts/**` - Utility scripts
  - `Makefile` - Build configuration
  - `tsi-bootstrap.sh` - Bootstrap script
  - `completions/**` - Shell completion scripts
  - `.github/workflows/test.yml` - The workflow file itself

- ❌ **Does NOT run when:**
  - Package files change (`packages/**`)
  - Documentation changes (`docs/**`, `README.md`)
  - Only package versions are updated

**Jobs:**
- `test-c`: Tests C/C++ version build and functionality
- `build-c`: Builds TSI for multiple architectures
- `lint`: Lints C code
- `test-all`: Runs full test suite

**Manual Trigger:** Yes, can be triggered manually via `workflow_dispatch`

## Validate Packages Workflow

**File:** `.github/workflows/validate-packages.yml`

**Purpose:** Validates package JSON files and ensures TSI can parse them

**Triggers:**
- ✅ **Only runs when package files change:**
  - `packages/**/*.json` - Package definition files
  - `.github/workflows/validate-packages.yml` - The workflow file itself

- ❌ **Does NOT run when:**
  - TSI source code changes
  - Documentation changes
  - Other workflow files change

**Jobs:**
- `validate-format`: Validates JSON syntax and structure
- `validate-tsi-parsing`: Tests that TSI can parse all packages
- `validate-dependencies`: Validates package dependencies
- `test-package-install`: Tests package installation in Docker

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

| Workflow | Triggers on Source Code | Triggers on Packages | Scheduled |
|----------|-------------------------|---------------------|-----------|
| TSI Tests | ✅ Yes | ❌ No | ❌ No |
| Validate Packages | ❌ No | ✅ Yes | ❌ No |
| Discover Versions | ❌ No | ❌ No | ✅ Weekly |
| Sync External | ❌ No | ❌ No | ❌ No |

## Benefits

1. **Faster CI/CD**: Tests only run when relevant code changes
2. **Reduced costs**: Fewer unnecessary workflow runs
3. **Clear separation**: Source code tests vs package validation
4. **Better feedback**: Developers get faster feedback on their changes

## Testing the Configuration

### Test 1: Source Code Change

```bash
# Make a change to source code
echo "// test" >> src/main.c
git commit -am "test: source code change"
git push
```

**Expected:** TSI Tests workflow runs, Validate Packages does NOT run

### Test 2: Package File Change

```bash
# Make a change to a package file
echo '{"test": true}' >> packages/test.json
git commit -am "test: package change"
git push
```

**Expected:** Validate Packages workflow runs, TSI Tests does NOT run

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

