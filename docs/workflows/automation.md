# Workflow Automation

TSI includes automated workflows that keep package definitions up-to-date automatically.

## Available Workflows

### 1. Discover Package Versions

**File:** `.github/workflows/discover-versions.yml`

Automatically discovers new package versions and updates package configuration files.

#### Features

- ✅ **Automatic Discovery**: Finds new versions from GitHub, git repos, and other sources
- ✅ **All Packages**: Automatically checks ALL packages in the repository
- ✅ **All Versions**: Discovers ALL available versions for each package (not limited)
- ✅ **File Updates**: Automatically updates package JSON files with new versions
- ✅ **Multi-Version Support**: Adds versions to the `versions` array while preserving existing ones
- ✅ **Pull Request Creation**: Creates PRs when new versions are found
- ✅ **Scheduled Runs**: Runs weekly on Mondays at 00:00 UTC
- ✅ **Manual Trigger**: Can be triggered manually for specific packages
- ✅ **Pagination Support**: Handles GitHub API pagination to fetch all versions

#### How It Works

1. **Discovery Phase**:
   - Reads package definitions from `packages/` directory
   - Extracts source information (GitHub repo, URLs, etc.)
   - Queries GitHub API or other sources for available versions
   - Filters out versions that already exist

2. **Update Phase**:
   - Generates new version definitions based on the latest existing version
   - Updates version numbers in URLs and metadata
   - Adds new versions to package files
   - Converts single-version packages to multi-version format if needed

3. **PR Creation**:
   - Detects changes in package files
   - Creates a summary of updated packages
   - Opens a pull request with all changes
   - Includes detailed information about what was updated

#### Usage

##### Scheduled (Automatic)

The workflow runs automatically every Monday at 00:00 UTC. No action required.

##### Manual Trigger

1. Go to **Actions** → **Discover Package Versions**
2. Click **Run workflow**
3. Configure options:
   - **Package**: Leave empty for all packages, or specify a package name
   - **Max versions**: (Optional) Maximum versions to discover per package. Leave empty to discover ALL versions
4. Click **Run workflow**

**Note:** By default, the workflow discovers **ALL available versions** for each package. The scheduled run always checks all packages and discovers all versions.

#### Example Output

When the workflow runs, it will:

```
📦 New versions discovered and package files updated!

Changed files:
packages/curl.json
packages/git.json

Summary of changes:
packages/curl.json | 9 +++++++++
packages/git.json  | 3 +++
```

The PR will include:
- List of updated packages
- Number of versions added
- Summary of changes
- Review checklist

### 2. Sync External Packages

**File:** `.github/workflows/sync-external-packages.yml`

Syncs package definitions from external repositories that include `.tsi.json` files.

See [External Package Configuration](external-packages.md) for details.

## Workflow Configuration

### Permissions

Both workflows require:
- `contents: write` - To update package files
- `pull-requests: write` - To create pull requests

These are automatically granted via `GITHUB_TOKEN`.

### Branch Strategy

- Workflows create branches with unique names (e.g., `auto-update-versions-123456`)
- Branches are automatically deleted after PR merge
- PRs target the `main` branch

### Rate Limiting

GitHub API has rate limits:
- **Authenticated requests**: 5,000 requests/hour
- **Unauthenticated requests**: 60 requests/hour

The workflows use `GITHUB_TOKEN` which provides authenticated rate limits. For large-scale updates, consider:
- Running workflows less frequently
- Processing packages in batches
- Using `--max-versions` to limit discovery per package

## Monitoring

### Workflow Status

Check workflow status:
1. Go to **Actions** tab
2. View recent runs
3. Click on a run to see details

### Success Indicators

✅ **Successful run**:
- Workflow completes without errors
- Package files are updated (if new versions found)
- Pull request is created (if changes detected)

⚠️ **No changes**:
- Workflow completes successfully
- Message: "No new versions discovered - all packages are up to date"
- No PR created

❌ **Failed run**:
- Check workflow logs for errors
- Common issues:
  - Network timeouts
  - Invalid package definitions
  - GitHub API rate limits

## Best Practices

### Package Definitions

For best results with automatic version discovery:

1. **Use semantic versioning** in URLs
2. **Include version in source URL** (e.g., `package-1.2.3.tar.gz`)
3. **Use consistent URL patterns** across versions
4. **Host on GitHub** when possible (best discovery support)

### Review Process

When reviewing auto-generated PRs:

1. ✅ Verify version numbers are correct
2. ✅ Check that source URLs are valid
3. ✅ Ensure dependencies are still accurate
4. ✅ Test that packages can be built with new versions
5. ✅ Verify URL patterns match expected format

### Manual Intervention

Sometimes manual updates are needed:

- **Non-standard version formats**: May require custom discovery logic
- **URL pattern changes**: If package maintainers change URL structure
- **Dependency updates**: New versions may need different dependencies
- **Build system changes**: New versions may use different build systems

## Troubleshooting

### Workflow Not Running

**Scheduled runs not executing:**
- Check repository settings → Actions → Workflow permissions
- Verify cron schedule is correct
- Check GitHub Actions status page for outages

**Manual trigger not working:**
- Ensure you have write access to the repository
- Check workflow file syntax
- Verify workflow is in `.github/workflows/` directory

### Discovery Not Finding Versions

**GitHub packages:**
- Verify repository is public or token has access
- Check that releases/tags exist
- Verify URL format matches GitHub patterns

**Other sources:**
- May require custom discovery logic
- Check if website structure has changed
- Verify network connectivity

### URL Replacement Issues

**URLs not updating correctly:**
- Ensure version is in the URL
- Check version format matches pattern
- Verify base version template is correct

**Multiple version patterns:**
- The system tries multiple patterns
- If issues persist, may need custom logic

## Integration

### With External Packages

Both workflows work together:

1. **External packages**: Projects maintain their own `.tsi.json` files
2. **Version discovery**: Automatically finds and adds new versions
3. **Both systems**: Keep packages up-to-date through different mechanisms

### With CI/CD

The workflows integrate with your CI/CD pipeline:

- PRs trigger validation workflows
- Package tests run on new versions
- Automated checks ensure quality

## See Also

- [Version Discovery](version-discovery.md) - Detailed version discovery documentation
- [External Package Configuration](external-packages.md) - External package sync workflow
- [Package Repository](../reference/package-repository.md) - Package format documentation

