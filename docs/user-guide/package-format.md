# Package Format

Packages are defined using JSON manifests. This document describes the format.

## Basic Structure

```json
{
  "name": "example-package",
  "version": "1.0.0",
  "description": "An example package",
  "source": {
    "type": "git",
    "url": "https://github.com/user/repo.git",
    "branch": "main"
  },
  "dependencies": ["libfoo@1.0.0", "libbar"],
  "build_dependencies": ["cmake@3.20.0", "pkg-config"],
  "build_system": "cmake",
  "cmake_args": ["-DBUILD_SHARED_LIBS=ON"],
  "env": {
    "CXXFLAGS": "-O2"
  }
}
```

## Multi-Version Format

Packages can have multiple versions:

```json
{
  "name": "example-package",
  "versions": [
    {
      "version": "1.2.0",
      "description": "Latest version",
      "source": {...},
      "dependencies": [...],
      ...
    },
    {
      "version": "1.1.0",
      "description": "Previous version",
      "source": {...},
      "dependencies": [...],
      ...
    }
  ]
}
```

## Required Fields

- `name`: Package name (must match filename)
- `version`: Package version (or `versions` array for multi-version)
- `description`: Brief description
- `source`: Source information (see below)

## Source Types

### Tarball

```json
{
  "source": {
    "type": "tarball",
    "url": "https://example.com/releases/package-1.0.0.tar.gz"
  }
}
```

### Git Repository

```json
{
  "source": {
    "type": "git",
    "url": "https://github.com/user/repo.git",
    "branch": "main"
  }
}
```

Or with a specific tag or commit:

```json
{
  "source": {
    "type": "git",
    "url": "https://github.com/user/repo.git",
    "tag": "v1.0.0"
  }
}
```

### Zip Archive

```json
{
  "source": {
    "type": "zip",
    "url": "https://example.com/releases/package-1.0.0.zip"
  }
}
```

### Local Directory

```json
{
  "source": {
    "type": "local",
    "path": "/path/to/source"
  }
}
```

## Build Systems

### Autotools

```json
{
  "build_system": "autotools",
  "configure_args": [
    "--prefix=/opt/tsi",
    "--enable-shared"
  ]
}
```

### CMake

```json
{
  "build_system": "cmake",
  "cmake_args": [
    "-DBUILD_SHARED_LIBS=ON",
    "-DCMAKE_BUILD_TYPE=Release"
  ]
}
```

### Meson

```json
{
  "build_system": "meson",
  "meson_args": [
    "--buildtype=release"
  ]
}
```

### Make

```json
{
  "build_system": "make",
  "make_args": ["-j4"]
}
```

### Custom

```json
{
  "build_system": "custom",
  "build_commands": [
    "./custom-build.sh",
    "make install"
  ]
}
```

### Meta

A package that installs nothing of its own and exists only to pull in others.
It declares **no source** — TSI records it and its dependencies and skips fetch,
build and link entirely.

```json
{
  "name": "autotools",
  "version": "2.72",
  "description": "Meta-package for GNU Autoconf, Automake, and Libtool",
  "dependencies": ["autoconf", "automake", "libtool"],
  "build_system": "meta"
}
```

Before this existed, `autotools` had to name some source to satisfy the schema
and downloaded GNU hello on every install. A `meta` package that declares a
source, or that declares no dependencies, is rejected by
`scripts/validate-packages.py`.

## Dependencies

### Runtime Dependencies

```json
{
  "dependencies": ["zlib", "openssl", "curl@8.7.1"]
}
```

### Build Dependencies

```json
{
  "build_dependencies": ["pkg-config", "cmake@3.20.0"]
}
```

Dependencies can be:
- Unversioned: `"zlib"`
- Versioned: `"curl@8.7.1"`

## Environment Variables

```json
{
  "env": {
    "CFLAGS": "-O2 -g",
    "CXXFLAGS": "-O2 -g",
    "LDFLAGS": "-L/opt/lib"
  }
}
```

## Patches

```json
{
  "patches": [
    "https://example.com/patches/fix.patch",
    "/path/to/local.patch"
  ]
}
```

## Optional Fields

- `configure_args`: Arguments for `./configure` (autotools)
- `cmake_args`: Arguments for `cmake`
- `meson_args`: Arguments for `meson`
- `make_args`: Arguments for `make`
- `env`: Environment variables
- `patches`: Array of patch file URLs or paths
- `build_commands`: Custom build commands (for custom build system)
- `source_dir`: Subdirectory of the fetched tree that holds the build root
- `platforms`: Platforms this version can build on, e.g. `["linux"]` or `["linux-aarch64", "darwin"]`. Omit it (the default) for portable packages; see [OS-specific configuration](../developer-guide/os-specific-config.md#restricting-a-package-to-some-platforms).

## `$TSI_INSTALL_DIR`

Package definitions can write `$TSI_INSTALL_DIR`, and TSI expands it before the
build tool sees it. **It expands to two different directories depending on where
you write it**, because two different directories are what you want in each
place:

| Where | Expands to | Use it for |
|---|---|---|
| `make_args` (build_system `make`), `build_commands` | this package's own versioned install dir, `install/<name>-<version>/` | telling the build where to install: `PREFIX=$TSI_INSTALL_DIR` |
| `configure_args`, `cmake_args`, `meson_args`, `make_args` (build_system `autotools`), `env` values | the shared prefix, `install/` | pointing at what dependencies already installed: `-I$TSI_INSTALL_DIR/include` |

The split is by what you need in each place, not by whim. Packages install into
their own versioned directory and are symlinked into the shared prefix
afterwards, so a value naming the package's own directory points at something
still empty while the package is being built -- useless for finding a
dependency. Conversely `make` and `build_commands` have no `--prefix` to
receive, so they have to be told where to install.

`autotools` is the case worth pausing on: it is the one build system where
`make_args` mean the shared prefix, because TSI passes `--prefix` to configure
itself, so an autotools package never has to name its own install directory.
Use `make_args` there for variables configure assigns in the Makefile and does
not let you override any other way -- `readline` needs
`SHLIB_LIBS=-L$TSI_INSTALL_DIR/lib -ltinfow`, without which its shared library
links no termcap library and everything using it dies on an undefined `UP`.

### Overriding `CPPFLAGS`

TSI already puts the shared prefix on the include path, but on macOS it uses
`-idirafter`, which loses to `/usr/include`. That is deliberate -- it keeps the
prefix from shadowing system headers -- but it is wrong when a dependency in the
prefix *replaces* a system library. `gawk` is the case: macOS ships a
`readline/readline.h` that is really a libedit shim without `history_list()`, so
gawk has to be told to prefer the prefix's GNU readline:

```json
"dependencies": ["readline"],
"env": { "CPPFLAGS": "-I$TSI_INSTALL_DIR/include" }
```

A package's own `CPPFLAGS` replaces TSI's default rather than adding to it, so
`-I` here wins over `/usr/include`. Reach for this only when a prefix header
must beat a system one; otherwise leave the default alone.

## Example Package Definitions

See the `packages/` directory for complete examples.

## Version Constraints

When specifying dependencies, you can use version constraints:

- `"package"` - Any version
- `"package@1.0.0"` - Exact version
- Version matching is done by string comparison

## Best Practices

1. **Use semantic versioning** for versions
2. **Include version in source URL** when possible
3. **Specify build dependencies** explicitly
4. **Use consistent URL patterns** across versions
5. **Document any special build requirements** in description

