# Bullet Hell Expansion — Design Spec

**Date:** 2026-04-29
**Author:** brainstorming session with Claude
**Status:** approved, ready for plan

## Goal

Pivot the prototype from Raiden 2 light-density combat to a Raiden Fighters
hybrid bullet hell. Multiply enemy variety to 11 types, extend stage duration
to a real arcade format (~150s with 4-act structure including mid-boss), and
replace the single-aimed-shot AI with 12 distinct bullet patterns plus a
death-burst modifier, delivering ~150-250 active enemy bullets at peak
intensity.

## Architecture

Three layered changes, isolated to clean module boundaries:

1. **Asset layer** — extract Bullet Pack + Player Bullets PNGs, add 5 new
   regular + 3 mid-boss enemy sprites, fix vertical orientation on MG/Kl
   bosses and AF fighter. Add `--flip-v` to mksprite.ts.
2. **Engine layer** — grow enemy bullet pool (256→512), add 6 enemy bullet
   sprite types with per-type rendering, add focus mode to player (Z hold
   slows + shrinks hitbox), add 12 AI patterns to ENEMY_AI module split
   into per-pattern files.
3. **Content layer** — extend STAGE script per stage to ~150s with mid-boss
   trigger, integrate 8 new enemy types into rosters, add new MID_BOSS
   FSM state slot between PLAYING and BOSS_INTRO.

## Tech Stack

- Watcom C/C++ 10.6 (existing)
- Bun TypeScript asset pipeline (existing, +flip-v option)
- mksprite compiled-sprite system (existing)
- New raw assets: Bullet Pack.rar, Player Bullets.rar (already extracted to ~/Downloads)

## Bullet System

### Pool sizing

`BULLET.H`:
- `MAX_ENEMY_BULLETS` 256 → **512**
- `MAX_PLAYER_BULLETS` stays 64

This raises `g_ebullets[]` size from ~6KB to ~12KB. Iterating 512 slots
per frame is acceptable on a 386 — bullet update is straightforward
position += velocity per slot, no allocation.

### Bullet types (enemy)

Currently 1 type (`BUL_ENEMY_AIMED`). Replace with 6 visual classes:

| ID | Name | Sprite source | Size | Use |
|----|------|---------------|------|-----|
| BUL_E_TINY | tiny circle | `Tiny_purple.png` 28x7 → 4 frames 7x7 | 7x7 | popcorn / fighter aimed shots |
| BUL_E_MED | medium circle | `Medium_Blue_Cian.png` 40x8 → 5 frames 8x8 | 8x8 | n-way spreads |
| BUL_E_PINK | pink medium | `Pink_medium.png` 45x9 → 5 frames 9x9 | 9x9 | mid-boss & boss |
| BUL_E_SPIKY | spiky | `Bullet_Spiky_Bulky_strip7.png` 56x8 → 7 frames 8x8 | 8x8 | sine + spiral |
| BUL_E_LINE | thin line | `Lines_yellow.png` 60x7 → variable frames | ~10x7 | rapid-fire turret |
| BUL_E_MASSIVE | massive | `Massive_Red_Orange_Yellow.png` 78x13 → 6 frames 13x13 | 13x13 | charge + boss "danmaku" |

Each is 4-7 frame animation; we pick frame by `(g_frame_counter / 4) % N`
for a uniform shimmer.

Hitboxes are tighter than sprite size for fairness — halved relative to the
sprite. Half-extents per type:

| Type | Sprite | Hitbox half-extent |
|------|--------|--------------------|
| TINY | 7x7 | 2 |
| MED | 8x8 | 2 |
| PINK | 9x9 | 3 |
| SPIKY | 8x8 | 2 |
| LINE | 10x7 | 3 (wider) |
| MASSIVE | 13x13 | 4 |

### AI patterns (12 motion/fire + 1 modifier)

The existing `enemy_ai_aim_player` becomes `PAT_AIM_1` (single shot).
12 active patterns are dispatched as `enum EnemyPattern`. The 13th
(`BURST_ON_DEATH`) is a flag on the Enemy struct that composes with
any motion pattern.

ENEMY_AI keeps a single `.CPP` file but splits into per-pattern
sections (each `enemy_ai_<name>`):

| Pattern | Behavior | Cooldown | Bullet |
|---------|----------|----------|--------|
| PAT_STRAIGHT_DOWN | no firing, just motion | — | — |
| PAT_AIM_1 | single aimed shot | 1500ms | TINY |
| PAT_FAN_3 | 3-way fan ±15° (centered down) | 1200ms | MED |
| PAT_FAN_5 | 5-way fan ±30° | 1500ms | MED |
| PAT_FAN_7 | 7-way fan ±45° | 1800ms | MED |
| PAT_AIM_3 | 3-way fan centered on player ±10° | 1300ms | TINY |
| PAT_RING_8 | 8-way full circle | 1800ms | PINK |
| PAT_RING_16 | 16-way full circle | 2400ms | PINK |
| PAT_SPIRAL | rotating turret, 1 shot/frame at 12°/shot | continuous | SPIKY |
| PAT_SINE | bullets curve sinusoidally | 800ms | SPIKY |
| PAT_LASER | 0.5s charge tell + 1s straight beam | 3000ms | MASSIVE |
| PAT_HOMING | slow tracking missile | 2200ms | LINE (with smoke) |

Plus on-death modifier:

| Modifier | Behavior |
|----------|----------|
| ENEMY_FLAG_BURST_DEATH | When killed, emit 8-way ring of TINY bullets |

Stored as bit in `Enemy.flags`. Set on spawn for E_BURSTER and E_MID_CI;
fired in COLLIDE.CPP when HP reaches 0.

### Lateral bullets

The "horizontal bullets from screen edges" idea is dropped from the
12-pattern set — it requires off-screen spawners and complicates AI
dispatch. Reroute to bosses if we want the look later.

## Player Focus Mode

- Z held = fire + focus (combined, no extra key)
- Focus active: speed multiplied by 0.6, hitbox shrinks 4x4 → 2x2
- Focus visual: a 1px white pixel at player center + thin cyan outline ring
  every other frame (8x8 ring at center)
- Focus state has no separate input timing — it's purely "is Z held this
  frame", checked in `player_update`.

## Stage Structure (150s per stage)

```
 0s ─────────────────── 40s ── 65s ────────────── 115s ── 130s ──── boss
   opening waves       mid-    middle waves        boss     boss
                       boss                       intro    fight
```

| Window | Content |
|--------|---------|
| 0–40s  | Stage-1 weight popcorn / fighter / blue mix (~15 events) |
| 40–55s | Mid-boss spawn + fight (event triggers `MID_BOSS` state) |
| 55–115s | Heavier waves: turret, tank1, kamikaze, popcorn0 mixed (~25 events) |
| 115–130s | Cooldown + warning (boss intro flash) |
| 130s+   | Main boss (existing) |

Stage clock now runs to ~130s before boss instead of 30s.

### MID_BOSS FSM state

Add `GS_MID_BOSS_INTRO` and `GS_MID_BOSS_FIGHT` between `GS_PLAYING` and
the existing `GS_BOSS_INTRO`. Stage script triggers entry with a special
event (t_ms = 40000, type = `EVT_MIDBOSS`). Mid-boss death returns to
`GS_PLAYING` and resumes script from where the timeline left off.

## Enemy Roster (11 types: 4 existing + 4 new regular + 3 mid-boss)

Existing 4 (Plan 15):

| ID | Sprite | Size | HP | Score | Default AI |
|----|--------|------|-----|-------|------------|
| E_POPCORN_A | pop_*.png | 23x31 | 2 | 100 | STRAIGHT / AIM_1 |
| E_FIGHTER_R | rn_*.png | 28x42 | 3 | 200 | AIM_1 |
| E_BLUE | Bl_*.png | 31x38 | 3 | 300 | AIM_3 |
| E_MED_C | MC_*.png | 70x55 | 8 | 1000 | FAN_5 |

New 4 regular (this plan):

| ID | Sprite | Frames | HP | Score | Default AI |
|----|--------|--------|-----|-------|------------|
| E_POPCORN_B | Pop0_*.png | 4 | 4 | 200 | FAN_3 |
| E_FIGHTER_AF | AF_*.png (flip-v) | 4 | 5 | 400 | AIM_3 |
| E_KAMIKAZE | ss_*.png | 4 | 1 | 150 | STRAIGHT (fast vy=64) |
| E_BURSTER | hb_*.png | 3 | 2 | 250 | AIM_1 + BURST_DEATH flag |
| E_TURRET | Turret_head_silver_*.png (ground) | 5 | 6 | 500 | SPIRAL |

3 mid-boss (this plan):

| ID | Sprite | Frames | HP | Score | Default AI |
|----|--------|--------|-----|-------|------------|
| E_MID_MV | MV_*.png | 4 | 100 | 5000 | FAN_5 ↔ SINE phase swap (2s) |
| E_MID_MS | MS_*.png | 4 | 130 | 8000 | HOMING + RING_8 alternating |
| E_MID_CI | CI_*.png | 5 | 150 | 10000 | RING_16 + BURST_DEATH flag |

Final count: 4 existing + 5 new regular + 3 mid-boss = **12**. Mid-bosses
share the Enemy slot/struct (no separate Boss struct) but get their
own FSM state for the boss-style intro/HP-bar HUD.

### Plane assignment

Most stay PLANE_AIR. E_TURRET goes PLANE_GROUND — added in Plan 15 enum
but unused so far. PLANE_GROUND enemies don't scroll with background;
they spawn at top, drift slowly down at background scroll speed, then
deactivate at bottom. They cannot be hit by player vertical bullets if
y is below the player (typical Raiden rule) — but for simplicity we'll
let player bullets damage them normally; only their hitbox vs player
ignores hitscan.

## Asset Pipeline

### convert.sh additions

```bash
# Vertical-flip pre-pass for upside-down sprites
echo "Packing MG (stage 2 boss) frames + flip-v..."
tools/montage_frames.sh _workspace/bmg_packed.png ...
bun tools/mksprite.ts --bin --flip-v --grid 6x1 _workspace/bmg_packed.png SPR_BMG

echo "Packing Kl (stage 3 boss) frames + flip-v..."
... --flip-v ...

echo "Packing AF (heavy fighter) + flip-v..."
... --flip-v ...

# 6 enemy bullet sprites
echo "Converting enemy bullet sprites..."
bun tools/mksprite.ts --bin --grid 4x1 \
  ASSETS/raw/jinvorionstg_bullets/.../Tiny_purple.png SPR_EBT
bun tools/mksprite.ts --bin --grid 5x1 ... SPR_EBM
... etc
```

### mksprite.ts: --flip-v option

Add a flag that flips the input image vertically (top↔bottom rows
swapped) before frame extraction. Implementation: after PNG decode but
before grid split. ~10 lines of TS.

### Bullet pack assets

Move `~/Downloads/bullet_extracted/Bullet Pack/Bullet Pack/*.png` and
`~/Downloads/player_bullets_extracted/Player Bullets/*.png` into:
- `ASSETS/raw/jinvorionstg_bullets/extracted/` (enemy bullet PNGs)
- `ASSETS/raw/jinvorionstg_bullets/player_bullets/` (player bullet PNGs)

This puts them under version control (well, asset path is gitignored;
just under the project tree where the pipeline can find them).

## Module Changes

| Module | Change |
|--------|--------|
| BULLET.H | `MAX_ENEMY_BULLETS` 512, add 6 BUL_E_* types, add half-extent table |
| BULLET.CPP | per-type render dispatch (compiled sprite call) |
| ENEMY.H | add 8 new EnemyType + per-type metadata, add `flags` u8 (BURST_ON_DEATH bit) |
| ENEMY.CPP | load 8 new sprites with optional g_have_* flags, render dispatch, BURST_ON_DEATH on kill |
| ENEMY_AI.H | declare 12 ai functions, document each |
| ENEMY_AI.CPP | split into 12 per-pattern blocks (single file) |
| PLAYER.CPP | focus mode: speed scale + hitbox shrink + visual marker |
| COLLIDE.CPP | use dynamic player hitbox half-extent (1 or 2 based on focus) |
| STATE.H | add `g_state.focus_active` u8 |
| STAGE.H/CPP | add `EVT_MIDBOSS` event type, extend script tables to ~150s, ~40 events per stage |
| GAME.CPP | add `GS_MID_BOSS_INTRO` and `GS_MID_BOSS_FIGHT`, transition logic |
| convert.sh | flip-v on MG/Kl/AF, add 6 bullet sprites + 8 enemy sprites |
| mksprite.ts | add `--flip-v` flag |

## Risks / Open Questions

1. **Frame budget at peak bullet count**: 512 enemy bullets + 12 player +
   32 enemies + 8 effects + boss + bg + player = ~570 active sprites
   per frame on a real DOSBox @ 386. Should profile after Plan 16.0.
   Mitigation: bullets are 7x7~13x13 (small compiled sprites, ~20-50
   instructions each). Should fit in 16ms but worth measuring.

2. **PLANE_GROUND scroll behavior unclear**: should ground enemies
   inherit background scroll speed exactly so they appear pinned to the
   ground texture, or move independently? Ruling: pin to background
   speed (60 px/sec), simpler.

3. **Mid-boss interrupts script timing**: stage clock during mid-boss
   fight — does it pause or keep running? Ruling: pause stage clock
   during MID_BOSS_FIGHT, resume after death. This way scripts after
   mid-boss aren't squeezed if mid-boss fight runs long.

4. **Focus mode + auto-fire interaction**: Z held = fire + focus. What
   if user wants only fire without focus? Ruling: not supported in this
   plan. Bullet hell convention is Z = focused fire. If we later add
   X for bomb-only, focus toggle could be split out.

5. **Bullet visual readability at peak density**: 512 small sprites might
   be visually muddy. Mitigation: each pattern uses one bullet type, so
   colors stay readable per-source. Boss + danmaku patterns intentionally
   use MASSIVE bullets so they stand out from popcorn TINY shots.

## Implementation Plan (high-level)

This will be split into ~5-7 incremental plans, each shippable:

- **Plan 16.0**: Pool expansion + 6 bullet sprite types + flip-v option +
  flip MG/Kl bosses. Verify nothing breaks. (Foundation only.)
- **Plan 16.1**: 12 AI patterns. Each one tested in isolation against a
  single test enemy.
- **Plan 16.2**: Add 4 new air enemies (Pop0, AF, ss kamikaze, hb
  burster). Turret deferred to 16.4 since it needs ground plane.
- **Plan 16.3**: Focus mode (player) + 2x2 focused hitbox + visual.
- **Plan 16.4**: Ground plane + Turret enemy. Test ground scroll rule.
- **Plan 16.5**: 3 mid-boss enemies + GS_MID_BOSS_* states + STAGE
  EVT_MIDBOSS handling.
- **Plan 16.6**: Stage scripts rewritten to 150s with mid-boss + 4-act
  structure + diverse roster usage. Verify all 3 stages.

The split lets each plan get visual verification before the next layer
lands. If we hit frame-budget trouble in 16.0, we can reduce pool size
or simplify bullet sprites before patterns land in 16.1.
