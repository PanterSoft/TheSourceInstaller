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
3. **Rebuilds TSI**: Downloads pre-built binary from GitHub releases, or builds from source with `cargo build --release`
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

TSI does not provide a self-uninstall command. To remove TSI, manually delete the installation directory.

### Basic Uninstall (Preserve Data)

Remove TSI binary and completion scripts, but preserve data:

```bash
rm -f ~/.tsi/bin/tsi ~/.tsi/bin/tsi.exe
rm -rf ~/.tsi/share/completions/
```

This preserves: `~/.tsi/db/`, `~/.tsi/install/`, `~/.tsi/packages/`, `~/.tsi/sources/`, `~/.tsi/build/`

### Complete Uninstall

Remove everything including all TSI data:

```bash
rm -rf ~/.tsi
```

**Warning**: This will remove all packages installed via TSI!

### Custom Prefix Uninstall

Uninstall from a custom installation location:

```bash
rm -rf /opt/tsi
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
- Check internet connection (for downloading pre-built binary)
- If building from source: ensure Rust toolchain is installed (`rustc --version`)
- Try manual build: `cargo build --release`

### Uninstall Fails

**Problem**: Permission errors when removing files

**Solutions**:
- Check file permissions: `ls -la ~/.tsi/bin/tsi`
- Use `sudo` if installed system-wide: `sudo rm -rf /opt/tsi`

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

