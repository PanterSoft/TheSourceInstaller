<p align="center">
  <img src="docs/assets/logo.svg" alt="TSI logo" width="80" height="80">
</p>

<h1 align="center">TSI · The Source Installer</h1>

<p align="center">
  Build any package from source, with its dependencies, on any system.<br>
  <a href="https://pantersoft.github.io/TheSourceInstaller/">Documentation</a> ·
  <a href="https://github.com/PanterSoft/TheSourceInstaller/releases/latest">Releases</a>
</p>

## Install

```bash
curl -fsSL https://raw.githubusercontent.com/PanterSoft/TheSourceInstaller/main/tsi-bootstrap.sh | sh
```

Then open a new terminal. Run the same command again to update.

## Use

```bash
tsi install curl      # build and install a package with its dependencies
tsi search ssl        # find packages
tsi list              # see what's installed
tsi upgrade           # upgrade everything
tsi ui                # browse and manage packages interactively
```

## Why TSI

- **Runs anywhere:** Linux (any distro), macOS and Windows, as one static binary.
- **Everything from source:** any version, built into its own prefix, never touching the system.
- **Nothing to set up:** downloads and unpacking are built in; you only need a C compiler and `make`.

<sub>MIT License</sub>
