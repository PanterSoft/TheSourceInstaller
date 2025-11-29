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

