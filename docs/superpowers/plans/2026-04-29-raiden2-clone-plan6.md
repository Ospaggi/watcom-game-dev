# Raiden 2 Clone — Plan 6: 무기 다양화 (V/L/P L1) + P 아이템 + 색순환

> **For agentic workers:** Use superpowers:subagent-driven-development. Steps use `- [ ]` checkbox tracking.

**Goal:** Vulcan + Laser + Plasma 3 무기 (모두 L1만), P 아이템이 화면 위에서 떠다니고, 플레이어 탄에 맞으면 색이 빨강(V) → 파랑(L) → 보라(P) → 빨강 순환, 먹으면 그 무기로 전환 + 10000점.

**Architecture:** 신규 모듈 1 (ITEM), BULLET/WEAPON/COLLIDE/GAME/HUD 확장. 무기 레벨 (L2~L4)은 후속 plan. B/H 픽업, 메달, 1UP 도 후속.

**Tech Stack:** Watcom C++, GFX/SPRITE/SFX 그대로.

**Spec 참조:** `docs/superpowers/specs/2026-04-28-raiden2-clone-design.md` §4.3 (메인 무기), §9.1 (P 색순환)

---

## File Map

### NEW FILES

| 경로 | 책임 |
|---|---|
| `SRC/ITEM.H` / `SRC/ITEM.CPP` | Item struct, 32-슬롯 풀, P 아이템 spawn/update/render + 색순환 헬퍼 |

### GENERATED FILES

| 경로 | 출처 |
|---|---|
| `SRC/SPR_PWUP.SPR` | `ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_0~14.png` (15 frames 11x27) montage → 165x27 sheet |

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `SRC/BULLET.H` / `SRC/BULLET.CPP` | `BUL_PLAYER_LASER`, `BUL_PLAYER_PLASMA` 추가 (탄 종류별 색/크기) |
| `SRC/WEAPON.H` / `SRC/WEAPON.CPP` | `g_state.weapon` 디스패치, `fire_laser_l1` + `fire_plasma_l1` |
| `SRC/COLLIDE.H` / `SRC/COLLIDE.CPP` | `collide_player_bullets_vs_items()`, `collide_player_vs_items()` |
| `SRC/GAME.CPP` | ITEM init/close/update/render + 5초마다 P 아이템 스폰 + 충돌 호출 + 렌더 통합 |
| `SRC/HUD.CPP` | 잔기/봄 옆에 작은 무기 색 표시 (V=빨강, L=파랑, P=보라 사각형) |
| `convert.sh` | montage_frames.sh + mksprite for pwupitem |

### NOT TOUCHED

- 무기 레벨 (L2~L4): Plan 7+
- B/H 아이템 픽업: Plan 7+
- 메달 시스템: Plan 7+
- 호밍 미사일 자동 발사: Plan 7+

---

## Task 1: pwupitem → SPR_PWUP.SPR + convert.sh 갱신

**Files:**
- Generated: `SRC/SPR_PWUP.SPR`
- Modify: `convert.sh`

15 프레임 회전 토큰 애니메이션. 각 프레임 11x27. montage → 165x27 → mksprite --grid 15x1.

- [ ] **Step 1: convert.sh 갱신**

`convert.sh`에서 explosion 변환 단계 다음, "Done." 직전에 추가:

```bash
echo "Packing power-up icon frames..."
tools/montage_frames.sh _workspace/pwup_packed.png \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_0.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_1.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_2.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_3.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_4.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_5.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_6.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_7.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_8.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_9.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_10.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_11.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_12.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_13.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted/Power up ship item/pwupitem_14.png"

echo "Converting power-up icon sprite..."
bun tools/mksprite.ts --bin --grid 15x1 _workspace/pwup_packed.png SPR_PWUP
```

- [ ] **Step 2: 실행 + 확인**

```bash
./convert.sh
ls -lh SRC/SPR_PWUP.SPR
sips -g pixelWidth -g pixelHeight _workspace/pwup_packed.png | tail -2
```

Expected: pwup_packed.png = 165x27. SPR_PWUP.SPR > 100 bytes.

- [ ] **Step 3: 커밋**

```bash
git add convert.sh
git commit -m "Convert pwupitem 15-frame icon to compiled SPR

pwupitem_0..14.png (15 frames 11x27 from jinvorionstg) -> 165x27
horizontal strip -> SRC/SPR_PWUP.SPR (15 compiled frames). Used by
ITEM module for the rotating power-up token. Frames cycle to give
the visual rotation effect."
```

---

## Task 2: BULLET — Laser + Plasma 종류 추가

**Files:**
- Modify: `SRC/BULLET.H`, `SRC/BULLET.CPP`

탄 종류별 크기/색을 spawn 시점에 다르게. 동일한 `g_pbullets[]` 풀 공유.

- [ ] **Step 1: BULLET.H — BulletKind enum 확장**

```c
typedef enum {
    BUL_PLAYER_VULCAN = 0,
    BUL_PLAYER_LASER  = 1,
    BUL_PLAYER_PLASMA = 2,
    BUL_ENEMY_AIMED   = 3
    /* future: BUL_HOMING, BUL_ENEMY_DOT, BUL_ENEMY_RICE, ... */
} BulletKind;
```

> **주의**: 기존 `BUL_ENEMY_AIMED = 1` 이 `= 3`으로 변경됨. 이 값은 BULLET.CPP의 `bullet_spawn_enemy()`에서만 직접 참조되고, 다른 곳에서 정수 리터럴이 쓰이지 않으므로 안전.

- [ ] **Step 2: BULLET.CPP — bullet_spawn_player를 kind별로 분기**

기존 `bullet_spawn_player`는 모든 탄을 같은 색/크기로 spawn했음. 이를 kind에 따라 다르게:

```c
int bullet_spawn_player(BulletKind kind, i16 x, i16 y, i16 vx_q4, i16 vy_q4)
{
    int i;
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        Bullet *b = &g_pbullets[i];
        if (b->active) continue;
        b->active = 1;
        b->kind = (u8)kind;
        switch (kind) {
        case BUL_PLAYER_VULCAN:
            b->color_idx = 25;     /* bright red */
            b->size_w = 2; b->size_h = 6;
            break;
        case BUL_PLAYER_LASER:
            b->color_idx = 158;    /* cyan-ish (palette guess; may need tuning) */
            b->size_w = 2; b->size_h = 12;
            break;
        case BUL_PLAYER_PLASMA:
            b->color_idx = 142;    /* magenta-ish (palette guess) */
            b->size_w = 4; b->size_h = 4;
            break;
        default:
            b->color_idx = 25;
            b->size_w = 2; b->size_h = 6;
            break;
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

> **참고**: `color_idx` 158/142는 palette.json의 실제 cyan/magenta 인덱스를 추정한 값. 실제 색이 어색하면 PALETTE.H를 보고 조정.

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -8
```

Expected: 0 errors.

- [ ] **Step 4: 커밋**

```bash
git add SRC/BULLET.H SRC/BULLET.CPP
git commit -m "Extend BULLET — add Laser and Plasma player bullet kinds

BUL_PLAYER_LASER  (2x12 cyan beam, fast fire rate)
BUL_PLAYER_PLASMA (4x4 magenta ball)

bullet_spawn_player now switch-dispatches color/size by kind. No
behavior change for Vulcan (still 2x6 red). BUL_ENEMY_AIMED was
moved from value 1 to 3 — no callers use the literal so this is
safe.

Damage is still hardcoded 1 in COLLIDE; per-weapon damage (Vulcan=1,
Laser=2, Plasma=3) deferred to the levels plan."
```

---

## Task 3: WEAPON — g_state.weapon 디스패치 + Laser/Plasma L1

**Files:**
- Modify: `SRC/WEAPON.H`, `SRC/WEAPON.CPP`

기존 weapon_fire_tick은 Vulcan만 발사. 이제 g_state.weapon 값에 따라 분기.

- [ ] **Step 1: WEAPON.CPP — fire 함수 3종 + dispatch**

기존 `fire_vulcan_l1`은 그대로. 새 함수 추가 (위에 같은 스타일로):

```c
#define VULCAN_PERIOD_MS  80
#define LASER_PERIOD_MS   50
#define PLASMA_PERIOD_MS 100

#define VULCAN_VY_Q4    (-96)   /* -6 px/frame upward */
#define LASER_VY_Q4    (-128)   /* -8 px/frame (faster) */
#define PLASMA_VY_Q4    (-80)   /* -5 px/frame (slower) */
```

(VULCAN_PERIOD_MS / VULCAN_VY_Q4 는 기존 그대로, 다른 두 개 신규)

새 함수:

```c
static void fire_laser_l1(void)
{
    i16 px = g_player.x - 1;
    i16 py = g_player.y - PLAYER_HALF_H - 12;
    bullet_spawn_player(BUL_PLAYER_LASER, px, py, 0, LASER_VY_Q4);
    sfx_play(SFX_ATK);
}

static void fire_plasma_l1(void)
{
    i16 px = g_player.x - 2;
    i16 py = g_player.y - PLAYER_HALF_H - 4;
    bullet_spawn_player(BUL_PLAYER_PLASMA, px, py, 0, PLASMA_VY_Q4);
    sfx_play(SFX_ATK);
}
```

`weapon_fire_tick`을 dispatch 형태로 변경:

```c
void weapon_fire_tick(u32 dt_ms, int z_held)
{
    /* decrement cooldown */
    if (g_vulcan_cd_ms > dt_ms) g_vulcan_cd_ms -= dt_ms;
    else                         g_vulcan_cd_ms = 0;

    if (!z_held) return;
    if (g_vulcan_cd_ms > 0) return;

    switch (g_state.weapon) {
    case PW_VULCAN:
        fire_vulcan_l1();
        g_vulcan_cd_ms = VULCAN_PERIOD_MS;
        break;
    case PW_LASER:
        fire_laser_l1();
        g_vulcan_cd_ms = LASER_PERIOD_MS;
        break;
    case PW_PLASMA:
        fire_plasma_l1();
        g_vulcan_cd_ms = PLASMA_PERIOD_MS;
        break;
    }
}
```

> **참고**: 변수명 `g_vulcan_cd_ms`는 이제 모든 무기 공용 쿨다운으로 의미 변경됨. 이름은 향후 `g_weapon_cd_ms`로 리네임 가능 (이 plan에서는 그대로 둠 — 작업 최소화).

- [ ] **Step 2: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -8
```

Expected: 0 errors.

- [ ] **Step 3: 커밋**

```bash
git add SRC/WEAPON.CPP
git commit -m "Dispatch weapon firing on g_state.weapon (V/L/P all L1)

Adds fire_laser_l1 (12px tall cyan beam, 50ms cooldown, -8 px/frame)
and fire_plasma_l1 (4x4 magenta ball, 100ms cooldown, -5 px/frame).
weapon_fire_tick now switches on g_state.weapon (set by P pickup in
Task 5/6). Cooldown still uses one variable g_vulcan_cd_ms — kept
the name to minimize churn; a rename is trivially deferred.

Levels L2-L4 (spread/beam variants) are deferred."
```

---

## Task 4: SRC/ITEM.H + ITEM.CPP — P 아이템 풀

**Files:**
- Create: `SRC/ITEM.H`, `SRC/ITEM.CPP`

P 아이콘이 위에서 등장 → 천천히 아래로 흘러감 (vy = +0.3 px/frame ≈ 5 q4) + sine 흔들림. 색순환 cooldown 상태 추적. 15프레임 회전 애니.

- [ ] **Step 1: ITEM.H 작성**

```c
#ifndef ITEM_H_INCLUDED
#define ITEM_H_INCLUDED

#include "STATE.H"

/* Item kinds (Plan 6: only ITEM_POWER). */
typedef enum {
    ITEM_POWER = 0
    /* future: ITEM_BOMB, ITEM_HOMING, ITEM_MEDAL_S, ITEM_MEDAL_L, ITEM_1UP */
} ItemKind;

/* Power-up sprite size (matches pwupitem 11x27 frames). */
#define PWUP_W       11
#define PWUP_H       27
#define PWUP_HALF_W   5   /* (11/2 rounded down) */
#define PWUP_HALF_H  13   /* (27/2 rounded down) */
#define PWUP_FRAMES  15

#define MAX_ITEMS 32

/* Position is the CENTER of the icon, in play-area coords. */
typedef struct {
    u8  active;
    u8  kind;            /* ItemKind */
    u8  pw_color;        /* PowerWeapon: PW_VULCAN/LASER/PLASMA when kind=ITEM_POWER */
    u8  frame;           /* current animation frame (0..14) */
    u32 anim_t_ms;
    u16 cycle_cd_ms;     /* color-cycle cooldown after a player bullet hit (300ms) */
    u32 spawn_t_ms;      /* used for sine wobble */
    i16 x, y;            /* center, play-area coords */
    i16 vy_q4;           /* downward velocity (no horizontal — wobble is render-time) */
    i16 spawn_x;         /* anchor x for sine wobble */
} Item;

extern Item g_items[MAX_ITEMS];

int  item_init(void);
void item_close(void);

/* Spawn a power-up at center (x, y). Returns 1 on success, 0 if pool full. */
int  item_spawn_power(i16 x, i16 y, PowerWeapon initial_color);

void item_update_all(u32 dt_ms);
void item_render(void);

/* Cycle a P icon's color (called from collision when player bullet hits it). */
void item_cycle_power_color(Item *e);

#endif
```

- [ ] **Step 2: ITEM.CPP 작성**

```c
#include "ITEM.H"
#include "GFX.H"
#include "SPRITE.H"

#define PLAY_X0    32
#define PLAY_W    256
#define PLAY_H    200

#define PWUP_ANIM_PERIOD_MS  60
#define CYCLE_COOLDOWN_MS   300
#define WOBBLE_AMP_PX         8     /* sine amplitude in px */
#define WOBBLE_PERIOD_MS   2000     /* one full sine cycle */

Item g_items[MAX_ITEMS];
static Sprite g_spr_pwup;

/* Outline color per weapon (drawn around the gray P icon to indicate the
 * cycle state). Palette indices are guesses — adjust after visual check. */
static unsigned char outline_color(u8 pw_color)
{
    switch (pw_color) {
    case PW_VULCAN: return 25;   /* red */
    case PW_LASER:  return 158;  /* cyan */
    case PW_PLASMA: return 142;  /* magenta */
    default:        return 15;   /* white fallback */
    }
}

int item_init(void)
{
    int i;
    for (i = 0; i < MAX_ITEMS; i++) g_items[i].active = 0;
    if (spr_load("SPR_PWUP.SPR", &g_spr_pwup) != SPR_OK) return -1;
    return 0;
}

void item_close(void)
{
    spr_free(&g_spr_pwup);
}

int item_spawn_power(i16 x, i16 y, PowerWeapon initial_color)
{
    int i;
    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &g_items[i];
        if (it->active) continue;
        it->active = 1;
        it->kind = (u8)ITEM_POWER;
        it->pw_color = (u8)initial_color;
        it->frame = 0;
        it->anim_t_ms = 0;
        it->cycle_cd_ms = 0;
        it->spawn_t_ms = 0;
        it->x = x;
        it->y = y;
        it->vy_q4 = 5;     /* about 0.3 px/frame downward */
        it->spawn_x = x;
        return 1;
    }
    return 0;
}

void item_cycle_power_color(Item *it)
{
    if (it->cycle_cd_ms > 0) return;   /* still cooling down */
    /* V -> L -> P -> V */
    it->pw_color = (u8)((it->pw_color + 1) % 3);
    it->cycle_cd_ms = CYCLE_COOLDOWN_MS;
}

/* Tiny sine LUT for wobble (16 samples over a full period, scaled to
 * WOBBLE_AMP_PX). Avoids float math. Index = (t_ms / (period/16)) % 16. */
static const i16 sine16[16] = {
     0,  3,  6,  7,  8,  7,  6,  3,
     0, -3, -6, -7, -8, -7, -6, -3,
};

static i16 wobble_offset(u32 t_ms)
{
    int idx = (int)((t_ms / (WOBBLE_PERIOD_MS / 16)) & 15);
    return sine16[idx];
}

void item_update_all(u32 dt_ms)
{
    int i;
    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &g_items[i];
        if (!it->active) continue;

        it->spawn_t_ms += dt_ms;

        /* downward drift (q4 subpixel — accumulator-less since vy is constant) */
        it->y += it->vy_q4 / 4;   /* coarse: 5/4 = 1 px/4-frames; close enough */

        /* sine wobble around spawn_x */
        it->x = it->spawn_x + wobble_offset(it->spawn_t_ms);

        /* deactivate if scrolled off bottom */
        if (it->y - PWUP_HALF_H > PLAY_H) it->active = 0;

        /* cycle cooldown */
        if (it->cycle_cd_ms > dt_ms) it->cycle_cd_ms -= (u16)dt_ms;
        else                          it->cycle_cd_ms = 0;

        /* animation frame cycle (15 frames -> rotation effect) */
        it->anim_t_ms += dt_ms;
        if (it->anim_t_ms >= PWUP_ANIM_PERIOD_MS) {
            it->anim_t_ms -= PWUP_ANIM_PERIOD_MS;
            it->frame = (u8)((it->frame + 1) % PWUP_FRAMES);
        }
    }
}

void item_render(void)
{
    int i;
    for (i = 0; i < MAX_ITEMS; i++) {
        const Item *it = &g_items[i];
        if (!it->active) continue;

        int sx = PLAY_X0 + it->x - PWUP_HALF_W;
        int sy = it->y - PWUP_HALF_H;
        if (sx < 0 || sy < 0 || sx + PWUP_W > 320 || sy + PWUP_H > 200) continue;

        /* outline rectangle indicating current cycle color */
        unsigned char c = outline_color(it->pw_color);
        gfx_rect(sx - 1, sy - 1, PWUP_W + 2, PWUP_H + 2, c);

        /* the rotating gray icon */
        gfx_draw_csprite(g_spr_pwup.frames[it->frame], sx, sy);
    }
}
```

> **참고**: `gfx_rect(int x, int y, int w, int h, color)` is the engine's rectangle outline (1px stroke). Verify it exists in GFX.H — yes it does (`void gfx_rect(int x, int y, int w, int h, unsigned char color);`).

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
```

Expected: 0 errors.

- [ ] **Step 4: 커밋**

```bash
git add SRC/ITEM.H SRC/ITEM.CPP
git commit -m "Add ITEM module — P power-up icon with color cycle

32-slot pool. P icon spawns drift downward (~0.3 px/frame), sine
wobble +/- 8 px around spawn_x (16-sample LUT, 2s period). 15-frame
rotation animation cycling every 60ms.

item_cycle_power_color advances the icon's pw_color (V -> L -> P -> V)
with a 300ms cooldown to prevent rapid Vulcan strafe from skipping
it. Render draws a 1px colored outline (red/cyan/magenta) around the
gray sprite as the visual cycle indicator (proper recolor of the
sprite itself is deferred until we either pre-compile color variants
or add gfx_blit_recolor)."
```

---

## Task 5: COLLIDE — 플레이어 탄 vs 아이템 (색순환), 플레이어 vs 아이템 (픽업)

**Files:**
- Modify: `SRC/COLLIDE.H`, `SRC/COLLIDE.CPP`

- [ ] **Step 1: COLLIDE.H — 새 함수 선언**

기존 두 collide 함수 다음에 추가:

```c
/* Player bullets vs power-up items: cycles each P item's color when hit
 * (with the item's internal cooldown). Bullets are NOT consumed (per
 * spec: shooting the icon doesn't destroy it, only cycles). */
void collide_player_bullets_vs_items(void);

/* Player center vs items: on contact, picks up the P (switches
 * g_state.weapon, +10000 score, deactivates the item). */
void collide_player_vs_items(void);
```

- [ ] **Step 2: COLLIDE.CPP — 두 함수 구현**

새 include:

```c
#include "ITEM.H"
```

파일 끝에 추가:

```c
void collide_player_bullets_vs_items(void)
{
    int bi, ii;

    for (bi = 0; bi < MAX_PLAYER_BULLETS; bi++) {
        Bullet *b = &g_pbullets[bi];
        if (!b->active) continue;

        int b_l = b->x;
        int b_t = b->y;
        int b_r = b->x + b->size_w;
        int b_b = b->y + b->size_h;

        for (ii = 0; ii < MAX_ITEMS; ii++) {
            Item *it = &g_items[ii];
            if (!it->active) continue;
            if (it->kind != ITEM_POWER) continue;

            int e_l = it->x - PWUP_HALF_W;
            int e_t = it->y - PWUP_HALF_H;
            int e_r = it->x + PWUP_HALF_W;
            int e_b = it->y + PWUP_HALF_H;

            if (b_r <= e_l || b_l >= e_r || b_b <= e_t || b_t >= e_b) continue;

            /* hit: cycle the item color (bullet stays active per spec) */
            item_cycle_power_color(it);
            /* don't break — one bullet could touch multiple items in a frame,
             * though that's unlikely. */
        }
    }
}

void collide_player_vs_items(void)
{
    int ii;

    if (!g_player.alive) return;

    /* player AABB uses the sprite's full extent for pickups (more generous
     * than the 4x4 damage hitbox — players want easy pickup). */
    int p_l = g_player.x - PLAYER_HALF_W;
    int p_t = g_player.y - PLAYER_HALF_H;
    int p_r = g_player.x + PLAYER_HALF_W;
    int p_b = g_player.y + PLAYER_HALF_H;

    for (ii = 0; ii < MAX_ITEMS; ii++) {
        Item *it = &g_items[ii];
        if (!it->active) continue;
        if (it->kind != ITEM_POWER) continue;

        int e_l = it->x - PWUP_HALF_W;
        int e_t = it->y - PWUP_HALF_H;
        int e_r = it->x + PWUP_HALF_W;
        int e_b = it->y + PWUP_HALF_H;

        if (p_r <= e_l || p_l >= e_r || p_b <= e_t || p_t >= e_b) continue;

        /* pickup: switch weapon + score */
        g_state.weapon = it->pw_color;
        g_state.score += 10000;
        it->active = 0;
    }
}
```

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -8
```

- [ ] **Step 4: 커밋**

```bash
git add SRC/COLLIDE.H SRC/COLLIDE.CPP
git commit -m "Add player bullets vs items + player vs items collision

collide_player_bullets_vs_items: AABB sweep, on hit calls
item_cycle_power_color (which respects the item's 300ms cooldown).
Bullets are NOT consumed.

collide_player_vs_items: pickup test using full player sprite extent
(more generous than the 4x4 damage hitbox). On pickup: switches
g_state.weapon to the item's pw_color, grants +10000 score, and
deactivates the item slot.

Both functions are called from GAME.CPP in Task 6."
```

---

## Task 6: GAME.CPP — ITEM init/close + 스폰 + 충돌 + 렌더 통합

**Files:**
- Modify: `SRC/GAME.CPP`

- [ ] **Step 1: 새 include 추가**

```c
#include "ITEM.H"
```

- [ ] **Step 2: init/close 통합**

`effect_init()` 다음에 item_init 추가 (실패 시 LIFO 정리하고 return 5):

```c
    if (effect_init() != 0) {
        ...
        return 4;
    }

    if (item_init() != 0) {
        effect_close();
        enemy_close();
        player_close();
        bg_close();
        gfx_close();
        sfx_close();
        snd_close();
        input_close();
        timer_close();
        return 5;
    }
```

shutdown LIFO에 추가 (item_close 가 effect_close 직전):

```c
    snd_stop();
    item_close();
    effect_close();
    enemy_close();
    ...
```

- [ ] **Step 3: P 아이템 스폰 스케줄러**

새 정적 변수 (g_spawn_t_ms 옆):

```c
static u32 g_item_spawn_t_ms;       /* P-item drop accumulator */
```

`state_reset()` 끝에 추가:

```c
    g_spawn_t_ms = 0;
    g_item_spawn_t_ms = 0;
```

메인 루프의 적 스폰 스케줄러 다음에 P 스폰:

```c
        /* Plan 6 test scaffold: spawn 1 P icon every 5000ms at random x
         * near the top, with random initial color. */
        g_item_spawn_t_ms += dt;
        if (g_item_spawn_t_ms >= 5000) {
            g_item_spawn_t_ms -= 5000;
            i16 ix = (i16)(PWUP_HALF_W + (i16)(next_rand() % (256 - PWUP_W)));
            PowerWeapon col = (PowerWeapon)(next_rand() % 3);
            item_spawn_power(ix, (i16)(-PWUP_HALF_H), col);
        }
```

- [ ] **Step 4: update + collide + render 통합**

`effect_update_all(dt);` 다음에 item_update_all 추가:

```c
        if (g_player.alive) {
            enemy_update_all(dt);
            effect_update_all(dt);
            item_update_all(dt);
        }
```

collide 블록에 두 새 호출 추가:

```c
        if (g_player.alive) {
            collide_player_bullets_vs_enemies();
            collide_enemy_bullets_vs_player();
            collide_player_bullets_vs_items();
            collide_player_vs_items();
        }
```

렌더 순서에 item_render 추가 (effect_render 직전 — 아이템이 폭발 아래에):

```c
        gfx_clear(0);
        bg_render();
        enemy_render_ground();
        bullet_render_player();
        enemy_render_air();
        player_render();
        bullet_render_enemy();
        item_render();              /* NEW: items above enemy bullets */
        effect_render();
        hud_render();
```

- [ ] **Step 5: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
ls -l SRC/GAME.EXE
```

Expected: 0 errors.

- [ ] **Step 6: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "Wire ITEM module — P icon spawn + collisions + render

P icons spawn every 5000ms at random x just above play area, with
a random initial color (V/L/P). Update/collide/render are wired
into the main loop:

  update:  item_update_all (only when alive)
  collide: collide_player_bullets_vs_items + collide_player_vs_items
  render:  item_render (between bullet_render_enemy and effect_render)

init returns 5 on failure (matches the existing return-code series).
LIFO shutdown places item_close before effect_close."
```

---

## Task 7: HUD — 현재 무기 표시

**Files:**
- Modify: `SRC/HUD.CPP`

좌측 HUD에 작은 무기 색 표시기 추가. y=28..32에 4x4 colored square (V=빨강, L=청, P=마젠타).

- [ ] **Step 1: HUD.CPP 확장**

기존 `hud_render()` 끝에 추가:

```c
    /* Weapon indicator (4x4 colored square at y=28..31, x=12..15). */
    unsigned char wcol;
    switch (g_state.weapon) {
    case PW_VULCAN: wcol = 25;  break;
    case PW_LASER:  wcol = 158; break;
    case PW_PLASMA: wcol = 142; break;
    default:        wcol = 15;  break;
    }
    gfx_fill_rect(12, 28, 4, 4, wcol);
```

(라벨 "W" 글자는 폰트가 없으므로 색 사각형으로 충분.)

- [ ] **Step 2: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -8
```

- [ ] **Step 3: 커밋**

```bash
git add SRC/HUD.CPP
git commit -m "HUD — show current weapon color indicator

4x4 colored square at HUD x=12, y=28: red=Vulcan, cyan=Laser,
magenta=Plasma. Color matches the bullet color and the P-icon
outline so it's immediately legible across the screen."
```

---

## Task 8: 시각/청각 검증

```bash
./run.sh
```

확인 사항:
- 5초마다 P 아이콘 등장 (15프레임 회전 + sine 흔들리며 하강)
- P 아이콘 주위 1px 색 외곽선 (빨강/청/마젠타) — 현재 cycle 색
- 자기 탄을 P에 맞히면 색이 V→L→P→V로 순환 (300ms 쿨다운)
- P를 먹으면:
  - 무기가 그 색으로 전환 (탄 색/모양 변화)
  - HUD 좌측 무기 색 사각형이 새 색으로 바뀜
  - 점수 +10000
- Vulcan = 빨강 2x6 탄 (80ms), Laser = 청 2x12 빔 (50ms 빠른), Plasma = 마젠타 4x4 (100ms 느린)
- 적/배경/적탄/사망/봄 모두 이전과 동일

---

## Self-Review Checklist

- [ ] **Spec coverage**:
  - V/L/P 무기 (L1만): ✓
  - P 색순환: ✓ (visual은 outline 색만 — full sprite recolor는 후속)
  - P 픽업 → 무기 전환: ✓
  - 픽업 +10000점: ✓
  - 무기 레벨업: ✗ (Plan 7+)
  - 봄/호밍 픽업, 메달, 1UP: ✗
- [ ] **타입 일관성**: i16 좌표, q4 subpixel — Plan 1-3과 일관
- [ ] **렌더 순서**: bg → ground → p_bul → air → player → e_bul → item → effect → hud
- [ ] **빌드 시스템**: ITEM.CPP 자동 컴파일

---

## Open Items (Plan 7 인계)

- 무기 레벨 1~4 (Vulcan 확산 → 부채꼴, Laser 빔 굵기 → 휘는 빔, Plasma 1~3볼)
- B 아이템 (봄 픽업) + H 아이템 (보너스 점수)
- 메달 시스템 (누적 가치 100→10000)
- 호밍 미사일 자동 발사
- P 아이콘 스프라이트 자체의 색 변화 (gfx_blit_recolor 또는 색별 SPR 컴파일)
