# Plan 16.0 — Bullet Foundation

> **Spec:** `docs/superpowers/specs/2026-04-29-bullet-hell-expansion-design.md` (Plan 16.0 row in "Implementation Plan").

**Goal:** Lay the foundation for bullet hell content: pool 256→512, six enemy
bullet sprite types, mksprite `--flip-v` option, fix MG/Kl boss orientation.
No new patterns yet — existing single-aimed-shot still uses the new system.

**Architecture:** All changes go through existing modules (BULLET, ENEMY_AI,
mksprite, convert.sh). No new modules. Backward-compatible: old
`bullet_spawn_enemy(BUL_ENEMY_AIMED, ...)` calls keep working by mapping
to the new `BUL_E_TINY` visual.

**Tech Stack:** Watcom C++ 10.6, Bun TypeScript pipeline, mksprite
compiled-sprite system.

---

## Task 1: Add `--flip-v` option to mksprite.ts

**Files:**
- Modify: `tools/mksprite.ts:387-417`

- [ ] **Step 1: Add `flipV` to parseArgs options**

In the `parseArgs` block, add the flag:

```typescript
const { values, positionals } = parseArgs({
  args: Bun.argv.slice(2),
  options: {
    test: { type: "boolean", default: false },
    grid: { type: "string" },
    raw:  { type: "boolean", default: false },
    bin:  { type: "boolean", default: false },
    "flip-v": { type: "boolean", default: false },
  },
  allowPositionals: true,
});
```

- [ ] **Step 2: Apply vertical flip after PNG decode**

After the `pixels = mapToIndexed(...)` line (~line 417), add:

```typescript
if (values["flip-v"]) {
  const flipped = new Array<number>(w * h);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      flipped[(h - 1 - y) * w + x] = pixels[y * w + x];
    }
  }
  pixels = flipped;
}
```

This must run BEFORE `--grid` frame extraction so each frame's row order
is correct.

- [ ] **Step 3: Verify `--flip-v` doesn't break existing builds**

Run:

```bash
bun tools/mksprite.ts --bin --grid 4x1 \
  ASSETS/raw/jinvorionstg_enemies/extracted_full/"Medium tiny enemies Asset pack"/Bl_0.png \
  TEST_FLIP --flip-v 2>&1 | tail -3
```

Expected: no error. Check `SRC/TEST_FLIP.SPR` exists. Then `rm SRC/TEST_FLIP.SPR`.

- [ ] **Step 4: Commit**

```bash
git add tools/mksprite.ts
git commit -m "feat: add --flip-v option to mksprite for upside-down sprites"
```

---

## Task 2: Move bullet PNGs into project tree

The Bullet Pack and Player Bullets archives were extracted to `~/Downloads`.
Move only the files we'll use into `ASSETS/raw/jinvorionstg_bullets/`.

**Files:**
- Create: `ASSETS/raw/jinvorionstg_bullets/extracted/` (6 enemy bullet PNGs)

- [ ] **Step 1: Create destination dir and copy 6 enemy bullet PNGs**

```bash
mkdir -p ASSETS/raw/jinvorionstg_bullets/extracted
cp "/Users/gcjjyy/Downloads/bullet_extracted/Bullet Pack/Bullet Pack/Tiny_purple.png"        ASSETS/raw/jinvorionstg_bullets/extracted/
cp "/Users/gcjjyy/Downloads/bullet_extracted/Bullet Pack/Bullet Pack/Medium_Blue_Cian.png"    ASSETS/raw/jinvorionstg_bullets/extracted/
cp "/Users/gcjjyy/Downloads/bullet_extracted/Bullet Pack/Bullet Pack/Pink_medium.png"         ASSETS/raw/jinvorionstg_bullets/extracted/
cp "/Users/gcjjyy/Downloads/bullet_extracted/Bullet Pack/Bullet Pack/Bullet_Spiky_Bulky_strip7.png" ASSETS/raw/jinvorionstg_bullets/extracted/
cp "/Users/gcjjyy/Downloads/bullet_extracted/Bullet Pack/Bullet Pack/Lines_yellow.png"        ASSETS/raw/jinvorionstg_bullets/extracted/
cp "/Users/gcjjyy/Downloads/bullet_extracted/Bullet Pack/Bullet Pack/Massive_Red_Orange_Yellow.png" ASSETS/raw/jinvorionstg_bullets/extracted/
ls ASSETS/raw/jinvorionstg_bullets/extracted/
```

Expected: 6 .png files listed.

- [ ] **Step 2: Verify dimensions match spec table**

```bash
for f in ASSETS/raw/jinvorionstg_bullets/extracted/*.png; do
  echo -n "$(basename $f): "
  sips -g pixelWidth -g pixelHeight "$f" 2>/dev/null | grep -E "pixel" | awk '{print $2}' | tr '\n' 'x' | sed 's/x$//'
  echo
done
```

Expected:
- Tiny_purple.png: 28x7
- Medium_Blue_Cian.png: 40x8
- Pink_medium.png: 45x9
- Bullet_Spiky_Bulky_strip7.png: 56x8
- Lines_yellow.png: 60x7
- Massive_Red_Orange_Yellow.png: 78x13

- [ ] **Step 3: Commit**

```bash
git add ASSETS/raw/jinvorionstg_bullets/
git commit -m "assets: import 6 enemy bullet sprites from Bullet Pack"
```

---

## Task 3: Add bullet + flip conversions to convert.sh

**Files:**
- Modify: `convert.sh` (append before "Done.")

- [ ] **Step 1: Add `--flip-v` to MG, Kl boss conversions**

Find these blocks in convert.sh:

```bash
echo "Converting MG (stage 2 boss) sprite..."
bun tools/mksprite.ts --bin --grid 6x1 _workspace/bmg_packed.png SPR_BMG
```

Change to:

```bash
echo "Converting MG (stage 2 boss) sprite..."
bun tools/mksprite.ts --bin --flip-v --grid 6x1 _workspace/bmg_packed.png SPR_BMG
```

Same for Kl:

```bash
echo "Converting Kl (stage 3 final boss) sprite..."
bun tools/mksprite.ts --bin --flip-v --grid 5x1 _workspace/bkl_packed.png SPR_BKL
```

- [ ] **Step 2: Add 6 enemy bullet sprite conversions**

Append BEFORE the final `echo "Done."`:

```bash
echo "Converting enemy bullet sprites..."
bun tools/mksprite.ts --bin --grid 4x1 ASSETS/raw/jinvorionstg_bullets/extracted/Tiny_purple.png        SPR_EBT
bun tools/mksprite.ts --bin --grid 5x1 ASSETS/raw/jinvorionstg_bullets/extracted/Medium_Blue_Cian.png    SPR_EBM
bun tools/mksprite.ts --bin --grid 5x1 ASSETS/raw/jinvorionstg_bullets/extracted/Pink_medium.png         SPR_EBP
bun tools/mksprite.ts --bin --grid 7x1 ASSETS/raw/jinvorionstg_bullets/extracted/Bullet_Spiky_Bulky_strip7.png SPR_EBS
bun tools/mksprite.ts --bin --grid 6x1 ASSETS/raw/jinvorionstg_bullets/extracted/Lines_yellow.png        SPR_EBL
bun tools/mksprite.ts --bin --grid 6x1 ASSETS/raw/jinvorionstg_bullets/extracted/Massive_Red_Orange_Yellow.png SPR_EBX
```

Note: `Lines_yellow.png` 60x7 with 6 cols → frames 10x7 (acceptable; close to spec's 10x7). `Massive` 78x13 / 6 = 13x13. Naming convention: `SPR_EBT/M/P/S/L/X` (B = bullet, last = type).

- [ ] **Step 3: Run `./convert.sh` to verify**

```bash
./convert.sh 2>&1 | tail -20
```

Expected: lists 6 new SPR_EB* lines, no errors. Verify files:

```bash
ls -la SRC/SPR_EB*.SPR
```

Expected: 6 files.

- [ ] **Step 4: Commit**

```bash
git add convert.sh SRC/SPR_EB*.SPR SRC/SPR_BMG.SPR SRC/SPR_BKL.SPR
git commit -m "feat: convert 6 enemy bullet sprites + flip MG/Kl boss vertically"
```

---

## Task 4: Expand BULLET.H pool + new enemy bullet types

**Files:**
- Modify: `SRC/BULLET.H`
- Modify: `SRC/BULLET.CPP`

- [ ] **Step 1: Expand pool size and add bullet type enum**

Edit `SRC/BULLET.H`:

```c
#define MAX_PLAYER_BULLETS 64
#define MAX_ENEMY_BULLETS 512   /* was 256 — bullet hell needs more */

typedef enum {
    BUL_PLAYER_VULCAN = 0,
    BUL_PLAYER_LASER  = 1,
    BUL_PLAYER_PLASMA = 2,
    BUL_ENEMY_AIMED   = 3,        /* legacy alias — maps to BUL_E_TINY */
    BUL_PLAYER_HOMING = 4,
    /* Plan 16.0 enemy bullet visual classes */
    BUL_E_TINY        = 5,        /* SPR_EBT 7x7   — popcorn / aimed */
    BUL_E_MED         = 6,        /* SPR_EBM 8x8   — n-way spreads */
    BUL_E_PINK        = 7,        /* SPR_EBP 9x9   — mid-boss / boss */
    BUL_E_SPIKY       = 8,        /* SPR_EBS 8x8   — sine + spiral */
    BUL_E_LINE        = 9,        /* SPR_EBL 10x7  — rapid turret */
    BUL_E_MASSIVE     = 10        /* SPR_EBX 13x13 — danmaku boss */
} BulletKind;
```

- [ ] **Step 2: Read BULLET.CPP to confirm spawn site**

```bash
grep -n "bullet_spawn_enemy\|g_ebullets" SRC/BULLET.CPP | head
```

- [ ] **Step 3: Replace enemy bullet rendering with sprite + per-type table**

In `SRC/BULLET.CPP`:

(a) Add includes near the top:

```c
#include "SPRITE.H"
```

(b) Add a static sprite cache and per-type half-extent table. Place
after the existing globals (around line 11):

```c
/* Enemy bullet sprite cache. Loaded by bullet_init() and freed by
 * bullet_close() (new). One Sprite per visual class (5..10 in BulletKind). */
static Sprite g_ebul_spr[6];     /* indexed by (kind - BUL_E_TINY) */
static int    g_ebul_have[6];    /* load success flag per slot */

/* Per-class hitbox half-extent for enemy bullets vs player. Spec table.
 * Indexed by (kind - BUL_E_TINY). */
static const u8 g_ebul_half[6] = {
    2,  /* TINY    7x7 */
    2,  /* MED     8x8 */
    3,  /* PINK    9x9 */
    2,  /* SPIKY   8x8 */
    3,  /* LINE   10x7 */
    4   /* MASSIVE 13x13 */
};

/* Per-class sprite size — used for size_w/size_h on spawn (so bullet
 * bounds-test logic that uses size_w/h still works, but rendering uses
 * the sprite, not gfx_fill_rect). */
static const u8 g_ebul_size_w[6] = { 7, 8, 9, 8, 10, 13 };
static const u8 g_ebul_size_h[6] = { 7, 8, 9, 8,  7, 13 };
```

(c) Add a load helper and call it from `bullet_init()`. Replace
`bullet_init` body:

```c
static void load_enemy_bullets(void)
{
    int i;
    static const char *names[6] = {
        "SPR_EBT.SPR",   /* TINY */
        "SPR_EBM.SPR",   /* MED */
        "SPR_EBP.SPR",   /* PINK */
        "SPR_EBS.SPR",   /* SPIKY */
        "SPR_EBL.SPR",   /* LINE */
        "SPR_EBX.SPR"    /* MASSIVE */
    };
    for (i = 0; i < 6; i++) {
        g_ebul_have[i] = (spr_load(names[i], &g_ebul_spr[i]) == SPR_OK) ? 1 : 0;
    }
}

static void unload_enemy_bullets(void)
{
    int i;
    for (i = 0; i < 6; i++) {
        if (g_ebul_have[i]) { spr_free(&g_ebul_spr[i]); g_ebul_have[i] = 0; }
    }
}

void bullet_init(void)
{
    int i;
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) g_pbullets[i].active = 0;
    for (i = 0; i < MAX_ENEMY_BULLETS;  i++) g_ebullets[i].active = 0;
    load_enemy_bullets();
}

void bullet_close(void)
{
    unload_enemy_bullets();
}
```

(d) Add `bullet_close()` declaration to BULLET.H (after `bullet_init`):

```c
void bullet_init(void);
void bullet_close(void);
```

(e) Update `bullet_spawn_enemy` to set per-type size and accept BUL_E_*
classes. Map legacy `BUL_ENEMY_AIMED` to `BUL_E_TINY`:

```c
int bullet_spawn_enemy(BulletKind kind, i16 x, i16 y, i16 vx_q4, i16 vy_q4)
{
    int i;
    /* Legacy alias */
    if (kind == BUL_ENEMY_AIMED) kind = BUL_E_TINY;

    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        Bullet *b = &g_ebullets[i];
        if (b->active) continue;
        b->active = 1;
        b->kind = (u8)kind;
        if (kind >= BUL_E_TINY && kind <= BUL_E_MASSIVE) {
            int idx = kind - BUL_E_TINY;
            b->size_w = g_ebul_size_w[idx];
            b->size_h = g_ebul_size_h[idx];
            b->color_idx = 0;     /* sprite has its own colors */
        } else {
            /* Fallback (shouldn't happen) */
            b->color_idx = 27;
            b->size_w = 4;
            b->size_h = 4;
        }
        b->x = x;
        b->y = y;
        b->vx = vx_q4;
        b->vy = vy_q4;
        b->sx = 0;
        b->sy = 0;
        return 1;
    }
    return 0;
}
```

(f) Replace `bullet_render_enemy` to draw via compiled sprite. The
sprite x,y is top-left, so we need to subtract half_w/half_h from the
bullet center:

```c
void bullet_render_enemy(void)
{
    int i;
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        const Bullet *b = &g_ebullets[i];
        if (!b->active) continue;
        if (b->kind < BUL_E_TINY || b->kind > BUL_E_MASSIVE) continue;
        int idx = b->kind - BUL_E_TINY;
        if (!g_ebul_have[idx]) continue;
        const Sprite *spr = &g_ebul_spr[idx];
        /* Pick frame: shimmer using stage_t_ms equivalent — use a static
         * frame counter incremented per render call so frames advance
         * regardless of bullet age. Cycle every 4 render frames. */
        static u32 g_render_tick = 0;
        u32 tick = g_render_tick++;
        int frame = (int)((tick >> 2) % spr->n_frames);
        int sx = PLAY_X0 + b->x;
        int sy = b->y;
        /* Cull off-screen (the gfx_draw_csprite has no clip per inspect) */
        if (sx + b->size_w <= 0 || sy + b->size_h <= 0 ||
            sx >= 320 || sy >= 200) continue;
        /* Stay inside screen (no cropping in compiled sprite — hard cull
         * if we'd exceed). */
        if (sx < 0 || sy < 0 || sx + b->size_w > 320 || sy + b->size_h > 200) continue;
        gfx_draw_csprite(spr->frames[frame], sx, sy);
    }
}
```

Note: `g_render_tick` increments once per `bullet_render_enemy` call; all
bullets share the same animation frame for that frame. That's fine — it
keeps them in sync and cheap.

- [ ] **Step 4: Wire `bullet_close()` into GAME.CPP shutdown**

In `SRC/GAME.CPP`, find the shutdown chain at the end of main:

```c
    bg_close();
    gfx_close();
```

There's currently no `bullet_close()` call (bullet_init is called but
no symmetric close). Add it just before `bg_close()`:

```c
    bullet_close();
    bg_close();
```

- [ ] **Step 5: Build**

```bash
rm -f SRC/*.OBJ SRC/GAME.EXE BUILD.LOG
./build.sh 2>&1 | tail -10
```

Expected: 0 errors, 0 warnings. GAME.EXE produced.

- [ ] **Step 6: Visual verify**

```bash
./run.sh
```

Manually check (user does this):
- Game starts at title without crash.
- Press Z to start.
- Stage 1 plays normally; **enemy bullets now appear as small purple animated
  circles** instead of red squares.
- Stage 2 boss MG appears **right-side-up** (engines on top, weapons on bottom).
- Stage 3 boss Kl appears **right-side-up**.
- Stop the run when verified.

- [ ] **Step 7: Commit**

```bash
git add SRC/BULLET.H SRC/BULLET.CPP SRC/GAME.CPP
git commit -m "feat: 6 enemy bullet sprite types + 512 pool + sprite rendering"
```

---

## Task 5: End-of-plan verification

- [ ] **Step 1: Sanity-test under stress**

Set `MAX_ENEMY_BULLETS` is now 512. Existing aim-shot AI fires single bullets;
stress test by simulating high spawn rate. Quick test edit (revert after):

In `SRC/ENEMY_AI.CPP::enemy_ai_aim_player`, temporarily change:

```c
#define AIM_FIRE_INTERVAL_MS  1500
```

to:

```c
#define AIM_FIRE_INTERVAL_MS  150
```

Build, run, observe 30 active enemies firing at 6.7 Hz → ~200 bullets on screen.
Check: no crash, framerate playable, sprites still render correctly.

- [ ] **Step 2: Revert the stress test edit**

Restore `AIM_FIRE_INTERVAL_MS` to `1500`.

```bash
rm -f SRC/*.OBJ && ./build.sh 2>&1 | tail -3
```

- [ ] **Step 3: Final commit (revert stress test if accidentally staged)**

```bash
git diff SRC/ENEMY_AI.CPP
# If diff shows interval changed, revert:
git checkout -- SRC/ENEMY_AI.CPP
```

---

## Plan 16.0 Done Criteria

- `tools/mksprite.ts` accepts `--flip-v` and produces a vertically flipped
  sprite.
- 6 SPR_EB*.SPR files exist in `SRC/`.
- `SRC/BULLET.H` defines BUL_E_TINY..BUL_E_MASSIVE and `MAX_ENEMY_BULLETS=512`.
- Enemy bullets in-game render as animated bullet sprites (not red squares).
- MG (stage 2 boss) and Kl (stage 3 boss) appear right-side-up.
- Tank2 (stage 1 boss) unchanged (top-down view).
- Game runs to completion (title → 3 stages → ending) without crash.
- Stress test confirmed: ~200 active bullets render without visual artifact.
