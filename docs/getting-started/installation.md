# Installation

## One-Line Install (Recommended)

Install TSI with a single command. The installer downloads TSI source and builds it:

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

Or using `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

The installer will automatically:
- Download TSI source code (via git clone or tarball)
- Build TSI using your C compiler
- Install TSI to `~/.tsi`

**Custom installation location:**
```bash
PREFIX=/opt/tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

**Repair/Update existing installation:**
```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --repair
```

This will rebuild and reinstall TSI, useful for:
- Fixing broken installations
- Updating to the latest version
- Rebuilding after system changes

After installation, add to your PATH:
```bash
export PATH="$HOME/.tsi/bin:$PATH"
```

## Manual Build

```bash
# Clone the repository
git clone https://github.com/PanterSoft/tsi.git
cd tsi

# Build
cd src
make

# Install
sudo make install

# Or install to custom location
make install DESTDIR=/opt/tsi
```

## Requirements

- **C compiler** (gcc, clang, or cc)
- **make**
- **git** or **wget/curl** (for downloading sources)
- **tar** (for extracting archives)

That's it! No Python, no other dependencies.

## Add to PATH

After installation, add TSI to your PATH:

```bash
# For bash/zsh
export PATH="$HOME/.tsi/bin:$PATH"

# Or add to your shell profile
echo 'export PATH="$HOME/.tsi/bin:$PATH"' >> ~/.bashrc
```

## Enable Autocomplete

TSI includes comprehensive shell completion for bash and zsh with full support for all commands and options:

**Bash:**
```bash
source ~/.tsi/share/completions/tsi.bash
# Or add to ~/.bashrc:
echo 'source ~/.tsi/share/completions/tsi.bash' >> ~/.bashrc
```

**Zsh:**
```bash
source ~/.tsi/share/completions/tsi.zsh
# Or add to ~/.zshrc:
echo 'source ~/.tsi/share/completions/tsi.zsh' >> ~/.zshrc
```

**Complete autocomplete support:**

- **Command completion**: `tsi <TAB>` - Shows all commands (install, remove, list, info, update, uninstall, --help, --version)
- **Package completion**:
  - `tsi install <TAB>` - Shows available packages from repository
  - `tsi install --force <TAB>` - Shows packages (after options)
  - `tsi info <TAB>` - Shows available packages
- **Installed package completion**:
  - `tsi remove <TAB>` - Shows installed packages
- **Option completion**:
  - `tsi install --<TAB>` - Shows `--force`, `--prefix`
  - `tsi update --<TAB>` - Shows `--repo`, `--local`, `--prefix`
  - `tsi uninstall --<TAB>` - Shows `--prefix`
- **Directory completion**:
  - `tsi install --prefix <TAB>` - Completes directory paths
  - `tsi update --local <TAB>` - Completes directory paths
  - `tsi uninstall --prefix <TAB>` - Completes directory paths

