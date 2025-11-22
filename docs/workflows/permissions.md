# GitHub Actions Workflow Permissions

## Issue: "GitHub Actions is not permitted to create or approve pull requests"

If you see this error, it means the repository settings need to be configured to allow GitHub Actions to create pull requests.

## Solution

### Step 1: Check Repository Settings

1. Go to your repository on GitHub
2. Click **Settings** (top menu)
3. Go to **Actions** → **General**
4. Scroll down to **Workflow permissions**
5. Select one of these options:

   **Option A: Read and write permissions (Recommended)**
   - Select: "Read and write permissions"
   - ✅ Check: "Allow GitHub Actions to create and approve pull requests"
   - Click **Save**

   **Option B: Read repository contents and packages permissions**
   - Select: "Read repository contents and packages permissions"
   - ✅ Check: "Allow GitHub Actions to create and approve pull requests"
   - Click **Save**

### Step 2: Verify Workflow Permissions

The workflow file should have these permissions:

```yaml
permissions:
  contents: write
  pull-requests: write
  issues: write
```

This is already configured in `.github/workflows/discover-versions.yml`.

### Step 3: Verify GITHUB_TOKEN

The workflow uses `${{ secrets.GITHUB_TOKEN }}` which is automatically provided by GitHub Actions. This token has the permissions specified in the workflow file.

## Alternative: Use Personal Access Token

If repository settings can't be changed, you can use a Personal Access Token (PAT):

1. **Create a PAT**:
   - Go to GitHub Settings → Developer settings → Personal access tokens → Tokens (classic)
   - Click "Generate new token (classic)"
   - Give it a name (e.g., "TSI Workflow PR")
   - Select scopes:
     - `repo` (full control of private repositories)
     - `workflow` (update GitHub Action workflows)
   - Click "Generate token"
   - **Copy the token** (you won't see it again!)

2. **Add as Secret**:
   - Go to repository → Settings → Secrets and variables → Actions
   - Click "New repository secret"
   - Name: `WORKFLOW_TOKEN`
   - Value: Paste your PAT
   - Click "Add secret"

3. **Update Workflow**:
   Change the workflow to use the PAT:
   ```yaml
   - name: Create Pull Request
     uses: peter-evans/create-pull-request@v6
     with:
       token: ${{ secrets.WORKFLOW_TOKEN }}  # Changed from GITHUB_TOKEN
   ```

## Verification

After making these changes:

1. **Re-run the workflow**:
   - Go to Actions tab
   - Find the failed workflow run
   - Click "Re-run all jobs"

2. **Check the logs**:
   - The workflow should now successfully create pull requests
   - Look for "Created pull request" in the logs

## Troubleshooting

### Still Getting Permission Errors?

1. **Check if you're the repository owner/admin**:
   - Only owners and admins can change workflow permissions
   - If you're not, ask a repository admin to make the change

2. **Check organization settings** (if repository is in an organization):
   - Organization settings may override repository settings
   - Go to Organization → Settings → Actions → General
   - Check workflow permissions settings

3. **Verify the workflow file**:
   - Make sure permissions are correctly set
   - Check that `pull-requests: write` is included

4. **Check branch protection rules**:
   - Some branches may have protection rules that prevent PR creation
   - The workflow creates PRs to `main` by default
   - Ensure the target branch allows PRs from workflows

## See Also

- [GitHub Actions Permissions](https://docs.github.com/en/actions/security-guides/automatic-token-authentication#permissions-for-the-github_token)
- [Workflow Automation](WORKFLOW-AUTOMATION.md)
- [Trigger Workflow](TRIGGER-WORKFLOW.md)

