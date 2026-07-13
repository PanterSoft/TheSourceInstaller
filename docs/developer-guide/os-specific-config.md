# OS-Specific Package Configuration

TSI supports OS-specific configurations in package definitions to handle differences between operating systems (e.g. macOS vs Linux vs Windows).

## Format

Package JSON files use the pattern `<field>_<os>` for the supported OS names below.

## Supported OS names (in JSON)

These keys are **implemented** for the three primary families:

- **`darwin`** — macOS
- **`linux`** — Linux distributions
- **`windows`** — Microsoft Windows

[`src/platform/mod.rs`](https://github.com/PanterSoft/TheSourceInstaller/blob/main/src/platform/mod.rs) can also report `freebsd`, `openbsd`, `netbsd`, or `unknown` at runtime, but **there are no** `env_freebsd` / `configure_args_freebsd`-style fields yet. On those hosts, only base `env`, `configure_args`, and `cmake_args` apply.

## Supported OS-specific fields

| Base field | OS-specific keys | Merge behavior |
|------------|------------------|----------------|
| `env` | `env_darwin`, `env_linux`, `env_windows` | **Merge keys:** start from base `env`, then apply the current OS map (overwriting duplicate keys), then apply arch maps `env_x86_64` / `env_aarch64` on top. |
| `configure_args` | `configure_args_darwin`, `configure_args_linux`, `configure_args_windows` | **Replace list:** if the OS-specific list is present, it **replaces** base `configure_args` entirely; otherwise base is used. Arch extras `configure_args_x86_64` / `configure_args_aarch64` are **appended** when set. |
| `cmake_args` | `cmake_args_darwin`, `cmake_args_linux`, `cmake_args_windows` | **Replace list:** if the OS-specific list is present, it **replaces** base `cmake_args` entirely; otherwise base `cmake_args` is used. No arch-specific cmake keys (yet). |

Not implemented in JSON today: OS-specific `make_args_*`, `build_system_*`, or OS-specific dependencies.

## Examples

### Environment (key merge)

```json
{
  "env": { "CFLAGS": "-O2 -g" },
  "env_darwin": {
    "CFLAGS": "-O2 -g -Wno-error=format-nonliteral"
  }
}
```

On macOS, `CFLAGS` ends up as the darwin value; on Linux/Windows, the base value unless `env_linux` / `env_windows` overrides.

### Autotools / configure (replace list when OS block exists)

```json
{
  "configure_args": ["--disable-nls"],
  "configure_args_linux": ["--disable-nls", "--disable-werror"]
}
```

On Linux the full configure argument list is exactly `configure_args_linux`. On macOS, if `configure_args_darwin` is omitted, the list is `["--disable-nls"]`.

### CMake (replace list when OS block exists)

```json
{
  "cmake_args": ["-DCMAKE_BUILD_TYPE=Release"],
  "cmake_args_darwin": [
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0"
  ],
  "cmake_args_windows": ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"]
}
```

On each OS, if the corresponding `cmake_args_<os>` is set, that list is used instead of `cmake_args`.

## OS detection

- **macOS** → `darwin`
- **Linux** → `linux`
- **Windows** → `windows`
- **BSD / other** → no `_*` overrides apply unless future fields are added; base fields only.
