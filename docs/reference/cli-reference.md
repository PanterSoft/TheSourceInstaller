# CLI Reference

Complete reference for all TSI commands and options.

## Global Options

All commands support:

- `--help`, `-h` - Show help
- `--version`, `-V` - Show version

Many commands support:

- `--prefix PATH` - Use custom installation prefix (default: `~/.tsi` on Unix, `%USERPROFILE%\.tsi` on Windows)

## Commands

### Install

Install a package from source.

```bash
tsi install <package-name> [options]
tsi install <package-name>@<version> [options]
```

**Options:**

- `--force` - Force reinstall even if already installed
- `--prefix PATH` - Installation prefix
- `--verbose` - Show full build output (default: compact, one line per step like Homebrew)

**Examples:**

```bash
tsi install zlib
tsi install curl@8.7.1
tsi install --prefix /opt/tsi cmake
tsi install --verbose curl   # full configure/make output
```

### Uninstall

Remove an installed package.

```bash
tsi uninstall <package> [package...] [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi uninstall zlib
tsi uninstall curl openssl
```

### Remove

Uninstall TSI from the system. Removes the installation prefix (binary, completions, package database, and all installed packages). You will be asked to confirm unless `--yes` is used.

```bash
tsi remove [options]
```

**Options:**

- `--prefix PATH` - Installation prefix to remove (default: detected from binary location)
- `--yes` - Skip confirmation prompt

**Examples:**

```bash
tsi remove                    # Interactive: prompts for confirmation
tsi remove --prefix /opt/tsi   # Remove custom prefix
tsi remove --yes              # Non-interactive (e.g. scripts)
```

### Upgrade

Upgrade installed packages to latest versions.

```bash
tsi upgrade [package...] [options]
```

**Options:**

- `--prefix PATH` - Installation prefix
- `--verbose` - Show full build output (default: compact)

**Examples:**

```bash
tsi upgrade              # Upgrade all
tsi upgrade curl zlib     # Upgrade specific packages
tsi upgrade --verbose     # Full build output when upgrading
```

### List

List installed packages.

```bash
tsi list [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

### Search

Search available packages.

```bash
tsi search <query> [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi search curl
tsi search ssl
```

### Info

Show detailed package information including available versions.

```bash
tsi info <package-name> [options]
tsi info <package-name>@<version> [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi info curl
tsi info zlib@1.3.1
```

### Update

Fetch the latest package definitions from the repository.

```bash
tsi update [options]
```

**Options:**

- `--repo URL` - Git repository URL
- `--local PATH` - Local directory path
- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi update
tsi update --repo https://github.com/user/packages.git
tsi update --local ./packages
```

### Doctor

Check your system for potential problems.

```bash
tsi doctor [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi doctor
```

Doctor checks for:

- C compiler (gcc/clang/cc)
- make
- Package definitions
- Install prefix writability
- git (for some packages)

### UI

Browse, install, and uninstall packages in an interactive terminal UI.

```bash
tsi ui [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

**Keybindings** (press `?` inside the UI for this list):

| Key | Action |
| --- | --- |
| `Up`/`Down`, `j`/`k` | Move selection |
| `PageUp`/`PageDown` | Move selection by 10 |
| `Home`/`g`, `End`/`G` | Jump to first/last |
| `Tab` | Cycle view (All / Installed / Available) |
| `/` | Filter packages by name or description |
| `i` | Install selected package (asks for confirmation) |
| `u` | Uninstall selected package (asks for confirmation) |
| `Esc` | Cancel filter/confirmation |
| `?` | Toggle help |
| `q` | Quit |

Requires an interactive terminal. Install/uninstall temporarily leave the UI to show the normal streaming output, then return.

## Exit Codes

- `0` - Success
- `1` - Error (e.g., package not found, build failure)

## Environment Variables

TSI respects:

- `HOME` / `USERPROFILE` - For default prefix resolution
- `PATH` - For finding build tools
