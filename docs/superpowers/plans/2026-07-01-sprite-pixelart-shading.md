# Sprite Pixel-Art Shading Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. This project has NO automated test suite — each task's "test cycle" is: regenerate PNGs, montage at 8× with ImageMagick, and inspect visually. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the wireframe line-art in `tools/draw_assets.ts` with filled, cel-shaded Raiden/Raptor-style pixel art for all `SPR_*` sprites, without touching any other pipeline stage or engine code.

**Architecture:** Add a palette-locked color system (replicating `mkpalette.ts`'s HSL ramp math so drawn pixels land exactly on palette indices) plus a shading-helper layer (`fillPoly`, `fillShaded`, `rim`, `dither2x2`, `spec`, `cockpit`) onto the existing `Img` class. Then rewrite each archetype `draw*` function to the pipeline: filled silhouette → directional shading → dark rim → bright accent. Frame sizes/counts/layouts are preserved so `SRC/` needs no changes.

**Tech Stack:** Bun + TypeScript (host-side asset tool), ImageMagick (`magick`) for montage inspection, existing `mksprite.ts` quantizer, DOSBox + Watcom for the final in-engine check.

## Global Constraints

- **Preserve every frame dimension, frame count, and sheet layout** exactly as the current `draw_assets.ts` produces them. Zero changes to `SRC/`.
- Transparent background = index 0 (RGBA alpha 0). Never emit a non-transparent pixel where transparency is intended.
- All non-transparent colors must come from the palette-locked `ramp()`/`gray()` helpers (Task 1) so mksprite quantization is an identity map.
- Readability rule: every sprite = dark rim + 3–5 shade body + exactly one bright accent; avoid isolated 1px details (2px minimum shapes).
- Output stays `ASSETS/generated/*.png`. Do NOT modify `mksprite.ts`, `mkpalette.ts`, `convert.sh`, or engine source.
- **No auto-commit.** Commit only when the user explicitly approves a batch. (Per project preference: inline sub-plan loop with visual verification.)
- Visual verification command (reuse throughout):
  ```bash
  SC=/private/tmp/claude-501/-Users-gcjjyy-lab-watcom-game-dev/0a487d47-acbc-4235-892e-02826957a950/scratchpad
  bun tools/draw_assets.ts
  magick ASSETS/generated/<NAME>.png -background '#202020' -flatten -filter point -resize 800% "$SC/<NAME>_big.png"
  # then Read the PNG to inspect
  ```

---

### Task 1: Palette-locked color system + shading helpers

**Files:**
- Modify: `tools/draw_assets.ts` (add color system near top; add methods to `Img` class ~line 30–140; keep existing primitives)

**Interfaces:**
- Consumes: nothing (foundation task).
- Produces (used by every later task):
  - `ramp(name: string, shade: number): Color` — `name` ∈ the 15 ramp names; `shade` 0..15 (0 darkest). Returns exact palette RGBA `[r,g,b,255]`.
  - `gray(level: number): Color` — `level` 0..15.
  - `Img.fillPoly(points: [number,number][], c: Color, ox=0, oy=0): void`
  - `Img.fillShaded(points: [number,number][], name: string, opts: {lo:number, hi:number, ox?:number, oy?:number, horiz?:boolean}): void` — fills the polygon, shading each pixel from `hi` (top / or left if `horiz`) to `lo` (bottom/right) across the shape's bounding span.
  - `Img.rim(points: [number,number][], c: Color, ox=0, oy=0): void` — outline (delegates to existing `poly`).
  - `Img.dither2x2(x:number, y:number, a: Color, b: Color): Color` — returns `a` or `b` by `(x+y)&1`.
  - `Img.spec(x:number, y:number, c: Color): void` — single highlight pixel (bounds-checked `set`).
  - `Img.cockpit(cx:number, cy:number, rx:number, ry:number, name:string): void` — filled bright canopy: `fillEllipse` with `ramp(name,11)`, top-half `spec` with `ramp(name,14)`.

- [ ] **Step 1: Port the ramp math (palette lock)**

Add after the `Color`/constant block (keep existing named constants; new code can live alongside). This mirrors `tools/mkpalette.ts` exactly:

```ts
function hslToRgb(h: number, s: number, l: number): [number, number, number] {
  h = ((h % 360) + 360) % 360;
  const c = (1 - Math.abs(2 * l - 1)) * s;
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
  const m = l - c / 2;
  let r = 0, g = 0, b = 0;
  if (h < 60)       { r = c; g = x; }
  else if (h < 120) { r = x; g = c; }
  else if (h < 180) { g = c; b = x; }
  else if (h < 240) { g = x; b = c; }
  else if (h < 300) { r = x; b = c; }
  else              { r = c; b = x; }
  return [Math.round((r + m) * 255), Math.round((g + m) * 255), Math.round((b + m) * 255)];
}

const RAMPS: Record<string, { h: number; s: number }> = {
  Red: { h: 0, s: 0.85 }, Orange: { h: 25, s: 0.85 }, Yellow: { h: 50, s: 0.85 },
  Lime: { h: 80, s: 0.80 }, Green: { h: 120, s: 0.80 }, Teal: { h: 160, s: 0.70 },
  Cyan: { h: 185, s: 0.75 }, Sky: { h: 210, s: 0.75 }, Blue: { h: 240, s: 0.80 },
  Indigo: { h: 265, s: 0.70 }, Purple: { h: 285, s: 0.70 }, Magenta: { h: 320, s: 0.70 },
  Brown: { h: 30, s: 0.50 }, Skin: { h: 20, s: 0.40 }, Forest: { h: 100, s: 0.40 },
};

function ramp(name: string, shade: number): Color {
  const r = RAMPS[name];
  const l = 0.06 + (Math.max(0, Math.min(15, shade)) / 15) * 0.88;
  const [rr, gg, bb] = hslToRgb(r.h, r.s, l);
  return [rr, gg, bb, 255];
}

function gray(level: number): Color {
  const v = Math.round((Math.max(0, Math.min(15, level)) / 15) * 255);
  return [v, v, v, 255];
}
```

- [ ] **Step 2: Add fill + shading primitives to `Img`**

Add these methods inside the `Img` class (after `fillEllipse`/`cross`):

```ts
  fillPoly(points: [number, number][], c: Color, ox = 0, oy = 0) {
    const pts = points.map(([x, y]) => [x + ox, y + oy] as [number, number]);
    let minY = Infinity, maxY = -Infinity;
    for (const [, y] of pts) { minY = Math.min(minY, y); maxY = Math.max(maxY, y); }
    minY = Math.max(0, Math.floor(minY)); maxY = Math.min(this.h - 1, Math.ceil(maxY));
    for (let y = minY; y <= maxY; y++) {
      const xs: number[] = [];
      for (let i = 0; i < pts.length; i++) {
        const a = pts[i], b = pts[(i + 1) % pts.length];
        const [ax, ay] = a, [bx, by] = b;
        if ((ay <= y && by > y) || (by <= y && ay > y)) {
          xs.push(ax + ((y - ay) / (by - ay)) * (bx - ax));
        }
      }
      xs.sort((p, q) => p - q);
      for (let i = 0; i + 1 < xs.length; i += 2) {
        for (let x = Math.round(xs[i]); x <= Math.round(xs[i + 1]); x++) this.set(x, y, c);
      }
    }
  }

  fillShaded(points: [number, number][], name: string,
             opts: { lo: number; hi: number; ox?: number; oy?: number; horiz?: boolean }) {
    const ox = opts.ox ?? 0, oy = opts.oy ?? 0;
    const pts = points.map(([x, y]) => [x + ox, y + oy] as [number, number]);
    let minY = Infinity, maxY = -Infinity, minX = Infinity, maxX = -Infinity;
    for (const [x, y] of pts) { minY = Math.min(minY, y); maxY = Math.max(maxY, y); minX = Math.min(minX, x); maxX = Math.max(maxX, x); }
    const y0 = Math.max(0, Math.floor(minY)), y1 = Math.min(this.h - 1, Math.ceil(maxY));
    const span = (opts.horiz ? maxX - minX : maxY - minY) || 1;
    for (let y = y0; y <= y1; y++) {
      const xs: number[] = [];
      for (let i = 0; i < pts.length; i++) {
        const a = pts[i], b = pts[(i + 1) % pts.length];
        const [ax, ay] = a, [bx, by] = b;
        if ((ay <= y && by > y) || (by <= y && ay > y)) xs.push(ax + ((y - ay) / (by - ay)) * (bx - ax));
      }
      xs.sort((p, q) => p - q);
      for (let i = 0; i + 1 < xs.length; i += 2) {
        for (let x = Math.round(xs[i]); x <= Math.round(xs[i + 1]); x++) {
          const t = opts.horiz ? (x - minX) / span : 1 - (y - minY) / span; // hi at top/left
          const shade = Math.round(opts.lo + t * (opts.hi - opts.lo));
          this.set(x, y, ramp(name, shade));
        }
      }
    }
  }

  rim(points: [number, number][], c: Color, ox = 0, oy = 0) {
    this.poly(points, c, ox, oy);
  }

  spec(x: number, y: number, c: Color) { this.set(x, y, c); }

  cockpit(cx: number, cy: number, rx: number, ry: number, name: string) {
    this.fillEllipse(cx, cy, rx, ry, ramp(name, 11));
    this.fillEllipse(cx, cy - Math.max(1, Math.floor(ry / 2)), Math.max(1, rx - 1), Math.max(1, Math.floor(ry / 2)), ramp(name, 14));
  }
```

(Note: `dither2x2` is a pure helper — add as a free function since it takes no `Img` state:)

```ts
function dither2x2(x: number, y: number, a: Color, b: Color): Color {
  return ((x + y) & 1) === 0 ? a : b;
}
```

- [ ] **Step 3: Smoke-test the helpers with a scratch render**

Temporarily append at the bottom of `draw_assets.ts` (remove after verifying):

```ts
sheet("ZTEST", 40, 40, 1, (img, ox, oy) => {
  img.fillShaded([[20, 2], [4, 36], [36, 36]], "Sky", { lo: 3, hi: 13, ox, oy });
  img.rim([[20, 2], [4, 36], [36, 36]], ramp("Sky", 1), ox, oy);
  img.cockpit(ox + 20, oy + 18, 5, 6, "Cyan");
});
```

Run:
```bash
SC=/private/tmp/claude-501/-Users-gcjjyy-lab-watcom-game-dev/0a487d47-acbc-4235-892e-02826957a950/scratchpad
bun tools/draw_assets.ts
magick ASSETS/generated/ZTEST.png -filter point -resize 800% "$SC/ztest_big.png"
```
Expected: a triangle filled with a smooth Sky top→dark gradient, a dark outline, and a bright cyan canopy. Read `$SC/ztest_big.png` to confirm.

- [ ] **Step 4: Remove the ZTEST scratch block**

Delete the `sheet("ZTEST", ...)` block. Re-run `bun tools/draw_assets.ts` and confirm it exits cleanly with the normal "Generated wireframe PNG assets" message (message text can be updated to "Generated shaded pixel-art assets" now).

- [ ] **Step 5: Commit (only if user approves)**

```bash
git add tools/draw_assets.ts
git commit -m "draw_assets: add palette-locked color system + shading helpers"
```

---

### Task 2: Hero sprites (player, round enemy, final boss) — STYLE GATE

**Files:**
- Modify: `tools/draw_assets.ts` — rewrite `drawPlayer` (~205), `drawRoundEnemy` (~227), `drawFinalBoss` (~287).

**Interfaces:**
- Consumes: `ramp`, `gray`, `fillShaded`, `fillPoly`, `rim`, `spec`, `cockpit`, `dither2x2` (Task 1).
- Produces: the confirmed house style (shade counts, rim shade, accent placement) that Tasks 3–9 copy.

This is the **visual approval gate**. Do not proceed to Task 3 until the user approves these three.

- [ ] **Step 1: Rewrite `drawPlayer` (SPR_PSHP, 16×24, frames 0/2/4/5/7/9 used)**

Recipe — an interceptor pointing up: hull is a `Sky` filled dart, top-lit; dark `Blue` rim; `Cyan` canopy; twin `Orange`→`Yellow` engine glow at the tail; banking frames shift `tilt` as today.

```ts
function drawPlayer(img: Img, ox: number, oy: number, f: number) {
  const used = [0, 2, 4, 5, 7, 9];
  if (!used.includes(f)) return;
  const tilt = f === 0 || f === 5 ? -2 : (f === 4 || f === 9 ? 2 : 0);
  const flame = f >= 5 ? 3 : 0;
  const hull: [number, number][] = [[8 + tilt, 1], [5, 9], [2, 19], [6, 16], [8, 18], [10, 16], [14, 19], [11, 9]];
  img.fillShaded(hull, "Sky", { lo: 4, hi: 13, ox, oy });
  img.rim(hull, ramp("Blue", 1), ox, oy);
  // wings
  img.fillShaded([[2, 12], [0, 18], [5, 16]], "Blue", { lo: 4, hi: 10, ox, oy });
  img.fillShaded([[14, 12], [16, 18], [11, 16]], "Blue", { lo: 4, hi: 10, ox, oy });
  img.cockpit(ox + 8 + Math.round(tilt / 2), oy + 8, 2, 3, "Cyan");
  // engine glow
  img.set(ox + 7, oy + 20, ramp("Orange", 12));
  img.set(ox + 8, oy + 20 + Math.min(3, flame), ramp("Yellow", 14));
  img.set(ox + 9, oy + 20, ramp("Orange", 12));
}
```

- [ ] **Step 2: Rewrite `drawRoundEnemy` (SPR_EBLU/EPB/EHB/ECI, accent ramp param)**

The accent param currently comes in as a `Color`. Change the call sites (Task 4) to pass a ramp *name* string instead. For hero verification, `SPR_EBLU` uses `"Sky"`. Recipe — a domed saucer: shaded ellipse body, dark rim, glowing core (accent ramp), two small underslung fins.

```ts
function drawRoundEnemy(img: Img, ox: number, oy: number, w: number, h: number, f: number, accent: string) {
  const cx = ox + Math.floor(w / 2), cy = oy + Math.floor(h / 2);
  const rx = w / 2 - 2, ry = h / 2 - 3;
  // shaded dome (top-lit) via horizontal bands of the Teal hull ramp
  for (let y = -Math.ceil(ry); y <= Math.ceil(ry); y++) {
    const t = 1 - (y + ry) / (2 * ry); // 1 at top
    const shade = Math.round(3 + t * 9);
    const half = Math.floor(rx * Math.sqrt(Math.max(0, 1 - (y / ry) ** 2)));
    for (let x = -half; x <= half; x++) img.set(cx + x, cy + y, ramp("Teal", shade));
  }
  img.ellipse(cx, cy, rx, ry, ramp("Teal", 1));                // dark rim
  const cr = Math.max(3, Math.min(w, h) / 5 + (f & 1));
  img.fillCircle(cx, cy, cr, ramp(accent, 12));                // glowing core
  img.circle(cx, cy, cr, ramp(accent, 15));
  img.spec(cx - 1, cy - 1, ramp(accent, 15));
  img.fillPoly([[cx - 5, cy + Math.floor(ry) - 1], [cx - 8, cy + Math.floor(ry) + 4], [cx - 3, cy + Math.floor(ry)]], ramp("Teal", 5));
  img.fillPoly([[cx + 5, cy + Math.floor(ry) - 1], [cx + 8, cy + Math.floor(ry) + 4], [cx + 3, cy + Math.floor(ry)]], ramp("Teal", 5));
}
```

- [ ] **Step 3: Rewrite `drawFinalBoss` (SPR_BFIN, 96×96)**

Recipe — an angular fortress hull (Indigo, top-lit), armor band highlights, two `Brown`/`gray` side cannon pods, concentric reactor rings ending in a pulsing `Red` core, twin down-barrels. Keep overall silhouette proportions.

```ts
function drawFinalBoss(img: Img, ox: number, oy: number, f: number) {
  const cx = ox + 48, cy = oy + 48;
  const hull: [number, number][] = [[48, 93], [4, 22], [20, 6], [48, 16], [76, 6], [92, 22]];
  img.fillShaded(hull, "Indigo", { lo: 3, hi: 12, ox, oy });
  img.rim(hull, ramp("Indigo", 0), ox, oy);
  img.line(ox + 14, oy + 24, ox + 82, oy + 24, ramp("Indigo", 9));   // armor band highlight
  img.line(ox + 20, oy + 14, ox + 76, oy + 14, ramp("Indigo", 10));
  for (const px of [15, 81]) {                                        // cannon pods
    img.fillCircle(ox + px, oy + 44, 9, ramp("Brown", 6));
    img.circle(ox + px, oy + 44, 9, gray(3));
    img.spec(ox + px - 2, oy + 41, gray(11));
  }
  img.fillCircle(cx, cy, 24, ramp("Indigo", 4));                      // reactor housing
  img.circle(cx, cy, 24, gray(2));
  img.fillCircle(cx, cy, 16, gray(4));
  img.fillCircle(cx, cy, 9, ramp("Red", 11 + (f & 1)));               // pulsing core
  img.circle(cx, cy, 9, ramp("Red", 15));
  img.cross(cx, cy, 11, ramp("Red", 14));
  for (const bx of [-10, 10]) {                                       // twin barrels
    img.fillRect(cx + bx - 1, oy + 70, 3, 20, gray(6));
    img.line(cx + bx - 1, oy + 70, cx + bx - 1, oy + 90, gray(10));
  }
}
```

- [ ] **Step 4: Update the two `drawRoundEnemy` hero call sites to pass ramp names**

For now update only `SPR_EBLU` (line ~397) to `"Sky"`; the other round-enemy call sites are handled in Task 4. To keep the file compiling, temporarily pass a name to all four `drawRoundEnemy` call sites (EBLU→"Sky", EPB→"Red", EHB→"Orange", ECI→"Magenta").

- [ ] **Step 5: Render + montage the heroes**

```bash
SC=/private/tmp/claude-501/-Users-gcjjyy-lab-watcom-game-dev/0a487d47-acbc-4235-892e-02826957a950/scratchpad
bun tools/draw_assets.ts
for f in SPR_PSHP SPR_EBLU SPR_BFIN; do magick ASSETS/generated/$f.png -background '#202020' -flatten -filter point -resize 800% "$SC/${f}_big.png"; done
magick "$SC"/SPR_PSHP_big.png "$SC"/SPR_EBLU_big.png -background '#202020' -append "$SC/hero_small.png"
```
Read `$SC/hero_small.png` and `$SC/SPR_BFIN_big.png`.

- [ ] **Step 6: USER VISUAL APPROVAL GATE**

Present the three renders to the user. Confirm the house style (shading depth, rim contrast, accent brightness) reads well at game scale before rolling out. Iterate on coordinates/shades until approved. **Do not commit or proceed until approved.**

- [ ] **Step 7: Commit (only after user approves)**

```bash
git add tools/draw_assets.ts
git commit -m "draw_assets: shaded pixel-art for hero sprites (player/round enemy/final boss)"
```

---

### Task 3: Small ships (SPR_EPOP, EFTR, EAF, ESS)

**Files:** Modify `tools/draw_assets.ts` — `drawSmallShip` (~217) and its 4 call sites (~395,396,400,401).

**Interfaces:** Consumes Task 1 helpers + Task 2 house style. Change `drawSmallShip`'s `accent: Color` param to `accent: string` (ramp name); update call sites: EPOP→"Red", EFTR→"Orange", EAF→"Yellow", ESS→"Magenta".

- [ ] **Step 1: Rewrite `drawSmallShip`** — filled arrowhead hull in the accent ramp (top-lit `lo:4,hi:12`), dark rim (`ramp(accent,1)`), `gray` cockpit dot, small `Orange` engine spark at tail; `wing` still animates by `f&1`.

```ts
function drawSmallShip(img: Img, ox: number, oy: number, w: number, h: number, f: number, accent: string) {
  const cx = ox + Math.floor(w / 2);
  const wing = 3 + (f & 1);
  const hull: [number, number][] = [[w / 2, h - 3], [3, 4 + wing], [w / 2, 9], [w - 4, 4 + wing]];
  img.fillShaded(hull, accent, { lo: 4, hi: 12, ox, oy });
  img.rim(hull, ramp(accent, 1), ox, oy);
  img.fillEllipse(cx, oy + 8, 2, 2, gray(11));               // cockpit
  img.set(cx, oy + h - 4, ramp("Orange", 13));               // engine spark
  img.spec(cx - 1, oy + 6, ramp(accent, 15));
}
```

- [ ] **Step 2: Render + montage** the four sheets (`SPR_EPOP EPFTR? use EFTR EAF ESS`):
```bash
SC=/private/tmp/claude-501/-Users-gcjjyy-lab-watcom-game-dev/0a487d47-acbc-4235-892e-02826957a950/scratchpad
bun tools/draw_assets.ts
for f in SPR_EPOP SPR_EFTR SPR_EAF SPR_ESS; do magick ASSETS/generated/$f.png -background '#202020' -flatten -filter point -resize 800% "$SC/${f}_big.png"; done
magick "$SC"/SPR_EPOP_big.png "$SC"/SPR_EFTR_big.png "$SC"/SPR_EAF_big.png "$SC"/SPR_ESS_big.png -background '#202020' -append "$SC/smallships.png"
```
Read `$SC/smallships.png`, iterate until clean.

- [ ] **Step 3: Commit (on approval)** — `git commit -m "draw_assets: shaded small-ship enemies"`

---

### Task 4: Remaining round enemies (SPR_EPB, EHB, ECI)

**Files:** Modify `tools/draw_assets.ts` — call sites only (`drawRoundEnemy` already rewritten in Task 2). Confirm names: EPB→"Red", EHB→"Orange", ECI→"Magenta" (EBLU done in Task 2).

- [ ] **Step 1:** Verify the three call sites pass ramp-name strings (set in Task 2 Step 4). No new function code.
- [ ] **Step 2: Render + montage** `SPR_EPB SPR_EHB SPR_ECI` (same montage pattern as Task 3 Step 2). Read and confirm the accent glow reads per color.
- [ ] **Step 3: Commit (on approval)** — `git commit -m "draw_assets: shaded round enemies EPB/EHB/ECI"`

---

### Task 5: Turret (SPR_ETUR, 27×20, 5 aim frames)

**Files:** Modify `tools/draw_assets.ts` — `drawTurret` (~238).

- [ ] **Step 1: Rewrite** — a `gray` armored base (shaded box), a domed `Brown` mount, and a `Red` barrel that swings with `dx` per frame; bright muzzle tip.

```ts
function drawTurret(img: Img, ox: number, oy: number, f: number) {
  const cx = ox + 13, cy = oy + 11;
  const dx = [-5, -2, 0, 2, 5][f];
  img.fillShaded([[2, 10], [2, 17], [24, 17], [24, 10]], "Brown", { lo: 3, hi: 9, ox, oy, horiz: false });
  img.rim([[2, 10], [2, 17], [24, 17], [24, 10]], gray(2), ox, oy);
  img.fillEllipse(cx, cy, 8, 6, gray(6));
  img.ellipse(cx, cy, 8, 6, gray(2));
  img.spec(cx - 3, cy - 2, gray(11));
  img.thickLine(cx, cy, cx + dx, oy + 19, ramp("Red", 8), 2);
  img.set(cx + dx, oy + 19, ramp("Yellow", 14));              // muzzle
  img.fillCircle(cx, cy, 3, gray(9));
}
```

- [ ] **Step 2: Render + montage** `SPR_ETUR` (8× resize). Read and confirm barrel sweep is legible across 5 frames.
- [ ] **Step 3: Commit (on approval)** — `git commit -m "draw_assets: shaded turret"`

---

### Task 6: Large bosses (SPR_EMC, EMV, EMS, BMG, BKL)

**Files:** Modify `tools/draw_assets.ts` — `drawLargeBoss` (~276) and its 5 call sites (~398,404,405,413,414). Change `accent: Color` → `accent: string`; names: EMC→"Cyan", EMV→"Green", EMS→"Orange", BMG→"Cyan", BKL→"Magenta".

- [ ] **Step 1: Rewrite `drawLargeBoss`** — a broad multi-point capital hull (`Teal`, top-lit), armor-band highlight line, central `gray` bridge, a pulsing accent core (cross), twin underslung fins.

```ts
function drawLargeBoss(img: Img, ox: number, oy: number, w: number, h: number, f: number, accent: string) {
  const cx = ox + Math.floor(w / 2), cy = oy + Math.floor(h / 2);
  const hull: [number, number][] = [[w / 2, h - 3], [6, 15], [w / 4, 5], [w / 2, 12], [w * 3 / 4, 5], [w - 7, 15]];
  img.fillShaded(hull, "Teal", { lo: 3, hi: 11, ox, oy });
  img.rim(hull, ramp("Teal", 0), ox, oy);
  img.line(ox + 12, oy + 18, ox + w - 13, oy + 18, ramp("Teal", 9));
  img.fillEllipse(cx, cy, Math.floor(w / 5), Math.floor(h / 5), gray(5));
  img.ellipse(cx, cy, Math.floor(w / 5), Math.floor(h / 5), gray(2));
  const cr = 6 + (f % 3);
  img.fillCircle(cx, cy, cr, ramp(accent, 12));
  img.cross(cx, cy, cr + 1, ramp(accent, 15));
  img.fillPoly([[cx, oy + h - 8], [cx - 8, oy + h - 22], [cx - 3, oy + h - 8]], ramp("Teal", 6));
  img.fillPoly([[cx, oy + h - 8], [cx + 8, oy + h - 22], [cx + 3, oy + h - 8]], ramp("Teal", 6));
}
```

- [ ] **Step 2: Update the 5 call sites** to pass ramp-name strings (see names above).
- [ ] **Step 3: Render + montage** `SPR_EMC SPR_EMV SPR_EMS SPR_BMG SPR_BKL` (append vertically). Read and confirm each accent color + silhouette reads.
- [ ] **Step 4: Commit (on approval)** — `git commit -m "draw_assets: shaded large bosses"`

---

### Task 7: Tank boss + head (SPR_BTKB 70×70 ×3, SPR_BTKH 70×70 ×1)

**Files:** Modify `tools/draw_assets.ts` — `drawBossTank` (~258), `drawBossHead` (~268).

- [ ] **Step 1: Rewrite `drawBossTank`** — a `Brown` armored chassis (shaded box), `gray` inner deck, row of `gray` rivets animating with `f`, central `Red` main gun pointing down, two `Yellow` side muzzles.

```ts
function drawBossTank(img: Img, ox: number, oy: number, f: number) {
  img.fillShaded([[8, 10], [8, 58], [62, 58], [62, 10]], "Brown", { lo: 3, hi: 9, ox, oy });
  img.rim([[8, 10], [8, 58], [62, 58], [62, 10]], gray(2), ox, oy);
  img.fillRect(ox + 15, oy + 18, 40, 28, ramp("Brown", 5));
  img.rect(ox + 15, oy + 18, 40, 28, gray(2));
  for (let i = 0; i < 5; i++) img.fillCircle(ox + 16 + i * 10, oy + 11, 2 + ((i + f) & 1), gray(9));
  img.fillRect(ox + 33, oy + 44, 4, 24, gray(6));                 // main gun
  img.set(ox + 35, oy + 67, ramp("Red", 13));
  img.thickLine(ox + 20, oy + 44, ox + 8, oy + 56, ramp("Yellow", 12), 2);
  img.thickLine(ox + 50, oy + 44, ox + 62, oy + 56, ramp("Yellow", 12), 2);
}
```

- [ ] **Step 2: Rewrite `drawBossHead`** — a `gray` armored turret head, shaded dome, `Red` barrel, twin `Yellow` side vents.

```ts
function drawBossHead(img: Img, ox: number, oy: number) {
  const hull: [number, number][] = [[35, 13], [9, 35], [35, 57], [61, 35]];
  img.fillShaded(hull, "Brown", { lo: 3, hi: 10, ox, oy });
  img.rim(hull, gray(2), ox, oy);
  img.fillRect(ox + 24, oy + 25, 22, 18, gray(6));
  img.rect(ox + 24, oy + 25, 22, 18, gray(2));
  img.thickLine(ox + 35, oy + 40, ox + 35, oy + 66, ramp("Red", 9), 2);
  img.thickLine(ox + 20, oy + 36, ox + 8, oy + 46, ramp("Yellow", 12), 2);
  img.thickLine(ox + 50, oy + 36, ox + 62, oy + 46, ramp("Yellow", 12), 2);
}
```

- [ ] **Step 3: Render + montage** `SPR_BTKB SPR_BTKH`. Read and confirm.
- [ ] **Step 4: Commit (on approval)** — `git commit -m "draw_assets: shaded tank boss + head"`

---

### Task 8: Explosion + power-up (SPR_EXPL 16×16 ×5, SPR_PWUP 11×27 ×15)

**Files:** Modify `tools/draw_assets.ts` — `drawExplosion` (~248), `drawPowerUp` (~316).

- [ ] **Step 1: Rewrite `drawExplosion`** — an expanding filled fireball: white-hot core → `Yellow` → `Orange` shell → dark smoke ring as `f` grows.

```ts
function drawExplosion(img: Img, ox: number, oy: number, f: number) {
  const cx = ox + 8, cy = oy + 8;
  const r = 2 + f * 2.6;
  if (f < 4) img.fillCircle(cx, cy, Math.min(7, r + 1), ramp("Orange", 8 - f));
  img.fillCircle(cx, cy, Math.min(6, r), ramp("Yellow", 12 - f * 2));
  if (f < 3) { img.fillCircle(cx, cy, Math.max(1, 4 - f), gray(14)); }   // white-hot core
  if (f >= 3) img.circle(cx, cy, Math.min(7, r), ramp("Orange", 3));      // smoke ring
}
```

- [ ] **Step 2: Rewrite `drawPowerUp`** — a shaded `gray` capsule with a cycling accent letter-glow (`Cyan`/`Yellow`/`Green` by `f%3`), bright rim spark.

```ts
function drawPowerUp(img: Img, ox: number, oy: number, f: number) {
  const cy = oy + 13;
  const acc = ["Cyan", "Yellow", "Green"][f % 3];
  img.fillEllipse(ox + 5, cy, 4, 10, gray(5));
  img.ellipse(ox + 5, cy, 4, 10, gray(2));
  img.spec(ox + 3, cy - 5, gray(12));
  img.fillCircle(ox + 5, cy, 3 + (f % 3), ramp(acc, 12));
  img.circle(ox + 5, cy, 3 + (f % 3), ramp(acc, 15));
}
```

- [ ] **Step 3: Render + montage** `SPR_EXPL SPR_PWUP`. Read and confirm the explosion animates hot→smoke and the capsule pulses.
- [ ] **Step 4: Commit (on approval)** — `git commit -m "draw_assets: shaded explosion + power-up"`

---

### Task 9: Bullets (SPR_EBT, EBM, EBP, EBS, EBL, EBX)

**Files:** Modify `tools/draw_assets.ts` — `drawBullet` (~331) and `drawRotatedLineBullet` (~356). Change `drawBullet`'s `color: Color` param to `name: string` (ramp name); update 5 call sites (~417–420,422): EBT→"Yellow", EBM→"Cyan", EBP→"Magenta", EBS→"Red", EBX→"Red".

- [ ] **Step 1: Rewrite `drawBullet`** — saturated filled body (`ramp(name,10)`), bright rim (`ramp(name,15)`), white-hot center `spec`, faint 1px halo for pop. Preserve per-kind size behavior.

```ts
function drawBullet(img: Img, ox: number, oy: number, w: number, h: number, f: number, name: string, kind: number) {
  const cx = ox + Math.floor(w / 2), cy = oy + Math.floor(h / 2);
  const rByKind = [2 + (f & 1), 3, 4, 3, 0, Math.floor(Math.min(w, h) / 2) - 1];
  if (kind === 4) {                       // vertical bolt
    img.thickLine(cx, oy, cx, oy + h - 1, ramp(name, 10), 2);
    img.line(cx, oy, cx, oy + h - 1, ramp(name, 15));
    img.spec(cx, cy, gray(15));
    return;
  }
  const r = rByKind[kind];
  img.fillCircle(cx, cy, r, ramp(name, 10));
  img.circle(cx, cy, r, ramp(name, 15));
  img.spec(cx, cy, gray(15));
  if (kind === 5) img.spec(cx - 1, cy - 1, gray(13));
}
```

- [ ] **Step 2: Rewrite `drawRotatedLineBullet` (SPR_EBL)** — a rotating shaded rod: `Orange` body, `Yellow` bright center.

```ts
function drawRotatedLineBullet(img: Img, ox: number, oy: number, f: number) {
  const cx = ox + 7, cy = oy + 7;
  const a = (Math.PI * 2 * f) / 16;
  const dx = Math.cos(a) * 5, dy = Math.sin(a) * 5;
  img.thickLine(cx - dx, cy - dy, cx + dx, cy + dy, ramp("Orange", 9), 3);
  img.line(cx - dx, cy - dy, cx + dx, cy + dy, ramp("Orange", 14));
  img.fillCircle(cx, cy, 2, ramp("Yellow", 14));
}
```

- [ ] **Step 3: Update the 5 `drawBullet` call sites** to pass ramp-name strings (names above).
- [ ] **Step 4: Render + montage** `SPR_EBT SPR_EBM SPR_EBP SPR_EBS SPR_EBL SPR_EBX`. Read and confirm each pops on the `#202020` background.
- [ ] **Step 5: Commit (on approval)** — `git commit -m "draw_assets: shaded bullets"`

---

### Task 10: Full regeneration + in-engine verification

**Files:** none modified (verification only).

- [ ] **Step 1: Regenerate all assets through the real pipeline**

```bash
bun tools/draw_assets.ts
./convert.sh
```
Expected: `convert.sh` runs mksprite over every sheet with no errors; `SRC/*.SPR` are refreshed.

- [ ] **Step 2: Sanity-check quantization** — pick 2–3 sheets and confirm mksprite reports no unexpected color loss (the palette-lock should make quantization exact). Inspect the montage vs. the on-disk PNG.

- [ ] **Step 3: Build + run in DOSBox**

```bash
./build.sh
```
Check `BUILD.LOG` for a clean compile/link (`GAME.EXE` produced), then:
```bash
./run.sh
```
Play through far enough to see the player, several enemies, a boss, bullets, an explosion, and a power-up. Confirm every sprite renders filled/shaded (no leftover wireframes), transparency is clean, and nothing is mis-sized (which would indicate a frame-dimension regression).

- [ ] **Step 4: Final commit (on user approval)**

```bash
git add tools/draw_assets.ts SRC/*.SPR
git commit -m "Regenerate shaded pixel-art sprites through pipeline"
```

---

## Self-Review Notes

- **Spec coverage:** palette-lock (Task 1) ✓, shading helpers (Task 1) ✓, all archetypes redrawn (Tasks 2–9) ✓, readability rules applied per recipe ✓, frame-size invariant preserved (no dimension changes anywhere) ✓, in-engine check (Task 10) ✓. Backgrounds correctly excluded (non-goal).
- **Param-type migration:** `accent: Color` → `accent: string` for `drawSmallShip`, `drawRoundEnemy`, `drawLargeBoss`; `color: Color` → `name: string` for `drawBullet`. All call sites are updated in the same task that changes the signature (Tasks 2/3/4/6/9), so the file always compiles at each commit boundary.
- **Naming consistency:** helper names (`ramp`, `gray`, `fillPoly`, `fillShaded`, `rim`, `spec`, `cockpit`, `dither2x2`) are identical across all tasks.
- **No auto-commit:** every commit step is gated on explicit user approval, per project preference.
