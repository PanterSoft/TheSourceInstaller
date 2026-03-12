# Documentation Deployment

The TSI documentation is built with MkDocs and deployed to GitHub Pages as part of the **Release** workflow.

## When Documentation Is Deployed

Documentation is built and deployed **only when a new version tag is pushed** (e.g. `v0.2.0`, `v1.0.0`). The same Release workflow that builds the TSI binaries also builds the docs and deploys them to GitHub Pages.

To update the live documentation:

1. Push a new tag: `git tag v0.2.0 && git push origin v0.2.0`
2. The Release workflow runs: builds binaries, builds docs, creates the GitHub Release, and deploys docs to Pages.

## GitHub Pages Setup

To enable GitHub Pages for this repository:

1. Go to **Settings** → **Pages**
2. Under **Source**, select **GitHub Actions**
3. The documentation will be available at:
   - `https://pantersoft.github.io/TheSourceInstaller/`

## Local Development

To build and test documentation locally:

```bash
# Create and activate virtual environment
python3 -m venv .venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate

# Install dependencies
pip install -r requirements-docs.txt

# Build documentation
mkdocs build

# Serve locally
mkdocs serve
```

The documentation will be available at `http://127.0.0.1:8000/`

## Troubleshooting

### Build Fails

- Check that all dependencies are installed: `pip install -r requirements-docs.txt`
- Verify `mkdocs.yml` syntax is correct
- Check for broken links: `mkdocs build --strict`

### Pages Not Updating

- Documentation deploys only on tag push. Ensure you pushed a tag and the Release workflow completed.
- Verify GitHub Pages is enabled in repository settings (Source: GitHub Actions).
- Check that the Release workflow completed successfully in the Actions tab.
