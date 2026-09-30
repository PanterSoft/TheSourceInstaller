# Brand & Colors

TSI's identity is **Forge Red** on warm neutrals. Red for a tool that takes raw
source and forges it into something installed; warm greys instead of cold blue
ones so the red sits in a consistent family rather than on top of it.

## The mark

<p><img src="../assets/logo.svg" alt="TSI mark" width="96" height="96"></p>

An arrow dropping into a tray: source goes in, an installed package comes out. It is
drawn from strokes only (no text, no font), so it stays legible down to a 16px
favicon. Use it on Forge Red as shown; on a red background use it white-on-red,
never red-on-red. The file is `docs/assets/logo.svg`.

## Palette

<div class="tsi-swatches" markdown>
<div class="tsi-swatch"><div class="tsi-swatch__chip" style="--c:#c62828"></div><div class="tsi-swatch__label"><strong>Forge Red</strong><code>#C62828</code><br>Primary. Header, logo, buttons.</div></div>
<div class="tsi-swatch"><div class="tsi-swatch__chip" style="--c:#8e1b1b"></div><div class="tsi-swatch__label"><strong>Forge Red Deep</strong><code>#8E1B1B</code><br>Dark-mode header, pressed states.</div></div>
<div class="tsi-swatch"><div class="tsi-swatch__chip" style="--c:#b71c1c"></div><div class="tsi-swatch__label"><strong>Link Red</strong><code>#B71C1C</code><br>Links on light backgrounds.</div></div>
<div class="tsi-swatch"><div class="tsi-swatch__chip" style="--c:#ff6b5e"></div><div class="tsi-swatch__label"><strong>Ember</strong><code>#FF6B5E</code><br>Red on dark backgrounds.</div></div>
<div class="tsi-swatch"><div class="tsi-swatch__chip" style="--c:#f7f4f3"></div><div class="tsi-swatch__label"><strong>Ash</strong><code>#F7F4F3</code><br>Warm light neutral.</div></div>
<div class="tsi-swatch"><div class="tsi-swatch__chip" style="--c:#1c1a1a"></div><div class="tsi-swatch__label"><strong>Kiln</strong><code>#1C1A1A</code><br>Warm dark neutral.</div></div>
</div>

### Contrast

Every pairing used for text meets WCAG AA (4.5:1):

| Foreground | Background | Ratio | Used for |
|---|---|---|---|
| White | Forge Red `#C62828` | 5.7:1 | Header text, button labels |
| White | Forge Red Deep `#8E1B1B` | 9.1:1 | Dark-mode header |
| Link Red `#B71C1C` | White | 6.6:1 | Links, light mode |
| Ember `#FF6B5E` | Kiln `#1C1A1A` | 6.2:1 | Links, dark mode |

Forge Red itself is *not* used for small text on white (it passes, but Link Red is
the darker, safer choice); Ember is never used on light backgrounds (it fails).

## Rules

1. **Red is the brand, not decoration.** It marks identity and primary actions:
   the header, the logo, links, the one primary button. Body text, tables and
   code stay neutral.
2. **One red per surface.** Forge Red on light, Ember on dark, Terminal Red in
   the terminal. Don't mix them.
3. **Status colors stay semantic.** Success is green, warnings are amber, errors
   are red. Because the brand is also red, an error is never signalled by color
   alone. It always carries its marker (`[XX]`, `✗`) or a label, so a reader
   can't mistake branding for a failure.

## In the documentation site

The palette is applied through Material's `custom` primary/accent colors in
`docs/stylesheets/tsi.css`, for both the light and dark schemes. The dark scheme
swaps Material's blue-tinted slate for Kiln, with a neutral warm hue (`--md-hue: 0`)
for the rest of its greys.

## In the terminal

A terminal can't tell TSI whether its background is light or dark, so the
terminal uses one red that works on both: **Terminal Red**, xterm-256 color
`167` (`#D75F5F`), 5.7:1 on black and 3.7:1 on white (enough for the bold
and non-text uses it has). It is defined once in `src/ui/palette.rs` and
shared by the CLI and the TUI.

<div class="tsi-swatches" markdown>
<div class="tsi-swatch"><div class="tsi-swatch__chip" style="--c:#d75f5f"></div><div class="tsi-swatch__label"><strong>Terminal Red</strong><code>167 · #D75F5F</code><br>Brand accent in the terminal.</div></div>
<div class="tsi-swatch"><div class="tsi-swatch__chip" style="--c:#af5f5f"></div><div class="tsi-swatch__label"><strong>Terminal Red Deep</strong><code>131 · #AF5F5F</code><br>Filled part of progress bars.</div></div>
<div class="tsi-swatch"><div class="tsi-swatch__chip" style="--c:#444444"></div><div class="tsi-swatch__label"><strong>Track</strong><code>238 · #444444</code><br>Unfilled part of progress bars.</div></div>
</div>

| Where | Role | Color |
|---|---|---|
| CLI | Section and build-step arrows `==>`, step arrows `->` | Terminal Red (bold for `==>`) |
| CLI | Spinners; progress bars | Terminal Red; Terminal Red Deep on Track |
| CLI | Success `[ok]` / warning `[!!]` | Green / yellow |
| CLI | Error `[XX]` | Bright red, bold |
| TUI | ` tsi ` badge at the left of the tab bar | White (231) on Terminal Red, bold |
| TUI | Focused border, selection, active tab, keys, running spinner | Terminal Red |
| TUI | Failed operations, destructive actions | Bright red, bold |
| Installer | `[INFO]` / `[WARN]` / `[ERROR]` | Terminal Red / yellow / bright red, bold |

Errors use the terminal's *bright* red in bold, a different and stronger red than
the brand's, and always keep their marker (rule 3). Color is only emitted to a
terminal: piped output stays plain, and the installer honors
[`NO_COLOR`](https://no-color.org). xterm-256 indices are used instead of 24-bit
color because they render the same on every terminal TSI targets, including ones
without truecolor support.

## In a GUI, or anything new

Every color above is also in a machine-readable file,
[`docs/assets/brand/tokens.json`](assets/brand/tokens.json), in the
[W3C Design Tokens](https://tr.designtokens.org/format/) format. It's published with
this site, so any tool can fetch it from
`https://pantersoft.github.io/TheSourceInstaller/assets/brand/tokens.json`. A
future GUI (or a website, installer screen, or icon set) should start from it:

- `brand.*` and `neutral.*` are the raw palette.
- `theme.light.*` and `theme.dark.*` map it to roles (background, surface, text,
  primary, link), so a GUI can switch schemes by switching one group.
- `status.*` holds success, warning and error for light and dark. All pass WCAG AA
  on their theme's background. Error is red like the brand, so rule 3 applies:
  pair it with an icon or label.
- `terminal.*` holds the xterm-256 indices used by the CLI and TUI.

When a color changes, change it in `tokens.json`, `docs/stylesheets/tsi.css` and
`src/ui/palette.rs` together.
