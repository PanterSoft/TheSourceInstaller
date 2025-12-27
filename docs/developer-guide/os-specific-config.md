# OS-Specific Package Configuration

TSI supports OS-specific configurations in package definitions to handle differences between operating systems (e.g., macOS vs Linux/BusyBox vs other Unix variants).

## Format

Package JSON files can include OS-specific fields using the pattern: `<field>_<os>`

## Supported OS Names

OS names are detected automatically via `uname()` and normalized to lowercase:

- **`darwin`** - macOS (Apple's Unix-based operating system)
- **`linux`** - All Linux distributions (Debian, Ubuntu, RedHat, Alpine, Arch, etc.)
- **`freebsd`** - FreeBSD
- **`openbsd`** - OpenBSD
- **`netbsd`** - NetBSD
- **`sunos`** - Solaris/Illumos
- **`aix`** - IBM AIX
- **`hp-ux`** - HP-UX
- **Other Unix variants** - Any other Unix-like system detected by `uname()`

**Note:** If OS detection fails, TSI defaults to `linux` as the most common Unix-like system.

## Supported OS-Specific Fields

- `env_darwin`, `env_linux`, etc. - OS-specific environment variables
- `configure_args_darwin`, `configure_args_linux`, etc. - OS-specific configure arguments
- `cmake_args_darwin`, `cmake_args_linux`, etc. - OS-specific CMake arguments
- `make_args_darwin`, `make_args_linux`, etc. - OS-specific make arguments

## How It Works

1. **Base configuration is loaded first** (e.g., `env`, `configure_args`) - this is the **DEFAULT** that is always used
2. **OS-specific configuration is then merged** based on detected OS
3. **OS-specific values override base values** for the same keys
4. **If OS-specific config doesn't exist**, base configuration is used as-is (default fallback)
5. **If OS-specific config exists but doesn't have a key**, base configuration value is used for that key

### Priority Order (highest to lowest):
1. OS-specific configuration (e.g., `env_darwin`, `env_linux`)
2. Base configuration (e.g., `env`) - **always used as default fallback**

## Example

```json
{
  "name": "m4",
  "version": "1.4.19",
  "build_system": "autotools",
  "env": {
    "CFLAGS": "-O2 -g"
  },
  "env_darwin": {
    "CFLAGS": "-O2 -g -Wno-error -Wno-error=format-nonliteral -Wno-format-nonliteral"
  },
  "configure_args": [],
  "configure_args_linux": [
    "--disable-werror"
  ],
  "configure_args_freebsd": [
    "--disable-werror"
  ]
}
```

In this example:
- **On macOS (darwin)**: `CFLAGS` will be `-O2 -g -Wno-error -Wno-error=format-nonliteral -Wno-format-nonliteral`
- **On Linux**: `CFLAGS` will be `-O2 -g` and `configure_args` will include `--disable-werror`
- **On FreeBSD**: `CFLAGS` will be `-O2 -g` and `configure_args` will include `--disable-werror`
- **On other Unix systems**: Uses base configuration (`CFLAGS: -O2 -g`, no extra configure args)

## OS Detection Details

- **macOS**: Always detected as `darwin` (the kernel name)
- **Linux**: All Linux distributions are detected as `linux` (regardless of distribution)
- **Other Unix variants**: Detected via `uname()` system call
- **Fallback**: If detection fails, defaults to `linux`

This design allows you to:
1. Target specific OS families (e.g., `darwin` for macOS, `linux` for all Linux)
2. Support specific Unix variants (e.g., `freebsd`, `openbsd`)
3. Have a base configuration that works for all systems
4. Override only where necessary for specific OS differences

