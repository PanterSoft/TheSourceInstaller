# How to Trigger the Version Discovery Workflow

## Quick Start

### Option 1: Via GitHub Web Interface

1. **Navigate to Actions**:
   - Go to your repository on GitHub
   - Click on the **Actions** tab

2. **Select the Workflow**:
   - In the left sidebar, click on **"Discover Package Versions"**

3. **Run the Workflow**:
   - Click the **"Run workflow"** dropdown button (top right)
   - Configure options:
     - **Package**: Leave empty to check ALL packages, or enter a specific package name (e.g., `curl`)
     - **Max versions**: Leave empty to discover ALL versions, or enter a number to limit (e.g., `10`)
   - Click **"Run workflow"**

4. **Monitor the Run**:
   - The workflow will start running
   - Click on the run to see logs in real-time
   - If new versions are found, a pull request will be created automatically

### Option 2: Via GitHub CLI

```bash
# Trigger workflow to check all packages (all versions)
gh workflow run discover-versions.yml

# Trigger workflow for a specific package
gh workflow run discover-versions.yml -f package=curl

# Trigger workflow with version limit
gh workflow run discover-versions.yml -f max_versions=10

# Trigger workflow for specific package with limit
gh workflow run discover-versions.yml -f package=curl -f max_versions=5
```

### Option 3: Via API

```bash
# Get workflow ID first
WORKFLOW_ID=$(gh api repos/:owner/:repo/actions/workflows/discover-versions.yml | jq -r '.id')

# Trigger the workflow
gh api repos/:owner/:repo/actions/workflows/$WORKFLOW_ID/dispatches \
  -X POST \
  -f ref=main \
  -f inputs='{}'

# With inputs
gh api repos/:owner/:repo/actions/workflows/$WORKFLOW_ID/dispatches \
  -X POST \
  -f ref=main \
  -f inputs='{"package":"curl","max_versions":"10"}'
```

## Testing Locally

Before triggering the workflow, you can test the script locally:

```bash
# Test single package (dry run)
python3 scripts/discover-versions.py curl --dry-run --max-versions 5

# Test all packages (dry run, limited versions for speed)
python3 scripts/discover-versions.py --all --dry-run --max-versions 3

# Run the test script
./scripts/test-discovery.sh
```

## What Happens When the Workflow Runs

1. **Checkout**: Repository is checked out
2. **Setup**: Python environment is set up
3. **Discovery**: Script discovers versions for all packages (or specified package)
4. **Update**: Package JSON files are updated with new versions
5. **Check**: Changes are detected
6. **PR Creation**: If changes found, a pull request is created automatically

## Monitoring Workflow Runs

### View Logs

1. Go to **Actions** tab
2. Click on **"Discover Package Versions"** workflow
3. Click on a specific run to see detailed logs

### Check for Errors

Common issues to watch for:

- **Rate Limiting**: If you see "rate limit exceeded", the workflow will retry or you can wait
- **Network Errors**: Temporary network issues - workflow will show in logs
- **Package Errors**: Some packages may fail to discover versions (logged as warnings)

### Success Indicators

✅ **Successful run**:
- Workflow completes with green checkmark
- Log shows "New versions discovered and package files updated!"
- Pull request is created (if changes found)

⚠️ **No changes**:
- Workflow completes successfully
- Log shows "No new versions discovered - all packages are up to date"
- No PR created (this is normal)

## Scheduled Runs

The workflow runs automatically:
- **Schedule**: Every Monday at 00:00 UTC
- **Action**: Checks all packages and discovers all versions
- **Result**: Creates PR if new versions are found

You don't need to do anything - it runs automatically!

## Troubleshooting

### Workflow Not Appearing

- Check that the workflow file is in `.github/workflows/discover-versions.yml`
- Verify the file is committed to the repository
- Check repository Actions settings (must be enabled)

### Workflow Fails

1. **Check logs**: Click on the failed run to see error messages
2. **Common issues**:
   - Syntax errors in workflow file
   - Missing dependencies
   - GitHub API rate limits (should be handled automatically with token)
3. **Fix and retry**: Make corrections and trigger again

### No Versions Discovered

- Some packages may not have discoverable versions (non-GitHub sources)
- Check package source URL format
- Verify GitHub repository exists and is accessible
- Check workflow logs for specific error messages

## See Also

- [Version Discovery Documentation](VERSION-DISCOVERY.md)
- [Workflow Automation](WORKFLOW-AUTOMATION.md)
- [Scripts README](../scripts/README.md)

