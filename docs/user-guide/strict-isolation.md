# Strict Isolation Mode

TSI supports a strict isolation mode that enforces complete independence from system tools (except during bootstrap).

## Configuration

Strict isolation is configured via a `tsi.cfg` file in your TSI installation directory (e.g., `/opt/tsi/tsi.cfg` or `~/.tsi/tsi.cfg`).

**Note:** The `tsi.cfg` file is created automatically during installation with default settings. You can edit it at any time to change the configuration.

### Config File Format

The `tsi.cfg` file is automatically created during installation with the following default format:

```ini
# TSI Configuration File
# Enable strict isolation mode (only use TSI packages after bootstrap)
strict_isolation=true
```

### Valid Values

- `strict_isolation=true` or `strict_isolation=1` or `strict_isolation=yes` - Enable strict isolation
- `strict_isolation=false` or `strict_isolation=0` or `strict_isolation=no` - Disable strict isolation (default)

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
   # Edit the config file (created automatically during installation)
   nano /opt/tsi/tsi.cfg
   # or
   echo "strict_isolation=true" > /opt/tsi/tsi.cfg
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
1. Remove the `strict_isolation` line from `tsi.cfg`
2. Set `strict_isolation=false` in `tsi.cfg`
3. Delete the `tsi.cfg` file

