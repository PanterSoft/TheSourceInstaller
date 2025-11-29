# TSI Package Repository

TSI uses a local package repository to store package definitions. The repository is located at `~/.tsi/repos/` by default.

## Repository Structure

```
~/.tsi/repos/
├── pkg-config.json
├── zlib.json
├── openssl.json
├── cmake.json
├── curl.json
└── ... (other package definitions)
```

Each package is defined as a JSON file. The filename should match the package name (e.g., `pkg-config.json` for the `pkg-config` package).

## Updating the Repository

### Default Update

Update from the official TSI repository:

```bash
tsi update
```

This will:
1. Clone or update the TSI repository from GitHub
2. Copy all `.json` files from the `packages/` directory
3. Update your local repository at `~/.tsi/repos/`

### Custom Repository

Update from a custom Git repository:

```bash
tsi update --repo https://github.com/user/custom-packages.git
```

The repository should have a `packages/` directory containing JSON package definitions.

### Local Directory

Update from a local directory (useful for development):

```bash
tsi update --local /path/to/packages
```

This will copy all `.json` files from the specified directory to your repository.

### Custom Prefix

Update repository for a custom TSI installation:

```bash
tsi update --prefix /opt/tsi
```

## Repository Sources

### Official Repository

The default repository is:
- **URL**: `https://github.com/PanterSoft/tsi.git`
- **Location**: `packages/` directory
- **Packages**: Essential packages (pkg-config, zlib, openssl, cmake, curl, etc.)

### Custom Repositories

You can create your own package repository:

1. Create a Git repository
2. Add a `packages/` directory
3. Add JSON package definitions
4. Update TSI: `tsi update --repo <your-repo-url>`

## Package Discovery

After updating, TSI will automatically discover all packages in the repository:

```bash
# List all available packages (from repository)
ls ~/.tsi/repos/*.json

# Get info about a package
tsi info pkg-config

# Install a package
tsi install pkg-config
```

## Repository Caching

TSI caches the repository clone in `~/.tsi/tmp-repo-update/` to speed up subsequent updates. The cache is automatically updated on each `tsi update` run.

## Manual Repository Management

You can also manually manage the repository:

```bash
# Add a package manually
cp my-package.json ~/.tsi/repos/

# Remove a package
rm ~/.tsi/repos/package-name.json

# Edit a package
vim ~/.tsi/repos/package-name.json
```

## Best Practices

1. **Regular Updates**: Run `tsi update` regularly to get new packages and updates
2. **Version Control**: Keep your custom packages in a Git repository
3. **Backup**: Backup `~/.tsi/repos/` if you have custom packages
4. **Testing**: Use `--local` for testing package definitions before committing

## Troubleshooting

### Update Fails

**Problem**: `tsi update` fails with "Failed to clone/update repository"

**Solutions**:
- Check internet connection
- Ensure `git` is installed
- Try updating manually: `git clone https://github.com/PanterSoft/tsi.git ~/.tsi/tmp-repo-update`

### No Packages Found

**Problem**: Update succeeds but no packages are available

**Solutions**:
- Check if `packages/` directory exists in the repository
- Verify JSON files are valid: `cat ~/.tsi/repos/*.json | jq .`
- Try updating from local: `tsi update --local /path/to/packages`

### Custom Repository Not Working

**Problem**: Custom repository update fails

**Solutions**:
- Ensure repository is publicly accessible (or use SSH for private repos)
- Verify `packages/` directory exists
- Check repository URL is correct

