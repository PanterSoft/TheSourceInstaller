# Installation

## One-Line Install (Recommended)

Install TSI with a single command. The installer downloads TSI source and builds the Rust binary (or uses a pre-built binary when available):

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

Or using `wget`:

```bash
wget -qO- https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

The installer will automatically:
- Download TSI source code (via git clone or tarball)
- Build TSI using Rust (cargo) or download a pre-built binary
- Install TSI to `~/.tsi`

**Custom installation location:**
```bash
PREFIX=/opt/tsi curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

**Repair/Update existing installation:**
```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh -s -- --repair
```

Or using environment variable:
```bash
REPAIR=1 curl -fsSL https://raw.githubusercontent.com/PanterSoft/tsi/main/tsi-bootstrap.sh | sh
```

This will rebuild and reinstall TSI, useful for:
- Fixing broken installations
- Updating to the latest version
- Rebuilding after system changes

After installation, add to your PATH:
```bash
export PATH="$HOME/.tsi/bin:$PATH"
```

## Manual Build (from source)

Requires [Rust](https://rustup.rs/) toolchain:

```bash
# Clone the repository
git clone https://github.com/PanterSoft/tsi.git
cd tsi

# Build
cargo build --release

# Install (Unix)
make install PREFIX=~/.tsi

# Or install manually
cp target/release/tsi ~/.tsi/bin/tsi
chmod +x ~/.tsi/bin/tsi
```

**Windows:** The binary will be at `target\release\tsi.exe`. Copy it to your desired location and add that directory to your PATH.

## Requirements

**To run TSI:** None — the binary is self-contained.

**To build TSI from source:** [Rust](https://rustup.rs/) toolchain (rustc, cargo).

**To build packages with TSI:**
- **macOS**: Xcode Command Line Tools (clang, make)
- **Linux**: gcc (or clang) and make
- **Windows**: Visual Studio Build Tools or MinGW

TSI uses built-in HTTP and archive extraction — no system curl, wget, or tar required for downloads.

## Add to PATH

After installation, add TSI to your PATH:

```bash
# For bash/zsh (Unix)
export PATH="$HOME/.tsi/bin:$PATH"

# Or add to your shell profile
echo 'export PATH="$HOME/.tsi/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

**Windows (PowerShell):**
```powershell
$env:Path += ";$env:USERPROFILE\.tsi\bin"
```

**Windows (permanent):** Add `%USERPROFILE%\.tsi\bin` to your system PATH via System Properties.

## Enable Autocomplete

TSI includes shell completion for bash and zsh:

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

**Supported commands:** install, uninstall, upgrade, list, search, info, update, doctor, remove
