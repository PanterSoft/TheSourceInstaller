# CLI Reference

Complete reference for all TSI commands and options.

## Global Options

All commands support:

- `--help`, `-h` - Show help
- `--version`, `-V` - Show version

Many commands support:

- `--prefix PATH` - Use custom installation prefix (default: `~/.tsi` on Unix, `%USERPROFILE%\.tsi` on Windows)

## Output Streams

All human-facing progress and diagnostics go to **stderr**. Machine-readable output
(currently `tsi list --json`) goes to **stdout**, so `tsi list --json > packages.json`
captures only clean JSON even when TSI is chatty.

## Commands

### Install

Install a package from source.

```bash
tsi install <package-name> [options]
tsi install <package-name>@<version> [options]
```

**Options:**

- `--force` - Force reinstall even if already installed
- `--prefix PATH` - Installation prefix
- `--verbose` - Show full build output (default: compact, one line per step like Homebrew)

**Examples:**

```bash
tsi install zlib
tsi install curl@8.7.1
tsi install --prefix /opt/tsi cmake
tsi install --verbose curl   # full configure/make output
```

### Uninstall

Remove an installed package.

```bash
tsi uninstall <package> [package...] [options]
```

By default TSI refuses to remove a package that other installed packages depend on,
since doing so leaves them with missing libraries and headers. Remove the dependents
first, or override with `--force`.

**Options:**

- `--force` - Remove even if other installed packages depend on it
- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi uninstall zlib
tsi uninstall curl openssl
tsi uninstall --force zlib     # even though curl needs it
```

Exits non-zero if any named package was refused or failed, so scripts can't mistake a
partial uninstall for a clean one. Packages that were simply not installed are a warning,
not a failure.

### Remove

Uninstall TSI from the system. Removes the installation prefix (binary, completions, package database, and all installed packages). You will be asked to confirm unless `--yes` is used.

```bash
tsi remove [options]
```

**Options:**

- `--prefix PATH` - Installation prefix to remove (default: detected from binary location)
- `--yes` - Skip confirmation prompt

**Examples:**

```bash
tsi remove                    # Interactive: prompts for confirmation
tsi remove --prefix /opt/tsi   # Remove custom prefix
tsi remove --yes              # Non-interactive (e.g. scripts)
```

### Upgrade

Upgrade installed packages to latest versions.

```bash
tsi upgrade [package...] [options]
```

**Options:**

- `--prefix PATH` - Installation prefix
- `--verbose` - Show full build output (default: compact)

**Examples:**

```bash
tsi upgrade              # Upgrade all
tsi upgrade curl zlib     # Upgrade specific packages
tsi upgrade --verbose     # Full build output when upgrading
```

### List

List installed packages.

```bash
tsi list [options]
```

**Options:**

- `--json` - Emit the installed set as JSON on stdout (for scripts and CI)
- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi list
tsi list --json | jq -r '.[].name'
tsi list --json | jq -r '.[] | select(.dependencies | index("zlib")) | .name'
```

Each JSON record carries `name`, `version`, `install_path`, `installed_at` (Unix seconds),
and `dependencies`.

### Search

Search available packages.

```bash
tsi search <query> [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi search curl
tsi search ssl
```

### Info

Show detailed package information including available versions.

```bash
tsi info <package-name> [options]
tsi info <package-name>@<version> [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi info curl
tsi info zlib@1.3.1
```

### Update

Fetch the latest package definitions from the repository.

```bash
tsi update [options]
```

**Options:**

- `--repo URL` - Git repository URL
- `--local PATH` - Local directory path
- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi update
tsi update --repo https://github.com/user/packages.git
tsi update --local ./packages
```

### Doctor

Check your system for potential problems.

```bash
tsi doctor [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

**Examples:**

```bash
tsi doctor
```

Doctor checks for:

- C compiler (gcc/clang/cc)
- make
- Package definitions
- Install prefix writability
- git (for some packages)

### UI

A btop-style terminal workspace for browsing, installing and maintaining packages.

```bash
tsi ui [options]
```

**Options:**

- `--prefix PATH` - Installation prefix

Requires an interactive terminal (fails fast if stdout is not a TTY) and package
definitions in the prefix — run `tsi update` first.

**Tabs**

| Tab | Contents |
| --- | --- |
| `1` Packages | Filterable package list with a details panel |
| `2` System | Prefix, package counts, upgradable count, `d` runs `tsi doctor` |
| `3` TSI | Maintenance actions: update definitions, self-update, bootstrap, remove TSI |

**Keybindings** (press `?` inside the UI for this list):

| Key | Action |
| --- | --- |
| `1`/`2`/`3` | Switch tab |
| `Up`/`Down`, `j`/`k` | Move selection |
| `PageUp`/`PageDown` | Move selection by 10 (scrolls the log pane when it is open) |
| `Home`/`g`, `End`/`G` | Jump to first/last |
| `Tab` | Cycle view (All / Installed / Available) |
| `/` | Filter packages by name or description |
| `Space` | Mark/unmark the package for a batch action |
| `i` | Install — the selected package, or every marked one |
| `r` | Remove — the selected package, or every marked one |
| `u` | Upgrade — the selected package, or every marked one |
| `y`/`n` | Confirm/cancel the pending action |
| `Esc` | Clear marks, then filter; closes a finished log pane |
| `?` | Toggle help |
| `q` | Quit (confirms first if an operation is running) |

**Operations**

Actions run as `tsi` subprocesses whose output streams into a log pane inside the UI —
the display never leaves the TUI. One runs at a time; a batch queues the rest and the
footer shows how many are waiting. The pane title shows a spinner while running and
`✔ done` / `✖ failed (exit N)` afterwards; scroll it with `PageUp`/`PageDown` and close
it with `Esc`.

When an operation finishes, the installed-package database *and* the package definitions
are reloaded, so a `tsi update` run from the TSI tab shows up in the list immediately.
The cursor stays on the package it was on, not on the index it happened to occupy.

**Removals that would break something**

The details panel lists `required by:` for an installed package. Pressing `r` on such a
package prompts with what the removal breaks — `Remove zlib 1.3 — breaks curl, git.
Force? y/N` — in the warning color, and only passes `--force` once you confirm.

Marking a whole dependency chain and pressing `r` needs no forcing: the batch is ordered
so dependents are removed before the packages they depend on. Only dependents left
installed outside the batch require a forced removal.

## Exit Codes

- `0` - Success
- `1` - Error (e.g., package not found, build failure)

## Environment Variables

TSI respects:

- `HOME` / `USERPROFILE` - For default prefix resolution
- `PATH` - For finding build tools
