<p align="center">
  <img src="docs/assets/logo.svg" alt="TSI logo" width="96" height="96">
</p>

<h1 align="center">TSI · The Source Installer</h1>

<p align="center">
  Build any package from source, with all its dependencies, on any system.<br>
  One static binary. No runtime dependencies.
</p>

<p align="center">
  <a href="https://github.com/PanterSoft/TheSourceInstaller/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/PanterSoft/TheSourceInstaller?style=flat-square&color=C62828&label=release"></a>
  <a href="https://github.com/PanterSoft/TheSourceInstaller/actions/workflows/test.yml"><img alt="Tests" src="https://img.shields.io/github/actions/workflow/status/PanterSoft/TheSourceInstaller/test.yml?branch=main&style=flat-square&label=tests"></a>
  <a href="https://pantersoft.github.io/TheSourceInstaller/"><img alt="Documentation" src="https://img.shields.io/badge/docs-online-C62828?style=flat-square"></a>
  <a href="LICENSE"><img alt="MIT License" src="https://img.shields.io/badge/license-MIT-8E1B1B?style=flat-square"></a>
</p>

<p align="center">
  <a href="#install">Install</a> ·
  <a href="#use">Use</a> ·
  <a href="https://pantersoft.github.io/TheSourceInstaller/">Documentation</a>
</p>

---

## Why TSI

- **Works where package managers don't.** Minimal containers, old or unusual
  distros, macOS and Windows, one tool for all of them.
- **Everything from source.** Any version of any package, built with your options
  into its own prefix, so nothing clashes with the system.
- **Nothing to install first.** Downloading and unpacking are built in. To build
  packages you only need a C compiler and `make`.

## Install

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh
export PATH="$HOME/.tsi/bin:$PATH"
```

Pre-built binaries cover Linux (x86_64, aarch64, i686, armv7, armv6, riscv64,
ppc64le), macOS and Windows. Other platforms build from source automatically.
For a custom location, repair or uninstall, see the
[installation guide](https://pantersoft.github.io/TheSourceInstaller/getting-started/installation/).

## Use

```bash
tsi update              # fetch package definitions (once after install)
tsi install curl        # build and install a package with its dependencies
tsi install git@2.45.0  # a specific version
tsi list                # what's installed
tsi search ssl          # what's available
tsi upgrade             # upgrade everything
tsi uninstall curl      # remove a package
tsi doctor              # check your build tools
tsi ui                  # interactive terminal UI (press ? for keys)
```

Need machine-readable output? `tsi list --json` writes clean JSON to stdout.

## Build from source

```bash
git clone --recurse-submodules https://github.com/PanterSoft/TheSourceInstaller.git
cd TheSourceInstaller
cargo build --release   # binary at target/release/tsi
```

## Learn more

[Getting started](https://pantersoft.github.io/TheSourceInstaller/getting-started/quick-start/) ·
[Package format](https://pantersoft.github.io/TheSourceInstaller/user-guide/package-format/) ·
[Architecture](https://pantersoft.github.io/TheSourceInstaller/developer-guide/architecture/) ·
[Testing](https://pantersoft.github.io/TheSourceInstaller/developer-guide/testing/) ·
[Brand & colors](https://pantersoft.github.io/TheSourceInstaller/brand/)

Contributions are welcome. Open an issue or a pull request.

<sub>MIT licensed · see [LICENSE](LICENSE)</sub>
