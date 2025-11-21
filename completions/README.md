# TSI Shell Completion

Shell completion scripts for TSI to enable Tab completion in your terminal.

## Supported Shells

- **Bash**: `tsi.bash`
- **Zsh**: `tsi.zsh`

## Installation

### Automatic Installation

The completion scripts are automatically installed when using `tsi-bootstrap.sh`. They are installed to:
- `~/.tsi/share/completions/tsi.bash`
- `~/.tsi/share/completions/tsi.zsh`

### Manual Installation

**Bash:**
```bash
# Copy to your home directory
cp completions/tsi.bash ~/.tsi/share/completions/

# Source in your ~/.bashrc
echo 'source ~/.tsi/share/completions/tsi.bash' >> ~/.bashrc

# Or install system-wide
sudo cp completions/tsi.bash /etc/bash_completion.d/tsi
```

**Zsh:**
```bash
# Copy to your home directory
cp completions/tsi.zsh ~/.tsi/share/completions/

# Source in your ~/.zshrc
echo 'source ~/.tsi/share/completions/tsi.zsh' >> ~/.zshrc

# Or add to fpath (recommended)
mkdir -p ~/.zsh/completions
cp completions/tsi.zsh ~/.zsh/completions/_tsi
echo 'fpath=(~/.zsh/completions $fpath)' >> ~/.zshrc
autoload -U compinit && compinit
```

## Features

### Command Completion
- `tsi <TAB>` - Shows all available commands:
  - `install` - Install a package
  - `remove` - Remove an installed package
  - `list` - List installed packages
  - `info` - Show package information
  - `update` - Update package repository
  - `uninstall` - Uninstall TSI
  - `--help`, `--version` - Help and version

### Package Completion
- `tsi install <TAB>` - Shows available packages from repository (`~/.tsi/repos/*.json`)
- `tsi install --force <TAB>` - Shows available packages (after --force)
- `tsi info <TAB>` - Shows available packages from repository
- `tsi remove <TAB>` - Shows installed packages (from `tsi list` output)

### Option Completion
- `tsi install --<TAB>` - Shows options: `--force`, `--prefix`
- `tsi install --prefix <TAB>` - Completes directory paths
- `tsi update --<TAB>` - Shows options: `--repo`, `--local`, `--prefix`
- `tsi update --local <TAB>` - Completes directory paths
- `tsi update --prefix <TAB>` - Completes directory paths
- `tsi uninstall --<TAB>` - Shows options: `--all`, `--prefix`
- `tsi uninstall --prefix <TAB>` - Completes directory paths

### Multi-Word Command Completion
- `tsi install --force <TAB>` - Completes packages after --force
- `tsi install --prefix /path <TAB>` - Completes packages after --prefix
- All commands support proper option and argument completion

## How It Works

### Bash Completion
The bash completion script uses the standard bash completion mechanism:
- Reads package names from `~/.tsi/repos/*.json` files
- Reads installed packages from `tsi list` command output
- Provides context-aware completion based on the current command

### Zsh Completion
The zsh completion script uses zsh's advanced completion system:
- Uses `_describe` for command descriptions
- Uses `_arguments` for option parsing
- Provides better completion descriptions and grouping

## Troubleshooting

### Completion Not Working

1. **Check if script is sourced:**
   ```bash
   # Bash
   source ~/.tsi/share/completions/tsi.bash

   # Zsh
   source ~/.tsi/share/completions/tsi.zsh
   ```

2. **Check shell profile:**
   - Ensure the source command is in `~/.bashrc` or `~/.zshrc`
   - Restart your terminal or run `source ~/.bashrc` / `source ~/.zshrc`

3. **Check permissions:**
   ```bash
   ls -l ~/.tsi/share/completions/tsi.*
   ```

4. **Test completion:**
   ```bash
   # Should show commands
   tsi <TAB>
   ```

### No Packages Showing

- Ensure you have packages in `~/.tsi/repos/` directory
- Package files should be named `*.json`
- For installed packages, ensure `tsi list` works correctly

### Zsh Completion Issues

If using zsh, ensure completion system is initialized:
```bash
autoload -U compinit
compinit
```

## Customization

You can customize the completion behavior by modifying the completion scripts:
- Change repository directory path
- Add custom package sources
- Modify completion behavior for specific commands

