# Documentation Deployment

The TSI documentation is built with MkDocs and automatically deployed to GitHub Pages.

## Automatic Deployment

The documentation is automatically built and deployed when:
- Changes are pushed to the `main` branch in:
  - `docs/` directory
  - `mkdocs.yml`
  - `requirements-docs.txt`
  - `.github/workflows/docs.yml`

## GitHub Pages Setup

To enable GitHub Pages for this repository:

1. Go to **Settings** → **Pages**
2. Under **Source**, select:
   - **Source**: `GitHub Actions`
3. The documentation will be available at:
   - `https://pantersoft.github.io/tsi/`

## Manual Deployment

You can also manually trigger the deployment workflow:

1. Go to **Actions** → **Build and Deploy Documentation**
2. Click **Run workflow**
3. Select the branch (usually `main`)
4. Click **Run workflow**

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

- Verify GitHub Pages is enabled in repository settings
- Check that the workflow has `pages: write` permission
- Ensure the workflow completed successfully in Actions tab

