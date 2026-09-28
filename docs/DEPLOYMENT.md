# Documentation Deployment

The TSI documentation is built with MkDocs and deployed to GitHub Pages by the **Documentation** workflow (`.github/workflows/docs.yml`).

## When Documentation Is Deployed

- **Pull requests and pushes to `dev`** build the docs with `mkdocs build --strict`, so a broken link or page fails CI, but nothing is deployed.
- **Pushes to `main`** that touch `docs/**`, `mkdocs.yml` or `requirements-docs.txt` build the docs and deploy them to GitHub Pages.
- **Manual runs** (Actions → Documentation → Run workflow on `main`) redeploy the current `main` docs.

Deploys run from `main` rather than from release tags because the `github-pages` environment only accepts deployments from the default branch; a tag-triggered deploy is rejected before it starts. Releases are cut from `main`, so the live docs match the latest release.

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

- Documentation deploys only from `main`. Ensure the change is merged to `main`, or run the Documentation workflow manually on `main`.
- Verify GitHub Pages is enabled in repository settings (Source: GitHub Actions).
- Check that the Documentation workflow completed successfully in the Actions tab. A deploy job that fails within seconds with "not allowed to deploy to github-pages" was started from a branch or tag the environment's deployment rules don't allow (Settings → Environments → github-pages).
