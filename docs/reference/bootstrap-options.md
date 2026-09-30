# Bootstrap Options

The TSI bootstrap installer (`tsi-bootstrap.sh`) needs no options: it installs to
`~/.tsi`, puts `~/.tsi/bin` on your PATH, and running it again updates the
installation. The options below are for the cases that need something else.

Environment variables go on the `sh` side of the pipe (`curl … | PREFIX=/opt/tsi sh`):
put before `curl`, they would only reach `curl`.

## Usage

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh -s -- [options]
```

Or download and run locally:

```bash
./tsi-bootstrap.sh [options]
```

## Options

### `--uninstall`

Remove TSI completely from the system. Deletes the entire installation prefix (binary, completions, package database, installed packages, and all data).

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh -s -- --uninstall
```

**Custom prefix:**
```bash
curl -fsSL .../tsi-bootstrap.sh | sh -s -- --uninstall --prefix /opt/tsi
```

**Non-interactive:** Set `UNINSTALL=1` and use `--non-interactive` to skip the confirmation prompt.

**Safety:** Uninstall only runs if a TSI binary is found at `PREFIX/bin/tsi` (or `tsi.exe`), so a wrong prefix will not be deleted.

### `--repair`

Repair or update an existing TSI installation.

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh -s -- --repair
```

**What it does:**
- Detects existing installation
- Checks for source updates (if source is a git repository, fetches updates)
- Downloads pre-built binary from GitHub releases, or builds from source with `cargo build --release`
- Reinstalls TSI binary and completion scripts
- **Preserves all data**: packages, database, repository, sources

**When to use:**
- TSI binary is corrupted or missing
- TSI is outdated and you want to update
- TSI stopped working after system updates

### `--prefix PATH`

Install TSI to a custom location.

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | PREFIX=/opt/tsi sh
```

Or:

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh -s -- --prefix /opt/tsi
```

**Default:** `$HOME/.tsi`

**What it does:**
- Installs TSI to the specified prefix
- Creates directory structure under the prefix
- Sets up binaries, completion scripts, and data directories

### `--help` or `-h`

Show help message.

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh -s -- --help
```

## Environment Variables

You can also use environment variables to configure the installer:

### `PREFIX`

Installation prefix (same as `--prefix` option).

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | PREFIX=/opt/tsi sh
```

### `TSI_REPO`

Custom repository URL (default: `https://github.com/PanterSoft/TheSourceInstaller.git`).

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | TSI_REPO=https://github.com/user/fork.git sh
```

### `TSI_BRANCH`

Branch to use from repository (default: `main`).

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | TSI_BRANCH=develop sh
```

### `UNINSTALL`

Set to `1`, `true`, or `yes` to enable uninstall mode (same as `--uninstall`). Use with `--non-interactive` for scripted uninstall.

```bash
curl -fsSL .../tsi-bootstrap.sh | UNINSTALL=1 sh -s -- --uninstall --non-interactive
```

### `TSI_NO_MODIFY_PATH`

Set to `1`, `true`, or `yes` to keep the installer from editing your shell profile.
You then add `$PREFIX/bin` to PATH yourself.

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | TSI_NO_MODIFY_PATH=1 sh
```

### `INSTALL_DIR`

Temporary directory for downloading and building source (default: `$HOME/tsi-install`).

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | INSTALL_DIR=/tmp/tsi-build sh
```

## Examples

### Standard Installation

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh
```

### Custom Prefix

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | PREFIX=/opt/tsi sh
```

### Repair Existing Installation

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh -s -- --repair
```

### Uninstall TSI

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh -s -- --uninstall
```

### Install from Fork

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | TSI_REPO=https://github.com/user/fork.git TSI_BRANCH=feature sh
```

### Combine Options

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | PREFIX=/opt/tsi sh -s -- --repair
```

## PATH

Unless `$PREFIX/bin` is already on your PATH or `TSI_NO_MODIFY_PATH` is set, the
installer appends one line to your shell's startup file:

```bash
export PATH="$HOME/.tsi/bin:$PATH"  # added by the TSI installer
```

It picks the file from `$SHELL`: `~/.zshrc` (or `$ZDOTDIR/.zshrc`) for zsh,
`~/.bashrc` for bash (`~/.bash_profile` on macOS), `~/.config/fish/conf.d/tsi.fish`
for fish (`fish_add_path`), and `~/.profile` otherwise. Running the installer again
doesn't add a second line, and `--uninstall` removes every line carrying the
`# added by the TSI installer` marker.

## Troubleshooting

### Installation Fails

- Verify internet connection for downloading pre-built binary
- If building from source: ensure Rust toolchain is installed (`rustc --version`)
- Check file permissions on installation directory

### Repair Fails

- Check file permissions on installation directory
- Verify internet connection (for pre-built binary) or Rust toolchain (for source build)
- Check that source repository is accessible

### Custom Prefix Issues

- Ensure the prefix directory is writable
- Use absolute paths for the prefix
- Check disk space in the target location

