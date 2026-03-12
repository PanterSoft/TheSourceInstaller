# Strict Isolation Mode

TSI supports a strict isolation mode that enforces complete independence from system tools (except during bootstrap).

## Configuration

Strict isolation is configured via a `tsi.toml` file in your TSI installation directory (e.g., `/opt/tsi/tsi.toml` or `~/.tsi/tsi.toml`).

**Note:** You can create the `tsi.toml` file manually. If it does not exist, strict isolation defaults to disabled.

### Config File Format

Create `tsi.toml` in your TSI prefix with the following format:

```toml
# TSI Configuration File
# Enable strict isolation mode (only use TSI packages after bootstrap)
strict_isolation = true

# Optional: log level (info, debug, etc.)
log_level = "info"
```

### Valid Values

- `strict_isolation = true` - Enable strict isolation
- `strict_isolation = false` - Disable strict isolation (default)

## How It Works

### Bootstrap Phase

During bootstrap (building essential packages like `make`, `coreutils`, `tar`, etc.), strict isolation mode still allows:
- **C compiler** (gcc/clang/cc) - Required to build packages
- **Basic system directories** (`/usr/bin`, `/bin`, `/usr/local/bin`) - For essential build tools
- **`/bin/sh`** - POSIX shell requirement

### After Bootstrap

Once bootstrap packages are installed, strict isolation mode:
- **Only uses TSI-installed packages** - All tools come from TSI's bin directory
- **Excludes system tools** - No access to system `/usr/bin`, `/usr/local/bin`, etc.
- **Only includes `/bin`** - For POSIX shell (`/bin/sh`) compatibility
- **No system compiler** - Must use TSI-installed compiler (if you build one)

## Example

1. **Enable strict isolation:**
   ```bash
   # Create or edit the config file
   nano /opt/tsi/tsi.toml
   # or
   echo 'strict_isolation = true' > /opt/tsi/tsi.toml
   ```

2. **Install packages:**
   ```bash
   tsi install git
   ```

   During bootstrap, system tools are used. After bootstrap, only TSI packages are used.

3. **Verify isolation:**
   ```bash
   # Check which tools are being used
   which make    # Should point to /opt/tsi/bin/make
   which tar     # Should point to /opt/tsi/bin/tar
   which gcc     # May still point to system gcc (if not built via TSI)
   ```

## Benefits

- **Complete independence** from system package managers
- **Reproducible builds** - Same tools across different systems
- **No conflicts** with system-installed packages
- **Portable** - Can move TSI installation between systems

## Limitations

- Requires building a C compiler via TSI if you want complete isolation
- Bootstrap phase still needs system tools (unavoidable)
- Some packages may require system libraries during bootstrap

## Disabling Strict Isolation

To disable strict isolation, either:
1. Remove the `strict_isolation` line from `tsi.toml`
2. Set `strict_isolation = false` in `tsi.toml`
3. Delete the `tsi.toml` file

