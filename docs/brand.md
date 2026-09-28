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
2. **One red per surface.** Forge Red on light, Ember on dark. Don't mix them.
3. **Status colors stay semantic.** Success is green, warnings are amber, errors
   are red. Because the brand is also red, an error is never signalled by color
   alone. It always carries its marker (`[XX]`, `✗`) or a label, so a reader
   can't mistake branding for a failure.

## In the documentation site

The palette is applied through Material's `custom` primary/accent colors in
`docs/stylesheets/tsi.css`, for both the light and dark schemes. The dark scheme
swaps Material's blue-tinted slate for Kiln, with a neutral warm hue (`--md-hue: 0`)
for the rest of its greys.

## In the terminal (proposed)

The CLI and TUI don't use the brand yet. Today the TUI accent is cyan
(`src/cli/ui/theme.rs`) and the CLI's `==>` section arrows are bold blue
(`src/ui/output.rs`). The proposed mapping:

| Role | Today | Proposed |
|---|---|---|
| TUI accent: focused border, selection, active tab, keys | Cyan | Brand red: 256-color index 160 (`#D70000`), falling back to ANSI red |
| CLI section arrow `==>`, build steps | Bold blue | Bold brand red |
| Success `[ok]` | Green | Green (unchanged) |
| Warning `[!!]` | Yellow | Yellow (unchanged) |
| Error `[XX]` | Red | Red, bold, and always with its marker (rule 3) |

The 256-color index is used instead of 24-bit color because it renders the same
on every terminal TSI targets, including ones without truecolor support.
