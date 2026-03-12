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

**Examples:**

```bash
tsi install zlib
tsi install curl@8.7.1
tsi install --prefix /opt/tsi cmake
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

**Examples:**

```bash
tsi upgrade              # Upgrade all
tsi upgrade curl zlib    # Upgrade specific packages
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

## Exit Codes

- `0` - Success
- `1` - Error (e.g., package not found, build failure)

## Environment Variables

TSI respects:

- `HOME` / `USERPROFILE` - For default prefix resolution
- `PATH` - For finding build tools
