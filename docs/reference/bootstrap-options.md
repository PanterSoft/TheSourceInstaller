# Bootstrap Options

The TSI bootstrap installer (`tsi-bootstrap.sh`) supports several options for customizing the installation.

## Usage

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- [options]
```

Or download and run locally:

```bash
./tsi-bootstrap.sh [options]
```

## Options

### `--repair`

Repair or update an existing TSI installation.

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --repair
```

**What it does:**
- Detects existing installation
- Checks for source updates (if source is a git repository, fetches updates)
- Rebuilds TSI from updated source
- Reinstalls TSI binary and completion scripts
- **Preserves all data**: packages, database, repository, sources

**When to use:**
- TSI binary is corrupted or missing
- TSI is outdated and you want to update
- TSI stopped working after system updates
- You want to rebuild TSI with a different compiler

### `--prefix PATH`

Install TSI to a custom location.

```bash
PREFIX=/opt/tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

Or:

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --prefix /opt/tsi
```

**Default:** `$HOME/.tsi`

**What it does:**
- Installs TSI to the specified prefix
- Creates directory structure under the prefix
- Sets up binaries, completion scripts, and data directories

### `--help` or `-h`

Show help message.

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --help
```

## Environment Variables

You can also use environment variables to configure the installer:

### `PREFIX`

Installation prefix (same as `--prefix` option).

```bash
PREFIX=/opt/tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

### `TSI_REPO`

Custom repository URL (default: `https://github.com/PanterSoft/tsi.git`).

```bash
TSI_REPO=https://github.com/user/fork.git curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

### `TSI_BRANCH`

Branch to use from repository (default: `main`).

```bash
TSI_BRANCH=develop curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

### `INSTALL_DIR`

Temporary directory for downloading and building source (default: `$HOME/tsi-install`).

```bash
INSTALL_DIR=/tmp/tsi-build curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

## Examples

### Standard Installation

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

### Custom Prefix

```bash
PREFIX=/opt/tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

### Repair Existing Installation

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --repair
```

### Install from Fork

```bash
TSI_REPO=https://github.com/user/fork.git TSI_BRANCH=feature curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

### Combine Options

```bash
PREFIX=/opt/tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --repair
```

## After Installation

After installation, add TSI to your PATH:

```bash
export PATH="$PREFIX/bin:$PATH"
```

Or add to your shell profile:

```bash
echo "export PATH=\"$PREFIX/bin:\$PATH\"" >> ~/.bashrc
```

## Troubleshooting

### Installation Fails

- Check that you have a C compiler: `gcc --version` or `clang --version`
- Check that you have `make`: `make --version`
- Verify internet connection for downloading source

### Repair Fails

- Check file permissions on installation directory
- Verify C compiler is available
- Check that source repository is accessible

### Custom Prefix Issues

- Ensure the prefix directory is writable
- Use absolute paths for the prefix
- Check disk space in the target location

