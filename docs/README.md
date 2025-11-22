# TSI Documentation

This directory contains the source files for the TSI documentation, built with [MkDocs](https://www.mkdocs.org/) and the [Material theme](https://squidfunk.github.io/mkdocs-material/).

## Structure

The documentation is organized into the following sections:

- **Getting Started**: Installation, quick start, and basic usage
- **User Guide**: Package management, package format, external packages, version discovery
- **Developer Guide**: Architecture, repository structure, maintenance
- **Workflows**: Automation, version discovery, external packages, permissions, triggers
- **Reference**: Package repository, scripts, bootstrap options

## Building Locally

### Prerequisites

Install MkDocs and required plugins:

```bash
pip install -r requirements-docs.txt
```

Or install manually:

```bash
pip install mkdocs mkdocs-material mkdocs-git-revision-date-localized-plugin pymdown-extensions
```

### Build

```bash
# Build the documentation
mkdocs build

# Serve locally (with auto-reload)
mkdocs serve
```

The documentation will be available at `http://127.0.0.1:8000/`

### Deploy

The documentation is automatically built and deployed to GitHub Pages via GitHub Actions when changes are pushed to the `main` branch.

## Configuration

The MkDocs configuration is in `mkdocs.yml` at the repository root.

## Adding Documentation

1. Add or edit Markdown files in the appropriate directory
2. Update `mkdocs.yml` if adding new pages or sections
3. Test locally with `mkdocs serve`
4. Commit and push - GitHub Actions will build and deploy

## Old Documentation Files

The old standalone Markdown files in this directory have been organized into the MkDocs structure. They are preserved for reference but the new structure should be used going forward.

