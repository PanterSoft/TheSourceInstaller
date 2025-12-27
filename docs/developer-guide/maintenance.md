# TSI Maintenance Guide

## Repairing TSI Installation

If your TSI installation is broken or outdated, you can repair it using the bootstrap installer with the `--repair` option.

### When to Use Repair Mode

- TSI binary is corrupted or missing
- TSI is outdated and you want to update
- TSI stopped working after system updates
- You want to rebuild TSI with a different compiler

### Repair Installation

**One-line repair:**
```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --repair
```

**Local repair:**
```bash
./tsi-bootstrap.sh --repair
```

**Custom prefix:**
```bash
PREFIX=/opt/tsi ./tsi-bootstrap.sh --repair
```

### What Repair Does

1. **Detects existing installation**: Checks if TSI is already installed
2. **Checks for source updates**:
   - If source is a git repository: Fetches and checks for remote changes
   - If source has changed: Updates using `git pull` or re-downloads
   - If source is up to date: Uses existing source
3. **Rebuilds TSI**: Builds TSI from the updated source
4. **Reinstalls**: Copies new binary and completion scripts to installation directory

### Repair Process

The repair mode will:
- ✅ **Check for source updates**: Automatically detects if source code has changed
- ✅ **Update source if needed**: Uses `git pull` for git repositories, or re-downloads if needed
- ✅ **Rebuild TSI**: Compiles fresh binary from updated source
- ✅ **Preserve all data**:
  - Package database (`~/.tsi/db/`)
  - Installed packages (`~/.tsi/install/`)
  - Package repository (`~/.tsi/packages/`)
  - Downloaded sources (`~/.tsi/sources/`)
- ✅ **Only replace**: TSI binary and completion scripts

## Uninstalling TSI

TSI provides an `uninstall` command to completely remove TSI from your system.

### Basic Uninstall

Removes TSI binary and completion scripts, but preserves data:

```bash
tsi uninstall
```

This will:
- Remove `~/.tsi/bin/tsi`
- Remove `~/.tsi/share/completions/`
- **Preserve** all TSI data (packages, database, repository, sources)

### Complete Uninstall

Remove everything including all TSI data:

```bash
tsi uninstall --all
```

This will:
- Remove TSI binary and completion scripts
- Remove all TSI data:
  - `~/.tsi/db/` (package database)
  - `~/.tsi/install/` (installed packages)
  - `~/.tsi/packages/` (package repository)
  - `~/.tsi/sources/` (downloaded sources)
  - `~/.tsi/build/` (build directories)
  - `~/.tsi/` (entire directory)

**Warning**: This will remove all packages installed via TSI!

### Custom Prefix Uninstall

Uninstall from a custom installation location:

```bash
tsi uninstall --prefix /opt/tsi
tsi uninstall --all --prefix /opt/tsi
```

### After Uninstalling

After uninstalling, you should also:

1. **Remove from PATH**: Edit your shell profile (`~/.bashrc`, `~/.zshrc`, etc.) and remove:
   ```bash
   export PATH="$HOME/.tsi/bin:$PATH"
   ```

2. **Remove completion**: Remove completion script sources:
   ```bash
   # Remove from ~/.bashrc or ~/.zshrc
   source ~/.tsi/share/completions/tsi.bash
   source ~/.tsi/share/completions/tsi.zsh
   ```

3. **Reload shell**: Restart your terminal or run:
   ```bash
   source ~/.bashrc  # or ~/.zshrc
   ```

## Reinstalling After Uninstall

After uninstalling, you can reinstall TSI:

```bash
# Reinstall
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh

# If you used --all, you'll need to:
# 1. Reinstall TSI
# 2. Update package repository: tsi update
# 3. Reinstall packages: tsi install <package>
```

## Troubleshooting

### Repair Fails

**Problem**: Repair mode fails to build

**Solutions**:
- Check C compiler is available: `gcc --version`
- Check make is available: `make --version`
- Check internet connection (for downloading source)
- Try manual build: `cd src && make clean && make`

### Uninstall Fails

**Problem**: `tsi uninstall` fails with permission errors

**Solutions**:
- Check file permissions: `ls -la ~/.tsi/bin/tsi`
- Use `sudo` if installed system-wide: `sudo tsi uninstall`
- Manually remove: `rm -rf ~/.tsi`

### Binary Still in PATH

**Problem**: After uninstall, `tsi` command still works

**Solutions**:
- Check PATH: `which tsi`
- May be installed in multiple locations
- Check system-wide installation: `ls -la /usr/local/bin/tsi`
- Remove from shell profile

## Best Practices

1. **Regular Updates**: Use `--repair` periodically to keep TSI updated
2. **Backup Before Uninstall**: If using `--all`, backup important packages
3. **Test After Repair**: Run `tsi --version` to verify repair worked
4. **Clean Uninstall**: Use `--all` only when you're sure you want to remove everything

