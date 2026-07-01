# Sprite Pixel-Art Shading — Design

**Date:** 2026-07-01
**Status:** Approved (design), pending implementation plan
**Scope:** `tools/draw_assets.ts` only — all `SPR_*` sprites

## Problem

`tools/draw_assets.ts` generates every sprite as **wireframe line-art**: it draws
only outlines (`line`/`circle`/`poly`) plus a single accent line, with no filled
bodies, no shading, and no rim light. The result looks like a blueprint or a
programmer placeholder — an 80s vector look, not a 90s VGA shmup.

Meanwhile the generated palette (`tools/mkpalette.ts` → `SRC/PALETTE.H`) provides
**15 color ramps × 16 shade steps** plus 15 grayscale levels. None of that shading
depth is used by the current art.

## Goal

Rewrite the drawing method in `tools/draw_assets.ts` from outline-only to
**filled + cel-shaded pixel art** in a Raiden/Raptor military-craft style, using
the palette ramps for volume. Every other pipeline stage (`mksprite`, `mkimg`,
`convert.sh`, engine code in `SRC/`) is untouched.

## Invariants (must not change)

- **Frame dimensions, frame counts, and sheet layouts stay identical** to the
  current `draw_assets.ts` output. Only pixel *content* changes → **zero changes
  required in `SRC/`**.
- Transparent background stays index 0 (alpha 0).
- Output path stays `ASSETS/generated/*.png`; `convert.sh`/`mksprite` consume it
  unchanged.

## Non-Goals

- Backgrounds: `IMG_DSRT`, `IMG_RIVR`, procedural starfield, `drawBackground()`,
  `SPR_GRID`. (Deferred to a later pass.)
- Palette changes (`mkpalette.ts`).
- Engine / gameplay code in `SRC/`.
- New sprites, new animations, or changed sprite sizes.

## Visual Target

**Raiden / Raptor: Call of the Shadows** look:
- Solid metallic hull, cel-shaded with 3–5 shade steps from one ramp.
- Top-lit convention: lighter shades toward the top/leading edge, darker toward
  the bottom/trailing edge.
- A dark 1px rim around each silhouette for readability against dark backgrounds.
- One bright accent per craft: cockpit (Cyan/Sky ramp), engine glow
  (Orange/Yellow ramp), cannon/core (Red ramp).

## Architecture

All work lives inside `tools/draw_assets.ts`. Three layers:

### 1. Palette-locked color system

Replicate `mkpalette.ts`'s exact HSL ramp math so drawn colors land precisely on
palette indices — mksprite's nearest-color quantization becomes an identity map,
so on-screen output is fully predictable.

- Port the `RAMPS` table (Red, Orange, Yellow, Lime, Green, Teal, Cyan, Sky, Blue,
  Indigo, Purple, Magenta, Brown, Skin, Forest) and the same
  `l = 0.06 + (shade/15) * 0.88` lightness formula and `hslToRgb`.
- Expose `ramp(name: string, shade: 0..15): Color` returning the exact RGBA.
- Expose `gray(level: 0..15): Color` for the grayscale band.
- Keep `T` (transparent) as index 0.

### 2. Shading helper library (new primitives on `Img`)

- `fillPoly(points, color)` — scanline polygon fill (currently only outline
  `poly()` exists; fill is the core missing primitive).
- `fillShaded(points, rampName, {topLit, steps})` — fill the silhouette, then
  apply directional shading by mapping each pixel's vertical position (and optional
  horizontal bias) to a ramp shade band.
- `rim(points, darkColor)` — draw a 1px dark outline tracing the silhouette after
  fill, for background separation.
- `dither2x2(x, y, a, b)` — ordered 2×2 dither picking shade `a` or `b` by pixel
  parity, for smooth gradient bands between two ramp steps.
- `spec(x, y, color)` / `cockpit(cx, cy, r, rampName)` — bright highlight dot and
  canopy accent helpers.

The existing outline primitives (`line`, `poly`, `ellipse`, `circle`,
`thickLine`, `cross`, `fillRect`, `fillCircle`, `fillEllipse`) remain available
for detailing on top of the filled base.

### 3. Redrawn sprite functions (by archetype)

Reuse the current archetype grouping so shared code stays shared. Each function
follows the same pipeline: **filled polygon silhouette → directional shading →
dark rim → bright accent(s)**.

- `drawPlayer` (SPR_PSHP, 16×24, 5×2 grid) — banking frames keep current tilt/flame logic.
- `drawSmallShip` (SPR_EPOP/EFTR/EAF/ESS) — per-call accent ramp preserved.
- `drawRoundEnemy` (SPR_EBLU/EPB/EHB/ECI) — domed shaded body + core.
- `drawTurret` (SPR_ETUR, 5 aim frames).
- `drawLargeBoss` (SPR_EMC/EMV/EMS/SPR_BMG/SPR_BKL) — per-call accent ramp preserved.
- `drawBossTank` (SPR_BTKB) / `drawBossHead` (SPR_BTKH).
- `drawFinalBoss` (SPR_BFIN, 96×96) — fortress hull, reactor core, twin barrels.
- `drawExplosion` (SPR_EXPL, 5 frames) — filled expanding fireball: white-hot core →
  yellow → orange → smoke ring, per frame.
- `drawPowerUp` (SPR_PWUP, 15 frames) — shaded capsule with cycling accent.
- Bullets (`drawBullet` kinds 0–5, `drawRotatedLineBullet` for SPR_EBL) — filled
  saturated core + white specular + faint halo so they pop on dark backgrounds.

## Readability Rules (VGA / small-sprite)

Every sprite = **dark rim + 3–5 shade body + exactly one bright accent**. Keep
transparent background. Avoid single-pixel isolated details that disappear at
game scale; prefer 2px-minimum shapes.

## Execution Plan (inline sub-plan loop, visual verification, no auto-commit)

1. **Helper library + hero sprites.** Build the color system and shading helpers,
   then redraw three hero sprites — `SPR_PSHP`, `SPR_EBLU`, `SPR_BFIN`. Generate
   PNGs, montage at 8× with ImageMagick, and get user visual approval before
   proceeding.
2. **Roll out remaining sprites** archetype batch by batch (small ships → round
   enemies → turret → large bosses → tank/head → explosion/powerup → bullets).
   Montage-verify each batch.
3. **In-engine check.** Run `./build.sh` then `./run.sh` to confirm sprites render
   correctly in Mode 13h through the real quantization + compile path.

## Testing

- Per-batch: `bun tools/draw_assets.ts`, then `magick ... -filter point -resize 800%`
  montage, inspected visually.
- End-to-end: `./convert.sh` (via `./build.sh`) → `./run.sh` in DOSBox.
- No automated unit tests (project has none); validation is visual + build/run.

## Risks

- **Quantization drift** if drawn colors don't exactly match palette indices →
  mitigated by the palette-lock color system (layer 1).
- **Frame-size mismatch** would force `SRC/` changes → mitigated by the hard
  invariant of preserving all dimensions/counts/layouts.
- **Over-detail at small size** → mitigated by the 2px-minimum readability rule and
  per-batch visual verification.
