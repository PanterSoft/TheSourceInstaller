# Installation

## Install

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh
```

That's all. Open a new terminal and `tsi` is ready. The installer:

- downloads the pre-built `tsi` binary for your platform (or builds it with cargo
  where there isn't one),
- installs it to `~/.tsi` along with the package definitions,
- adds `~/.tsi/bin` to your PATH with one marked line in your shell profile
  (`~/.zshrc`, `~/.bashrc`, `~/.bash_profile` on macOS, fish's `conf.d`, or
  `~/.profile`).

No curl? Use `wget -qO- <same URL> | sh`.

## Update

Run the same command again. An existing installation is updated in place and keeps
your packages. From a working install, `tsi self-update` does the same.

## Options

Rarely needed; see [Bootstrap Options](../reference/bootstrap-options.md) for all of
them.

```bash
# Install somewhere else
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh -s -- --prefix /opt/tsi

# Leave your shell profile alone (then add $PREFIX/bin to PATH yourself)
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | TSI_NO_MODIFY_PATH=1 sh

# Uninstall: removes ~/.tsi and the PATH line
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh -s -- --uninstall
```

## Platforms with pre-built binaries

Every release ships a binary for each of these (Linux ones are fully static, so they run
on any distro, glibc or musl). Anything else builds from source with cargo.

| OS      | Architectures                                                  |
|---------|----------------------------------------------------------------|
| Linux   | `x86_64`, `aarch64`, `i686`, `armv7`, `armv6`, `riscv64`, `ppc64le` |
| macOS   | `aarch64` (Apple Silicon), `x86_64` (Intel)                     |
| Windows | `x86_64`, `aarch64`                                             |

## Manual Build (from source)

Requires [Rust](https://rustup.rs/) toolchain:

```bash
# Clone the repository
git clone https://github.com/PanterSoft/TheSourceInstaller.git
cd TheSourceInstaller

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

## PATH on Windows or after a manual build

The installer sets up PATH for you. If you built TSI by hand, or use Windows outside
Git Bash/MSYS2, add the `bin` directory yourself:

```bash
export PATH="$HOME/.tsi/bin:$PATH"   # add to your shell profile to keep it
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

**Supported commands:** install, uninstall, upgrade, list, search, info, update, self-update, doctor, remove
