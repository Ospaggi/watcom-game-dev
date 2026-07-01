# Raiden 2 Clone — Plan 2: Vertical Slice 2 (적 + 충돌 + 폭발)

> **For agentic workers:** Use superpowers:subagent-driven-development to execute. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 1종 적 (popcorn)이 화면 위에서 등장해 직진 하강하고, 플레이어 탄에 맞으면 폭발 + 점수 가산. 적 탄막/플레이어 사망/HUD/메달은 후속 plan.

**Architecture:** Plan 1의 BG/PLAYER/BULLET 위에 적 + 이펙트 + 충돌 모듈 추가. 임시 스폰 스케줄러는 GAME.CPP에 직접 (STAGE 시스템은 후속 plan). 점수는 `g_state.score`에 누적만 (화면 표시 X — FONT 자산 파이프라인 부재).

**Tech Stack:** Watcom C++, TypeScript+Bun, ImageMagick (montage 헬퍼), DOSBox.

**Spec 참조:** `docs/superpowers/specs/2026-04-28-raiden2-clone-design.md` §5 (적 시스템), §6 (탄), §7 (충돌), §8 (폭발), §9.6 (점수)

---

## File Map

### NEW FILES

| 경로 | 책임 |
|---|---|
| `tools/montage_frames.sh` | 개별 PNG 프레임 N개를 가로 시트로 합치는 ImageMagick 래퍼 |
| `SRC/ENEMY.H` / `SRC/ENEMY.CPP` | Enemy 구조체, 32-슬롯 풀, 스폰/업데이트(AI 디스패치)/렌더 |
| `SRC/ENEMY_AI.H` / `SRC/ENEMY_AI.CPP` | `PAT_STRAIGHT_DOWN` 함수 (Plan 2 = 1 패턴만; 후속 plan에서 PAT_*  추가) |
| `SRC/EFFECT.H` / `SRC/EFFECT.CPP` | Effect 구조체 (폭발용), 24-슬롯 풀, 5프레임 애니 |
| `SRC/COLLIDE.H` / `SRC/COLLIDE.CPP` | `collide_player_bullets_vs_enemies()` AABB 검사 — 적중 시 탄/적 비활성, 이펙트 스폰, 점수 가산 |

### GENERATED FILES

| 경로 | 출처 |
|---|---|
| `SRC/SPR_EPOP.SPR` | `ASSETS/raw/jinvorionstg_enemies/extracted_full/.../pop_0~3.png` (4 frames 23x31, 가로로 montage → 92x31 sheet) |
| `SRC/SPR_EXPL.SPR` | `ASSETS/raw/ansimuz_spaceship/.../explosion.png` (80x16 sheet, 5 frames 16x16) |

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `SRC/GAME.CPP` | 신규 모듈 init/close/update/render/collide 호출 + 임시 스폰 스케줄러 (1초마다 popcorn 1대) |
| `convert.sh` | montage_frames.sh + mksprite 호출 추가 (pop, explosion) |

### NOT TOUCHED

- 적 탄 풀 / 플레이어 충돌 / 사망/부활 → Plan 3
- HUD / 폰트 / 점수 표시 → Plan 4
- STAGE 시스템 (스폰 스크립트) → Plan 5
- 메달 / 아이템 / P 색순환 → Plan 6
- 봄 / 오토봄 → Plan 7
- 보스 → Plan 8

---

## Task 1: tools/montage_frames.sh — 개별 프레임을 가로 시트로 합치는 헬퍼

**Files:**
- Create: `tools/montage_frames.sh`

jinvorionstg 적/터렛 자산은 프레임이 개별 PNG로 분리됨 (예: `pop_0.png`, `pop_1.png`, ..., `pop_3.png`). mksprite.ts는 단일 시트 입력을 가정하므로 전처리 필요.

ImageMagick `magick montage` 또는 `magick convert ... +append`를 사용해 가로로 이어붙임. 각 프레임이 동일 크기여야 함 (이 plan의 pop은 모두 23x31).

`brew install imagemagick`이 사전조건 — ImageMagick이 없으면 helper에서 명확한 에러 메시지.

- [ ] **Step 1: tools/montage_frames.sh 작성**

```bash
#!/usr/bin/env bash
# montage_frames.sh - Concatenate N same-size PNGs into a single horizontal strip.
#
# All inputs must have identical w x h. Output PNG is N*w wide and h tall.
# Used by convert.sh to prep multi-file frame sets (e.g., jinvorionstg pop_0..3.png)
# for `mksprite --bin --grid Nx1`.
#
# Usage: tools/montage_frames.sh <out.png> <in1.png> <in2.png> [<in3.png>...]

set -euo pipefail

if [ "$#" -lt 3 ]; then
  echo "usage: $0 <out.png> <in1.png> <in2.png> [<inN.png>...]" >&2
  exit 1
fi

if ! command -v magick >/dev/null 2>&1; then
  echo "error: ImageMagick 'magick' not found in PATH (try: brew install imagemagick)" >&2
  exit 1
fi

OUT="$1"
shift

magick "$@" +append "$OUT"
```

- [ ] **Step 2: 실행 권한 + 동작 확인 (pop_*.png 4개를 합쳐 92x31 시트 생성)**

```bash
chmod +x tools/montage_frames.sh
mkdir -p _workspace
tools/montage_frames.sh _workspace/pop_packed.png \
  ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium\ tiny\ enemies\ Asset\ pack/pop_0.png \
  ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium\ tiny\ enemies\ Asset\ pack/pop_1.png \
  ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium\ tiny\ enemies\ Asset\ pack/pop_2.png \
  ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium\ tiny\ enemies\ Asset\ pack/pop_3.png

sips -g pixelWidth -g pixelHeight _workspace/pop_packed.png | tail -2
```

Expected: pixelWidth = 92, pixelHeight = 31.

- [ ] **Step 3: 커밋**

```bash
git add tools/montage_frames.sh
git commit -m "Add montage_frames.sh helper for multi-file frame sets

Concatenates N same-size PNGs into a single horizontal strip via
ImageMagick montage. Used by convert.sh to prep jinvorionstg-style
frame sets (pop_0..3.png etc.) for mksprite --grid Nx1.

Output goes to _workspace/ (gitignored, intermediate)."
```

---

## Task 2: pop_*.png → SPR_EPOP.SPR + convert.sh 갱신

**Files:**
- Generated: `SRC/SPR_EPOP.SPR`
- Modify: `convert.sh`

- [ ] **Step 1: convert.sh에 montage + mksprite 추가**

`convert.sh`의 "Converting desert background..." 다음, "Done." 직전에 추가:

```bash
echo "Packing popcorn enemy frames..."
tools/montage_frames.sh _workspace/pop_packed.png \
  "ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium tiny enemies Asset pack/pop_0.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium tiny enemies Asset pack/pop_1.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium tiny enemies Asset pack/pop_2.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium tiny enemies Asset pack/pop_3.png"

echo "Converting popcorn enemy sprite..."
bun tools/mksprite.ts --bin --grid 4x1 _workspace/pop_packed.png SPR_EPOP
```

- [ ] **Step 2: 실행 + SPR 생성 확인**

```bash
./convert.sh
ls -lh SRC/SPR_EPOP.SPR
```

Expected stderr line: `/* SRC/SPR_EPOP.SPR: 92x31 sheet, 4x1 grid, 4 frames of 23x31 */`
Expected: SRC/SPR_EPOP.SPR 존재, 수십~수백 바이트 (투명 비율에 따라).

- [ ] **Step 3: 커밋**

```bash
git add convert.sh
git commit -m "Convert popcorn enemy PNG frames to compiled SPR

pop_0..3.png (4 frames 23x31 from jinvorionstg) -> _workspace/
pop_packed.png (92x31 horizontal strip) -> SRC/SPR_EPOP.SPR
(4 compiled frames). Used by ENEMY module's E_POPCORN type.

Frame indices 0..3 cycle as the popcorn's idle animation (no
direction-based selection, unlike the player ship)."
```

---

## Task 3: explosion.png → SPR_EXPL.SPR + convert.sh 갱신

**Files:**
- Generated: `SRC/SPR_EXPL.SPR`
- Modify: `convert.sh`

`explosion.png`는 이미 80x16 시트 (5프레임 16x16). 별도 montage 불필요.

- [ ] **Step 1: convert.sh에 explosion 변환 추가**

`convert.sh`의 popcorn 단계 다음, "Done." 직전:

```bash
echo "Converting explosion sprite..."
bun tools/mksprite.ts --bin --grid 5x1 \
  ASSETS/raw/ansimuz_spaceship/extracted/Spaceship-shooter-gamekit/Assets/spritesheets/explosion.png \
  SPR_EXPL
```

- [ ] **Step 2: 실행 + 확인**

```bash
./convert.sh
ls -lh SRC/SPR_EXPL.SPR
```

Expected stderr: `/* SRC/SPR_EXPL.SPR: 80x16 sheet, 5x1 grid, 5 frames of 16x16 */`

- [ ] **Step 3: 커밋**

```bash
git add convert.sh
git commit -m "Convert explosion sprite (5 frames 16x16) to compiled SPR

ansimuz explosion.png (80x16 horizontal strip) -> SRC/SPR_EXPL.SPR.
Used by EFFECT module for enemy death animation."
```

---

## Task 4: SRC/ENEMY.H + ENEMY.CPP — 적 풀 + AI 디스패치

**Files:**
- Create: `SRC/ENEMY.H`, `SRC/ENEMY.CPP`

Plan 2 = 1 적 종류 (E_POPCORN_A) + 1 패턴 (PAT_STRAIGHT_DOWN). 구조체와 인프라는 spec 전체를 수용하도록 설계 (후속 plan에서 적 종류만 늘림).

- [ ] **Step 1: ENEMY.H 작성**

```c
#ifndef ENEMY_H_INCLUDED
#define ENEMY_H_INCLUDED

#include "STATE.H"

/* Enemy type codes (Plan 2: only popcorn). */
typedef enum {
    E_POPCORN_A = 0
    /* future: E_POPCORN_B, E_FIGHTER_R, ..., E_MED_*, etc. (spec §5.2) */
} EnemyType;

/* Enemy plane (always PLANE_AIR in Plan 2). */
typedef enum {
    PLANE_AIR    = 0,
    PLANE_GROUND = 1
} EnemyPlane;

/* AI pattern IDs (Plan 2: only PAT_STRAIGHT_DOWN). */
typedef enum {
    PAT_STRAIGHT_DOWN = 0
    /* future: PAT_SINE, PAT_AIM_PLAYER, PAT_DIVE, PAT_FORMATION_V, ... */
} EnemyPattern;

/* Enemy sprite size (per type). Plan 2 uses popcorn 23x31. */
#define POPCORN_W       23
#define POPCORN_H       31
#define POPCORN_HALF_W  11   /* center-based AABB box */
#define POPCORN_HALF_H  15
#define POPCORN_HP       2   /* takes 2 vulcan hits to die */

/* Position is the CENTER of the enemy sprite, in play-area coords (0..256, 0..200). */
typedef struct {
    u8  active;          /* 0 = empty slot */
    u8  type;            /* EnemyType */
    u8  plane;           /* EnemyPlane */
    u8  ai_id;           /* EnemyPattern */
    i16 x, y;            /* center, play-area coords */
    i16 vx, vy;          /* 1/16 px/frame subpixel velocity */
    i16 sx, sy;          /* subpixel accumulators */
    u16 hp;
    u16 t_spawn_ms;      /* ms since spawn (for AI state changes) */
    u8  fire_cooldown;   /* ms (Plan 2 unused) */
    u8  frame;           /* animation frame index */
    u32 anim_t_ms;       /* animation timer accumulator */
    u16 ai_state;        /* generic AI FSM (Plan 2 unused) */
} Enemy;

#define MAX_ENEMIES         32
#define ENEMY_ANIM_PERIOD_MS 120

extern Enemy g_enemies[MAX_ENEMIES];

int  enemy_init(void);
void enemy_close(void);

/* Spawn a new enemy. Returns 1 on success, 0 if pool full.
 * vx_q4/vy_q4 are 1/16 px/frame velocities. */
int  enemy_spawn(EnemyType type, EnemyPlane plane, EnemyPattern pattern,
                 i16 x, i16 y, i16 vx_q4, i16 vy_q4);

void enemy_update_all(u32 dt_ms);

/* Render layer split: ground first, then air (per spec §5.4 render order).
 * Plan 2 only spawns AIR enemies, but the split is in place for later plans. */
void enemy_render_ground(void);
void enemy_render_air(void);

#endif
```

- [ ] **Step 2: ENEMY.CPP 작성**

```c
#include "ENEMY.H"
#include "ENEMY_AI.H"
#include "GFX.H"
#include "SPRITE.H"

#define PLAY_X0    32

Enemy g_enemies[MAX_ENEMIES];
static Sprite g_spr_pop;

int enemy_init(void)
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) g_enemies[i].active = 0;
    if (spr_load("SPR_EPOP.SPR", &g_spr_pop) != SPR_OK) return -1;
    return 0;
}

void enemy_close(void)
{
    spr_free(&g_spr_pop);
}

int enemy_spawn(EnemyType type, EnemyPlane plane, EnemyPattern pattern,
                i16 x, i16 y, i16 vx_q4, i16 vy_q4)
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_enemies[i];
        if (e->active) continue;
        e->active = 1;
        e->type = (u8)type;
        e->plane = (u8)plane;
        e->ai_id = (u8)pattern;
        e->x = x;
        e->y = y;
        e->vx = vx_q4;
        e->vy = vy_q4;
        e->sx = 0;
        e->sy = 0;
        e->hp = POPCORN_HP;   /* Plan 2: only popcorn */
        e->t_spawn_ms = 0;
        e->fire_cooldown = 0;
        e->frame = 0;
        e->anim_t_ms = 0;
        e->ai_state = 0;
        return 1;
    }
    return 0;
}

void enemy_update_all(u32 dt_ms)
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_enemies[i];
        if (!e->active) continue;

        e->t_spawn_ms += (u16)dt_ms;

        /* AI dispatch: Plan 2 only PAT_STRAIGHT_DOWN.
         * Future plans add a switch on e->ai_id. */
        enemy_ai_straight_down(e, dt_ms);

        /* subpixel motion */
        e->sx += e->vx;
        e->sy += e->vy;
        e->x  += e->sx >> 4;  e->sx &= 0x0F;
        e->y  += e->sy >> 4;  e->sy &= 0x0F;

        /* deactivate if scrolled off bottom of play area */
        if (e->y - POPCORN_HALF_H > 200 + 32) e->active = 0;

        /* animation */
        e->anim_t_ms += dt_ms;
        if (e->anim_t_ms >= ENEMY_ANIM_PERIOD_MS) {
            e->anim_t_ms -= ENEMY_ANIM_PERIOD_MS;
            e->frame = (u8)((e->frame + 1) & 3);  /* 4-frame cycle */
        }
    }
}

void enemy_render_ground(void)
{
    /* Plan 2 has no ground enemies. Stub for render-order parity. */
}

void enemy_render_air(void)
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        const Enemy *e = &g_enemies[i];
        if (!e->active) continue;
        if (e->plane != PLANE_AIR) continue;

        /* center -> top-left */
        int sx = PLAY_X0 + e->x - POPCORN_HALF_W;
        int sy = e->y - POPCORN_HALF_H;

        /* compiled sprites need on-screen positions; popcorn can spawn
         * partially above screen (y < 0) — when it would clip, skip the
         * draw for this frame. */
        if (sx < 0 || sy < 0 || sx + POPCORN_W > 320 || sy + POPCORN_H > 200) continue;

        gfx_draw_csprite(g_spr_pop.frames[e->frame], sx, sy);
    }
}
```

> **참고**: Plan 1과 동일하게 컴파일된 스프라이트는 화면 클리핑 안 됨 → 부분 화면 밖이면 그리기 스킵. 적이 위에서 들어오는 동안 잠깐 안 보이지만, 23x31 적이 화면 안에 들어오면 정상 표시됨. 후속 plan에서 가장자리 클리핑 처리 개선 가능.

- [ ] **Step 3: 커밋 (아직 컴파일 X — ENEMY_AI 선언 필요)**

이 시점엔 ENEMY.CPP가 `enemy_ai_straight_down`을 호출하지만 ENEMY_AI.H가 없어 빌드 에러. Task 5에서 ENEMY_AI 만든 다음 두 모듈 함께 빌드.

```bash
git add SRC/ENEMY.H SRC/ENEMY.CPP
git commit -m "Add ENEMY module — pool + AI dispatch (popcorn only)

Enemy struct supports the full spec design (types, planes, AI patterns)
but Plan 2 only spawns E_POPCORN_A with PAT_STRAIGHT_DOWN. 32-slot
pool, q4 subpixel velocity, 4-frame idle animation cycling every
120ms.

Renders compiled SPR_EPOP frames; partially-off-screen positions are
skipped (compiled sprites do not clip). Render is split into
enemy_render_ground/air per spec §5.4 ordering, though only AIR is
populated in Plan 2.

Build will fail until Task 5 supplies enemy_ai_straight_down."
```

---

## Task 5: SRC/ENEMY_AI.H + ENEMY_AI.CPP — PAT_STRAIGHT_DOWN

**Files:**
- Create: `SRC/ENEMY_AI.H`, `SRC/ENEMY_AI.CPP`

가장 단순한 패턴: 스폰 시 vy 설정 후 그대로 두고, AI는 매 프레임 아무것도 하지 않음.

- [ ] **Step 1: ENEMY_AI.H 작성**

```c
#ifndef ENEMY_AI_H_INCLUDED
#define ENEMY_AI_H_INCLUDED

#include "STATE.H"

/* Forward declaration to avoid circular include with ENEMY.H. */
struct Enemy;

/* Per-frame AI hook: PAT_STRAIGHT_DOWN.
 *
 * Spawn sets vy and the enemy keeps it forever. This function is a no-op
 * here, but the dispatcher calls it for parity with future patterns
 * (PAT_AIM_PLAYER, PAT_DIVE, etc.) that DO mutate state per frame. */
void enemy_ai_straight_down(struct Enemy *e, u32 dt_ms);

#endif
```

- [ ] **Step 2: ENEMY_AI.CPP 작성**

```c
#include "ENEMY_AI.H"
#include "ENEMY.H"

void enemy_ai_straight_down(struct Enemy *e, u32 dt_ms)
{
    (void)e;
    (void)dt_ms;
    /* No-op: motion is fully determined by vy set at spawn time. */
}
```

- [ ] **Step 3: 빌드 검증 — ENEMY+ENEMY_AI 둘 다 컴파일되는지**

```bash
./build.sh
cat BUILD.LOG | tail -15
ls -l SRC/GAME.EXE
```

Expected: ENEMY.CPP + ENEMY_AI.CPP 둘 다 0 errors, 0 warnings. GAME.EXE 갱신 (아직 게임 로직에서 enemy_init 호출 안 함 — 단지 link만 성공).

- [ ] **Step 4: 커밋**

```bash
git add SRC/ENEMY_AI.H SRC/ENEMY_AI.CPP
git commit -m "Add ENEMY_AI module — PAT_STRAIGHT_DOWN (no-op stub)

The straight-down pattern is fully determined by the spawn-time vy
velocity, so the per-frame AI hook is a no-op. Future patterns
(PAT_AIM_PLAYER, PAT_SINE, PAT_DIVE, etc.) will mutate enemy state
each frame and live in this module."
```

---

## Task 6: SRC/EFFECT.H + EFFECT.CPP — 폭발 이펙트 풀

**Files:**
- Create: `SRC/EFFECT.H`, `SRC/EFFECT.CPP`

5프레임 폭발 애니, 각 프레임 ~80ms 표시. 24-슬롯 풀 (동시 24개 폭발 OK).

- [ ] **Step 1: EFFECT.H 작성**

```c
#ifndef EFFECT_H_INCLUDED
#define EFFECT_H_INCLUDED

#include "STATE.H"

#define EFFECT_EXPL_W       16
#define EFFECT_EXPL_H       16
#define EFFECT_EXPL_HALF_W   8
#define EFFECT_EXPL_HALF_H   8
#define EFFECT_EXPL_FRAMES   5
#define EFFECT_EXPL_PERIOD_MS 80   /* per-frame duration */

#define MAX_EFFECTS 24

typedef enum {
    FX_EXPLOSION = 0
    /* future: FX_HIT_FLASH, FX_BOMB, etc. */
} EffectKind;

typedef struct {
    u8  active;
    u8  kind;
    u8  frame;
    i16 x, y;            /* center, play-area coords */
    u32 t_ms;            /* accumulated time on current frame */
} Effect;

extern Effect g_effects[MAX_EFFECTS];

int  effect_init(void);
void effect_close(void);

/* Spawn an explosion at center (x, y) (play-area coords).
 * Returns 1 on success, 0 if pool full. */
int  effect_spawn_explosion(i16 x, i16 y);

void effect_update_all(u32 dt_ms);
void effect_render(void);

#endif
```

- [ ] **Step 2: EFFECT.CPP 작성**

```c
#include "EFFECT.H"
#include "GFX.H"
#include "SPRITE.H"

#define PLAY_X0   32

Effect g_effects[MAX_EFFECTS];
static Sprite g_spr_expl;

int effect_init(void)
{
    int i;
    for (i = 0; i < MAX_EFFECTS; i++) g_effects[i].active = 0;
    if (spr_load("SPR_EXPL.SPR", &g_spr_expl) != SPR_OK) return -1;
    return 0;
}

void effect_close(void)
{
    spr_free(&g_spr_expl);
}

int effect_spawn_explosion(i16 x, i16 y)
{
    int i;
    for (i = 0; i < MAX_EFFECTS; i++) {
        Effect *e = &g_effects[i];
        if (e->active) continue;
        e->active = 1;
        e->kind = (u8)FX_EXPLOSION;
        e->frame = 0;
        e->x = x;
        e->y = y;
        e->t_ms = 0;
        return 1;
    }
    return 0;
}

void effect_update_all(u32 dt_ms)
{
    int i;
    for (i = 0; i < MAX_EFFECTS; i++) {
        Effect *e = &g_effects[i];
        if (!e->active) continue;

        e->t_ms += dt_ms;
        while (e->t_ms >= EFFECT_EXPL_PERIOD_MS) {
            e->t_ms -= EFFECT_EXPL_PERIOD_MS;
            e->frame++;
            if (e->frame >= EFFECT_EXPL_FRAMES) {
                e->active = 0;
                break;
            }
        }
    }
}

void effect_render(void)
{
    int i;
    for (i = 0; i < MAX_EFFECTS; i++) {
        const Effect *e = &g_effects[i];
        if (!e->active) continue;

        int sx = PLAY_X0 + e->x - EFFECT_EXPL_HALF_W;
        int sy = e->y - EFFECT_EXPL_HALF_H;
        if (sx < 0 || sy < 0 || sx + EFFECT_EXPL_W > 320 || sy + EFFECT_EXPL_H > 200) continue;

        gfx_draw_csprite(g_spr_expl.frames[e->frame], sx, sy);
    }
}
```

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -15
```

Expected: 0 errors. 14개 모듈 컴파일됨 (기존 12 + ENEMY + ENEMY_AI + EFFECT = 14… 잠깐 그러면 15개. SCRNCAP이 있으니 15.)

- [ ] **Step 4: 커밋**

```bash
git add SRC/EFFECT.H SRC/EFFECT.CPP
git commit -m "Add EFFECT module — explosion effect pool (24 slots)

5-frame explosion animation (16x16, 80ms per frame, total 400ms).
Loaded from SRC/SPR_EXPL.SPR. Center-based positioning in play-area
coords like ENEMY/PLAYER. Skips draw when partially off-screen
(compiled sprites don't clip). Slot self-deactivates after the last
frame plays."
```

---

## Task 7: SRC/COLLIDE.H + COLLIDE.CPP — 플레이어 탄 vs 적 충돌

**Files:**
- Create: `SRC/COLLIDE.H`, `SRC/COLLIDE.CPP`

매 프레임 호출. AABB. 적중 시: 적 hp -= 1 (탄 데미지 1, Plan 1 vulcan), hp == 0이면 비활성 + 폭발 이펙트 + 점수 100, 탄은 항상 비활성.

- [ ] **Step 1: COLLIDE.H 작성**

```c
#ifndef COLLIDE_H_INCLUDED
#define COLLIDE_H_INCLUDED

#include "STATE.H"

/* Per-frame collision check: every active player bullet against every
 * active air enemy. AABB.
 *
 * Side effects on a hit:
 *  - bullet.active = 0 (always, regardless of whether enemy dies)
 *  - enemy.hp -= 1; if hp == 0:
 *      - enemy.active = 0
 *      - effect_spawn_explosion at enemy center
 *      - g_state.score += 100  (popcorn kill score, spec §9.6)
 *
 * Plan 2 hardcodes "1 damage per bullet" — future plans differentiate
 * per-weapon damage (Vulcan=1, Laser=2, Plasma=3) via a dispatch on
 * bullet.kind. */
void collide_player_bullets_vs_enemies(void);

#endif
```

- [ ] **Step 2: COLLIDE.CPP 작성**

```c
#include "COLLIDE.H"
#include "BULLET.H"
#include "ENEMY.H"
#include "EFFECT.H"

void collide_player_bullets_vs_enemies(void)
{
    int bi, ei;

    for (bi = 0; bi < MAX_PLAYER_BULLETS; bi++) {
        Bullet *b = &g_pbullets[bi];
        if (!b->active) continue;

        /* bullet AABB: (b->x, b->y) is top-left, (size_w, size_h) extent */
        int b_l = b->x;
        int b_t = b->y;
        int b_r = b->x + b->size_w;
        int b_b = b->y + b->size_h;

        for (ei = 0; ei < MAX_ENEMIES; ei++) {
            Enemy *e = &g_enemies[ei];
            if (!e->active) continue;
            if (e->plane != PLANE_AIR) continue;   /* Plan 2: only AIR matters anyway */

            /* enemy AABB: (e->x, e->y) is CENTER, half-extents (POPCORN_HALF_W/H).
             * Plan 2 only has popcorn, so half-extents are constants. Future
             * plans switch on e->type to look up per-type half-extents. */
            int e_l = e->x - POPCORN_HALF_W;
            int e_t = e->y - POPCORN_HALF_H;
            int e_r = e->x + POPCORN_HALF_W;
            int e_b = e->y + POPCORN_HALF_H;

            if (b_r <= e_l || b_l >= e_r || b_b <= e_t || b_t >= e_b) continue;

            /* hit */
            b->active = 0;
            if (e->hp > 1) {
                e->hp--;
            } else {
                e->active = 0;
                effect_spawn_explosion(e->x, e->y);
                g_state.score += 100;
            }
            break;   /* this bullet is gone; move to next bullet */
        }
    }
}
```

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -15
```

Expected: 0 errors. 16개 모듈 (이전 + COLLIDE).

- [ ] **Step 4: 커밋**

```bash
git add SRC/COLLIDE.H SRC/COLLIDE.CPP
git commit -m "Add COLLIDE module — player bullets vs air enemies

AABB sweep of MAX_PLAYER_BULLETS=64 vs MAX_ENEMIES=32 = 2048 checks
per frame worst case (well within budget). Bullet always deactivates
on hit; enemy hp decrements and only dies on hp == 0, at which point
an explosion effect spawns at the enemy center and score increments
by 100 (popcorn kill, spec §9.6).

Damage hardcoded at 1 (Vulcan); per-weapon damage dispatch deferred
to Plan 3 when LASER/PLASMA are added."
```

---

## Task 8: GAME.CPP 통합 + 임시 스폰 스케줄러

**Files:**
- Modify: `SRC/GAME.CPP`

새 모듈들의 init/close/update/render/collide를 메인 루프에 끼워넣고, 임시 스폰 스케줄러를 추가 (1초마다 popcorn 1대를 화면 위 랜덤 x에 스폰). STAGE 시스템은 후속 plan.

- [ ] **Step 1: GAME.CPP 수정 — 새 include + init/close 추가**

새 include (기존 `#include "WEAPON.H"` 다음):

```c
#include "ENEMY.H"
#include "EFFECT.H"
#include "COLLIDE.H"
```

`weapon_init()` 다음에 enemy/effect init 추가 (실패 시 LIFO 정리하고 return):

```c
    bullet_init();
    weapon_init();

    if (enemy_init() != 0) {
        player_close();
        bg_close();
        gfx_close();
        sfx_close();
        snd_close();
        input_close();
        timer_close();
        return 3;
    }

    if (effect_init() != 0) {
        enemy_close();
        player_close();
        bg_close();
        gfx_close();
        sfx_close();
        snd_close();
        input_close();
        timer_close();
        return 4;
    }
```

shutdown에 (LIFO, `player_close()` 앞):

```c
    effect_close();
    enemy_close();
    player_close();
    bg_close();
    ...
```

- [ ] **Step 2: 임시 스폰 스케줄러 + 메인 루프 업데이트/렌더 통합**

`g_last_ms` 옆에 새 정적 변수 추가:

```c
static u32 g_last_ms;
static u32 g_spawn_t_ms;       /* time-since-last-popcorn-spawn accumulator */

/* simple LCG for spawn x randomization (deterministic for repro) */
static u32 g_rng = 1;
static u32 next_rand(void) { g_rng = g_rng * 1103515245UL + 12345UL; return g_rng; }
```

`state_reset()`에서 spawn_t_ms 리셋 추가:

```c
    g_state.gs_t_ms = 0;
    g_spawn_t_ms = 0;
}
```

메인 루프의 `bg_update(dt);` 직전이나 직후에 스폰 로직 + enemy/effect/collide 추가:

```c
    while (!input_key(KEY_ESC)) {
        u32 now = timer_ms();
        u32 dt = now - g_last_ms;
        g_last_ms = now;

        /* Plan 2 test scaffold: spawn 1 popcorn every 1000ms at random x,
         * just above the play area. Replaced by STAGE in a later plan. */
        g_spawn_t_ms += dt;
        if (g_spawn_t_ms >= 1000) {
            g_spawn_t_ms -= 1000;
            i16 sx = (i16)(POPCORN_HALF_W + (i16)(next_rand() % (256 - POPCORN_W)));
            enemy_spawn(E_POPCORN_A, PLANE_AIR, PAT_STRAIGHT_DOWN,
                        sx, (i16)(-POPCORN_HALF_H),
                        0, /*vy*/ 24);   /* 1.5 px/frame downward */
        }

        bg_update(dt);
        player_update(dt);
        weapon_fire_tick(dt, input_key(KEY_Z));
        bullet_update_all(dt);
        enemy_update_all(dt);
        effect_update_all(dt);

        collide_player_bullets_vs_enemies();

        gfx_clear(0);
        bg_render();
        enemy_render_ground();   /* Plan 2: empty stub, but keeps order */
        bullet_render_player();
        enemy_render_air();
        player_render();
        effect_render();

        gfx_vsync();
        gfx_flip();
    }
```

> **렌더 순서 (spec §6.2)**:
> 1. 배경
> 2. 지상 적 (배경 위)
> 3. 플레이어 탄
> 4. 공중 적
> 5. 플레이어
> 6. 적탄 (Plan 3)
> 7. 아이템 (Plan 4+)
> 8. 보스 (Plan 8)
> 9. 이펙트 (모든 적/탄 위에)
>
> Plan 2는 1, 2(빈), 3, 4, 5, 9만 호출.

- [ ] **Step 3: 빌드 + 시각 검증** (run.sh는 사용자가)

```bash
./build.sh
cat BUILD.LOG | tail -10
ls -l SRC/GAME.EXE
```

Expected: 0 errors. GAME.EXE 갱신.

기대 동작:
- 사막 배경 스크롤 (이전과 동일)
- 1초마다 화면 위에서 popcorn 적 1대 등장 (랜덤 x, 4프레임 idle 애니)
- 적이 1.5 px/frame 속도로 아래로 직진
- Z를 누르면 Vulcan 탄이 발사되고 적에 맞으면 폭발 (16x16 5프레임 애니, ~400ms)
- 적이 화면 아래로 흘러가면 사라짐
- 점수는 내부적으로 +100 (화면 표시 X — Plan 3 또는 4에서 추가)

- [ ] **Step 4: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "Integrate ENEMY + EFFECT + COLLIDE — first kills

GAME.CPP main loop now spawns a popcorn enemy every 1000ms at random
x just above play area, with 1.5 px/frame downward velocity. Enemies
update via the AI dispatcher, render in the air-enemy layer, take
damage from player bullets, and explode (5-frame animation) on death
with a +100 score increment.

Render order matches spec §6.2: bg -> ground -> p_bullets -> air ->
player -> effects. Ground and the later layers (enemy bullets, items,
boss) remain stubs/absent until subsequent plans.

Plan 2 vertical slice complete."
```

---

## Task 9: 마무리 + 시각 검증 #4

**Files:** none

- [ ] **Step 1: 최종 동작 확인 (사용자 시각 검증)**

```bash
./run.sh
```

확인 사항:
- 매 1초 화면 위에서 popcorn 적 1대 등장 → 아래로 직진
- Z로 발사 → 적에 적중 시 1발 데미지 (popcorn HP=2, 2발 필요)
- HP 0 시 적 사라지고 폭발 5프레임 애니 재생
- 동시에 여러 적 / 여러 폭발 OK (풀 32 / 24)
- ESC 정상 종료

- [ ] **Step 2: git log 점검**

```bash
git log --oneline -10
```

Expected: Plan 2의 ~9개 커밋이 main에 있음 (Plan 1 끝 `424f027` 위에).

---

## Self-Review Checklist

- [ ] **Spec coverage** (M4-M5):
  - ENEMY 구조체 + AIR/GROUND 구분: ✓ (Plan 2는 AIR만 인스턴스화)
  - 1 적 종류 (popcorn) + 1 패턴 (PAT_STRAIGHT_DOWN): ✓
  - 충돌 (player_bullet vs air_enemy): ✓
  - 폭발 5프레임 애니: ✓
  - 점수 가산 (popcorn 100): ✓
  - 적 탄 / 메달 / HUD / 사망: 의도적 제외 (후속 plan)
- [ ] **타입 일관성**:
  - `i16 x, y` 일관 (Player, Enemy, Effect, Bullet)
  - `u32 dt_ms` 일관
  - 충돌은 모두 AABB
- [ ] **함수 시그니처 매칭**:
  - enemy_spawn (EnemyType, EnemyPlane, EnemyPattern, i16 x, i16 y, i16 vx_q4, i16 vy_q4) → int
  - effect_spawn_explosion (i16 x, i16 y) → int
  - collide_player_bullets_vs_enemies (void) → void
- [ ] **빌드 시스템**: 신규 *.CPP 자동 컴파일 (WCL386 *.CPP)
- [ ] **자산 경로**: 모두 SRC/ 안 (`SPR_EPOP.SPR`, `SPR_EXPL.SPR` — 짧은 8.3 이름)

---

## Open Items (Plan 3 인계)

- 적 탄 풀 (MAX_ENEMY_BULLETS=256) + PAT_AIM_PLAYER 패턴
- 플레이어 vs 적탄 충돌 (히트박스 4x4)
- 사망/부활/무적 깜빡임
- 폭발 SFX (현재 무음)
- 점수 화면 표시 (FONT 자산 파이프라인 셋업: ENG_HGDIARY.FNT/HAN_GOTHIC.FNT 소스 확보)
