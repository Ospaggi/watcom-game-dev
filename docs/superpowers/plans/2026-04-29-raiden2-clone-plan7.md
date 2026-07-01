# Raiden 2 Clone — Plan 7: 1면 보스 (Tank2) + Stage Clear

> **For agentic workers:** Use superpowers:subagent-driven-development. Steps use `- [ ]` checkbox tracking.

**Goal:** 30초 플레이 후 Tank2 보스 등장. 본체(70x70 3프레임) + 헤드(70x70 1프레임). HP 200, 3 페이즈 (1탄/3탄팬/5탄팬). 격파 시 다중 폭발 + STAGE CLEAR freeze. 봄 데미지 50.

**Architecture:** 신규 모듈 1 (BOSS). COLLIDE 확장 (탄 vs 보스, 보스 vs 플레이어). GAME.CPP 스폰 스케줄러 변경 (30초 후 popcorn 중지 + 보스 스폰). HUD 확장 (보스 HP 바). 보스 격파 → g_state.gs = GS_STAGE_CLEAR로 freeze 화면.

**Tech Stack:** Watcom C++, GFX/SPRITE 그대로.

**Spec 참조:** `docs/superpowers/specs/2026-04-28-raiden2-clone-design.md` §5.3 (보스), §11.3 (Tank2 페이즈), §12 (FSM, 부분 사용)

---

## File Map

### NEW FILES

| 경로 | 책임 |
|---|---|
| `SRC/BOSS.H` / `SRC/BOSS.CPP` | Boss 단일 인스턴스, 3-페이즈 FSM, 본체+헤드 렌더, AI 발사 |

### GENERATED FILES

| 경로 | 출처 |
|---|---|
| `SRC/SPR_BTNK_B.SPR` | `Tank2_1.png + Tank2_2.png + Tank2_3.png` (3 frames 70x70 → 210x70 sheet) |
| `SRC/SPR_BTNK_H.SPR` | `tank2_head.png` (70x70 single, --grid 1x1) |

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `convert.sh` | montage + mksprite for Tank2 body + head |
| `SRC/COLLIDE.H` / `SRC/COLLIDE.CPP` | `collide_player_bullets_vs_boss()`, `collide_boss_vs_player()` |
| `SRC/PLAYER.CPP` | `blast_all_enemies()` 봄 발동 시 보스에 50 데미지 |
| `SRC/GAME.CPP` | 30초 후 보스 스폰, 보스전 동안 popcorn 중지, 보스 격파 시 STAGE_CLEAR freeze |
| `SRC/HUD.CPP` | 보스 HP 바 (보스전 중에만 표시) — 화면 상단 가로 32~287 |

### NOT TOUCHED

- 보스 헤드 회전 (사용자 추적 시각 회전): 단일 프레임으로 시작, 회전은 후속 plan
- 정식 FSM (GS_BOSS_INTRO 인트로 시퀀스, GS_CONTINUE_PROMPT 등): Plan 8
- 2면/3면 보스 (MG, Kl): Plan 9+
- 보스 점수 보너스 + 잔여 봄/잔기 보너스: Plan 8 (FSM에서 STAGE_CLEAR 처리 시)

---

## Task 1: Tank2 sprite 변환

**Files:**
- Generated: `SRC/SPR_BTNK_B.SPR`, `SRC/SPR_BTNK_H.SPR`
- Modify: `convert.sh`

- [ ] **Step 1: convert.sh 갱신**

기존 explosion 단계 다음, "Done." 직전에 추가:

```bash
echo "Packing Tank2 boss body frames..."
tools/montage_frames.sh _workspace/btnk_body_packed.png \
  "ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium tiny enemies Asset pack/Tank2_1.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium tiny enemies Asset pack/Tank2_2.png" \
  "ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium tiny enemies Asset pack/Tank2_3.png"

echo "Converting Tank2 boss body sprite..."
bun tools/mksprite.ts --bin --grid 3x1 _workspace/btnk_body_packed.png SPR_BTNK_B

echo "Converting Tank2 boss head sprite..."
bun tools/mksprite.ts --bin --grid 1x1 \
  "ASSETS/raw/jinvorionstg_enemies/extracted_full/Medium tiny enemies Asset pack/tank2_head.png" \
  SPR_BTNK_H
```

- [ ] **Step 2: 실행 + 결과 확인**

```bash
./convert.sh
ls -lh SRC/SPR_BTNK_B.SPR SRC/SPR_BTNK_H.SPR
```

Expected stderr lines:
- `/* SRC/SPR_BTNK_B.SPR: 210x70 sheet, 3x1 grid, 3 frames of 70x70 */`
- `/* SRC/SPR_BTNK_H.SPR: 70x70 sheet, 1x1 grid, 1 frames of 70x70 */`

Both SPR > 1KB (70x70 with detail).

- [ ] **Step 3: 커밋**

```bash
git add convert.sh
git commit -m "Convert Tank2 boss sprites (body 3 frames + head 1 frame)

Tank2_1..3.png (3 body frames 70x70 from jinvorionstg) -> 210x70
horizontal strip -> SRC/SPR_BTNK_B.SPR (rolling tracks animation).
tank2_head.png -> SRC/SPR_BTNK_H.SPR (single 70x70 turret head).
Used by BOSS module for the Stage 1 boss."
```

---

## Task 2: SRC/BOSS.H + BOSS.CPP — 보스 모듈

**Files:**
- Create: `SRC/BOSS.H`, `SRC/BOSS.CPP`

단일 인스턴스 보스. 3 페이즈, slide LR 이동, 페이즈별 발사 패턴 (1/3/5 fan). HP 200. 본체 + 헤드 렌더.

- [ ] **Step 1: BOSS.H 작성**

```c
#ifndef BOSS_H_INCLUDED
#define BOSS_H_INCLUDED

#include "STATE.H"

/* Boss types (Plan 7: only Tank2). */
typedef enum {
    BOSS_TANK2 = 0
    /* future: BOSS_MG (stage 2), BOSS_KL (stage 3) */
} BossType;

/* Tank2 sprite + hitbox. Body 70x70 + head 70x70 same center. */
#define BOSS_TANK2_W       70
#define BOSS_TANK2_H       70
#define BOSS_TANK2_HALF_W  35
#define BOSS_TANK2_HALF_H  35
#define BOSS_TANK2_HP_MAX 200
#define BOSS_TANK2_BODY_FRAMES 3

#define BOSS_BODY_ANIM_PERIOD_MS 100   /* track-rolling animation */

typedef struct {
    u8  active;          /* 0 = no boss present */
    u8  type;
    u8  phase;           /* 0/1/2 */
    u16 hp;
    u16 hp_max;
    i16 x, y;            /* center, play-area coords */
    i16 vx_q4;           /* slide-LR velocity (q4) */
    u32 t_phase_ms;
    u16 fire_cd_ms;
    u8  body_frame;      /* 0..2 cycling */
    u32 anim_t_ms;
    u8  dying;           /* 1 = death sequence in progress */
    u32 dying_t_ms;
} Boss;

extern Boss g_boss;

int  boss_init(void);
void boss_close(void);

/* Spawn the Stage 1 boss (Tank2) above the play area, sliding down. */
void boss_spawn_tank2(void);

void boss_update(u32 dt_ms);
void boss_render(void);

/* Apply damage to the boss. Returns 1 if this hit killed the boss. */
int  boss_take_damage(u16 dmg);

/* Returns 1 if a boss is currently active (alive or in death sequence). */
int  boss_is_active(void);

#endif
```

- [ ] **Step 2: BOSS.CPP 작성**

```c
#include "BOSS.H"
#include "GFX.H"
#include "SPRITE.H"
#include "BULLET.H"
#include "PLAYER.H"
#include "EFFECT.H"
#include "SFX.H"

#define PLAY_X0   32
#define PLAY_W   256
#define PLAY_H   200

/* Movement params */
#define BOSS_SLIDE_SPEED_Q4  16   /* 1 px/frame LR while sliding */
#define BOSS_ENTRY_VY_Q4     16   /* 1 px/frame downward during entry */
#define BOSS_ENTRY_TARGET_Y  60   /* center y where boss settles */

/* Phase HP thresholds */
#define BOSS_PHASE1_HP  (BOSS_TANK2_HP_MAX * 2 / 3)   /* 133 */
#define BOSS_PHASE2_HP  (BOSS_TANK2_HP_MAX / 3)       /* 66 */

/* Fire intervals per phase (ms) */
#define BOSS_FIRE_P0_MS   1500
#define BOSS_FIRE_P1_MS   1500
#define BOSS_FIRE_P2_MS    900

/* Bullet speed for aimed shots */
#define BOSS_BULLET_SPEED_Q4  32   /* 2 px/frame */

/* Death sequence duration (multiple explosions) */
#define BOSS_DEATH_MS  1500

Boss g_boss;
static Sprite g_spr_body;
static Sprite g_spr_head;

int boss_init(void)
{
    g_boss.active = 0;
    if (spr_load("SPR_BTNK_B.SPR", &g_spr_body) != SPR_OK) return -1;
    if (spr_load("SPR_BTNK_H.SPR", &g_spr_head) != SPR_OK) {
        spr_free(&g_spr_body);
        return -2;
    }
    return 0;
}

void boss_close(void)
{
    spr_free(&g_spr_head);
    spr_free(&g_spr_body);
}

void boss_spawn_tank2(void)
{
    g_boss.active = 1;
    g_boss.type = (u8)BOSS_TANK2;
    g_boss.phase = 0;
    g_boss.hp = BOSS_TANK2_HP_MAX;
    g_boss.hp_max = BOSS_TANK2_HP_MAX;
    g_boss.x = (i16)(PLAY_W / 2);                    /* horizontal center */
    g_boss.y = (i16)(-BOSS_TANK2_HALF_H);             /* off-screen above */
    g_boss.vx_q4 = BOSS_SLIDE_SPEED_Q4;               /* start drifting right */
    g_boss.t_phase_ms = 0;
    g_boss.fire_cd_ms = BOSS_FIRE_P0_MS;
    g_boss.body_frame = 0;
    g_boss.anim_t_ms = 0;
    g_boss.dying = 0;
    g_boss.dying_t_ms = 0;
}

int boss_is_active(void)
{
    return g_boss.active;
}

static i16 abs_i16(i16 v) { return v < 0 ? (i16)(-v) : v; }

/* Fire one bullet aimed at the player, with optional perpendicular offset
 * for a fan (perp: -2..+2 multiplier). */
static void boss_fire_aimed(i16 perp_step)
{
    i16 dx = (i16)(g_player.x - g_boss.x);
    i16 dy = (i16)(g_player.y - g_boss.y);
    i16 ax = abs_i16(dx);
    i16 ay = abs_i16(dy);
    i16 m = ax > ay ? ax : ay;
    if (m == 0) return;

    /* unit-ish direction in q4 */
    i16 ux = (i16)(((i32)dx * BOSS_BULLET_SPEED_Q4) / m);
    i16 uy = (i16)(((i32)dy * BOSS_BULLET_SPEED_Q4) / m);

    /* perpendicular: rotate (ux, uy) by 90 deg → (-uy, ux), scaled by perp_step */
    i16 pvx = (i16)(-uy * perp_step / 4);
    i16 pvy = (i16)( ux * perp_step / 4);

    bullet_spawn_enemy(BUL_ENEMY_AIMED,
                       (i16)(g_boss.x - 2),
                       (i16)(g_boss.y - 2),
                       (i16)(ux + pvx),
                       (i16)(uy + pvy));
}

static void boss_fire_pattern(void)
{
    switch (g_boss.phase) {
    case 0:
        /* Phase 0: single aimed shot */
        boss_fire_aimed(0);
        break;
    case 1:
        /* Phase 1: 3-bullet fan */
        boss_fire_aimed(-2);
        boss_fire_aimed( 0);
        boss_fire_aimed( 2);
        break;
    case 2:
        /* Phase 2: 5-bullet fan */
        boss_fire_aimed(-3);
        boss_fire_aimed(-1);
        boss_fire_aimed( 1);
        boss_fire_aimed( 3);
        boss_fire_aimed( 0);
        break;
    }
}

static void boss_advance_phase_if_needed(void)
{
    if (g_boss.phase == 0 && g_boss.hp <= BOSS_PHASE1_HP) {
        g_boss.phase = 1;
        g_boss.t_phase_ms = 0;
    } else if (g_boss.phase == 1 && g_boss.hp <= BOSS_PHASE2_HP) {
        g_boss.phase = 2;
        g_boss.t_phase_ms = 0;
    }
}

void boss_update(u32 dt_ms)
{
    if (!g_boss.active) return;

    /* Death sequence: keep showing boss + spawning explosions, then deactivate. */
    if (g_boss.dying) {
        g_boss.dying_t_ms += dt_ms;
        /* spawn random explosion every ~150ms */
        static u32 exp_acc = 0;
        exp_acc += dt_ms;
        if (exp_acc >= 150) {
            exp_acc -= 150;
            i16 rx = (i16)(g_boss.x - BOSS_TANK2_HALF_W + (i16)(g_boss.dying_t_ms % 64));
            i16 ry = (i16)(g_boss.y - BOSS_TANK2_HALF_H + (i16)((g_boss.dying_t_ms / 7) % 64));
            effect_spawn_explosion(rx, ry);
            sfx_play(SFX_HIT);
        }
        if (g_boss.dying_t_ms >= BOSS_DEATH_MS) {
            g_boss.active = 0;
        }
        return;
    }

    g_boss.t_phase_ms += dt_ms;

    /* Entry: descend until at target y */
    if (g_boss.y < BOSS_ENTRY_TARGET_Y) {
        g_boss.y += BOSS_ENTRY_VY_Q4 / 16;   /* coarse: 1 px/frame */
        if (g_boss.y > BOSS_ENTRY_TARGET_Y) g_boss.y = BOSS_ENTRY_TARGET_Y;
    } else {
        /* Slide LR, bounce off play-area edges */
        g_boss.x += g_boss.vx_q4 / 16;       /* coarse: 1 px/frame */
        if (g_boss.x - BOSS_TANK2_HALF_W < 0) {
            g_boss.x = BOSS_TANK2_HALF_W;
            g_boss.vx_q4 = BOSS_SLIDE_SPEED_Q4;
        }
        if (g_boss.x + BOSS_TANK2_HALF_W > PLAY_W) {
            g_boss.x = (i16)(PLAY_W - BOSS_TANK2_HALF_W);
            g_boss.vx_q4 = (i16)(-BOSS_SLIDE_SPEED_Q4);
        }

        /* Fire pattern by phase */
        if (g_boss.fire_cd_ms > dt_ms) {
            g_boss.fire_cd_ms -= (u16)dt_ms;
        } else {
            g_boss.fire_cd_ms = 0;
        }
        if (g_boss.fire_cd_ms == 0) {
            boss_fire_pattern();
            switch (g_boss.phase) {
            case 0: g_boss.fire_cd_ms = BOSS_FIRE_P0_MS; break;
            case 1: g_boss.fire_cd_ms = BOSS_FIRE_P1_MS; break;
            case 2: g_boss.fire_cd_ms = BOSS_FIRE_P2_MS; break;
            }
        }
    }

    /* Track-rolling body animation */
    g_boss.anim_t_ms += dt_ms;
    if (g_boss.anim_t_ms >= BOSS_BODY_ANIM_PERIOD_MS) {
        g_boss.anim_t_ms -= BOSS_BODY_ANIM_PERIOD_MS;
        g_boss.body_frame = (u8)((g_boss.body_frame + 1) % BOSS_TANK2_BODY_FRAMES);
    }

    boss_advance_phase_if_needed();
}

void boss_render(void)
{
    if (!g_boss.active) return;

    int sx = PLAY_X0 + g_boss.x - BOSS_TANK2_HALF_W;
    int sy = g_boss.y - BOSS_TANK2_HALF_H;

    /* Compiled sprites need to fit on screen entirely. The boss can be
     * partially off-screen during entry (y starts negative), so clip
     * by skipping when sy < 0. Once entered, sy is guaranteed >= 0. */
    if (sx < 0 || sy < 0 || sx + BOSS_TANK2_W > 320 || sy + BOSS_TANK2_H > 200) return;

    gfx_draw_csprite(g_spr_body.frames[g_boss.body_frame], sx, sy);
    gfx_draw_csprite(g_spr_head.frames[0], sx, sy);
}

int boss_take_damage(u16 dmg)
{
    if (!g_boss.active || g_boss.dying) return 0;
    if (g_boss.hp > dmg) {
        g_boss.hp -= dmg;
        return 0;
    }
    g_boss.hp = 0;
    g_boss.dying = 1;
    g_boss.dying_t_ms = 0;
    sfx_play(SFX_BOMB);   /* big death thud */
    return 1;
}
```

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
```

Expected: 0 errors. BOSS.CPP가 새로 컴파일됨.

- [ ] **Step 4: 커밋**

```bash
git add SRC/BOSS.H SRC/BOSS.CPP
git commit -m "Add BOSS module — Tank2 stage-1 boss (3 phases)

Single instance. 70x70 body (3-frame track animation) + 70x70 head
overlay. HP 200, phases at 100%/66%/33% HP. Pattern per phase:
  P0: 1 aimed shot every 1500ms
  P1: 3-bullet fan every 1500ms
  P2: 5-bullet fan every 900ms

Fan computed by perpendicular offset on the aim direction (avoids
sin/cos LUT for prototype). Boss enters from above, slides LR
within play area, bounces off edges. Death sequence spawns random
explosions for 1500ms then deactivates.

Wired into GAME.CPP / COLLIDE / HUD in subsequent tasks."
```

---

## Task 3: COLLIDE — bullet vs boss + boss vs player

**Files:**
- Modify: `SRC/COLLIDE.H`, `SRC/COLLIDE.CPP`

플레이어 탄이 보스 AABB에 닿으면 보스 데미지 1 + 탄 비활성. 보스 본체가 플레이어 4x4 히트박스에 닿으면 player_take_damage().

- [ ] **Step 1: COLLIDE.H — 새 함수 선언**

```c
/* Player bullets vs boss: each hit deactivates the bullet and applies
 * 1 damage to the boss (per-weapon damage scaling deferred). */
void collide_player_bullets_vs_boss(void);

/* Boss body vs player: if boss is alive and overlaps the player's 4x4
 * hitbox, calls player_take_damage(). */
void collide_boss_vs_player(void);
```

- [ ] **Step 2: COLLIDE.CPP — 새 include + 두 함수**

```c
#include "BOSS.H"
```

파일 끝에 추가:

```c
void collide_player_bullets_vs_boss(void)
{
    int bi;

    if (!g_boss.active || g_boss.dying) return;

    int e_l = g_boss.x - BOSS_TANK2_HALF_W;
    int e_t = g_boss.y - BOSS_TANK2_HALF_H;
    int e_r = g_boss.x + BOSS_TANK2_HALF_W;
    int e_b = g_boss.y + BOSS_TANK2_HALF_H;

    for (bi = 0; bi < MAX_PLAYER_BULLETS; bi++) {
        Bullet *b = &g_pbullets[bi];
        if (!b->active) continue;

        int b_l = b->x;
        int b_t = b->y;
        int b_r = b->x + b->size_w;
        int b_b = b->y + b->size_h;

        if (b_r <= e_l || b_l >= e_r || b_b <= e_t || b_t >= e_b) continue;

        /* hit: 1 damage per bullet (Plan 6 weapons all damage 1) */
        b->active = 0;
        boss_take_damage(1);
        sfx_play(SFX_HIT);
    }
}

void collide_boss_vs_player(void)
{
    if (!g_boss.active || g_boss.dying) return;
    if (!g_player.alive) return;
    if (g_player.inv_ms > 0) return;

    int p_l = g_player.x - 2;
    int p_t = g_player.y - 2;
    int p_r = g_player.x + 2;
    int p_b = g_player.y + 2;

    int e_l = g_boss.x - BOSS_TANK2_HALF_W;
    int e_t = g_boss.y - BOSS_TANK2_HALF_H;
    int e_r = g_boss.x + BOSS_TANK2_HALF_W;
    int e_b = g_boss.y + BOSS_TANK2_HALF_H;

    if (p_r <= e_l || p_l >= e_r || p_b <= e_t || p_t >= e_b) return;
    player_take_damage();
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
git commit -m "Add boss collision — player bullets vs boss + boss touch

collide_player_bullets_vs_boss: AABB sweep, every hit deactivates
the bullet and deals 1 damage. SFX_HIT plays on each connect.

collide_boss_vs_player: 4x4 player hitbox vs boss 70x70 AABB. If
the boss body overlaps the player (and player isn't invincible),
calls player_take_damage() — same treatment as enemy bullet hits
(autobomb if bombs > 0, else die)."
```

---

## Task 4: PLAYER — 봄으로 보스 데미지

**Files:**
- Modify: `SRC/PLAYER.CPP`

`blast_all_enemies()`에서 보스도 50 데미지.

- [ ] **Step 1: PLAYER.CPP — include + blast 확장**

새 include:

```c
#include "BOSS.H"
```

`blast_all_enemies()` 끝에 추가:

```c
static void blast_all_enemies(void)
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_enemies[i];
        if (!e->active) continue;
        if (e->plane != PLANE_AIR) continue;
        e->active = 0;
        effect_spawn_explosion(e->x, e->y);
    }
    /* Bomb also damages the boss (spec: 200 dmg in full game; using 50
     * here because Plan 7 boss HP is 200 not 2000 — keeps the bomb a
     * meaningful but not instant-kill weapon). */
    if (g_boss.active && !g_boss.dying) {
        boss_take_damage(50);
    }
    sfx_play(SFX_BOMB);
}
```

- [ ] **Step 2: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -5
```

- [ ] **Step 3: 커밋**

```bash
git add SRC/PLAYER.CPP
git commit -m "Bombs damage the active boss (50 dmg vs HP 200 = 25%)

blast_all_enemies now also calls boss_take_damage(50) when a boss
is active. Spec says 200 dmg in the full game (boss HP 2000); we
scale to 50 because Plan 7 prototype HP is 200. With 2 starting
bombs and pickups, a player who hoards bombs can melt 50%+ of the
boss this way — intentional safety net."
```

---

## Task 5: GAME.CPP — 보스 스폰, popcorn 중지, STAGE_CLEAR freeze

**Files:**
- Modify: `SRC/GAME.CPP`

30초 후 보스 등장. 보스전 동안 popcorn 스폰 중지. 보스 격파 후 g_state.gs = GS_STAGE_CLEAR로 freeze.

- [ ] **Step 1: 새 include + init/close 통합**

```c
#include "BOSS.H"
```

`item_init()` 다음에 boss_init 추가 (실패 시 LIFO 정리하고 return 6):

```c
    if (item_init() != 0) {
        ...
        return 5;
    }

    if (boss_init() != 0) {
        item_close();
        effect_close();
        enemy_close();
        player_close();
        bg_close();
        gfx_close();
        sfx_close();
        snd_close();
        input_close();
        timer_close();
        return 6;
    }
```

shutdown LIFO에 추가 (item_close 직전):

```c
    snd_stop();
    boss_close();
    item_close();
    effect_close();
    ...
```

- [ ] **Step 2: 보스 스폰 트리거 + popcorn 중지**

`state_reset()`에 추가:

```c
    g_item_spawn_t_ms = 0;
    /* boss state is reset by boss_spawn_tank2 when the trigger fires;
     * we just zero the active flag here. */
    g_boss.active = 0;
}
```

메인 루프 — popcorn 스폰 부분을 g_boss.active 체크로 감싸고, 보스 스폰 트리거 추가:

```c
        /* Boss trigger: at 30s of stage time, if no boss yet, spawn one. */
        g_state.stage_t_ms += dt;
        if (g_state.stage_t_ms >= 30000 && !g_boss.active &&
            g_state.gs != GS_STAGE_CLEAR) {
            boss_spawn_tank2();
            g_state.gs = GS_BOSS_FIGHT;
        }

        /* Popcorn spawn — disabled while a boss is on screen and during
         * stage clear freeze. */
        if (!g_boss.active && g_state.gs != GS_STAGE_CLEAR) {
            g_spawn_t_ms += dt;
            if (g_spawn_t_ms >= 1000) {
                g_spawn_t_ms -= 1000;
                /* (existing popcorn spawn code) */
                i16 sx = (i16)(POPCORN_HALF_W + (i16)(next_rand() % (256 - POPCORN_W)));
                EnemyPattern pat = (next_rand() & 1) ? PAT_AIM_PLAYER : PAT_STRAIGHT_DOWN;
                enemy_spawn(E_POPCORN_A, PLANE_AIR, pat,
                            sx, (i16)(-POPCORN_HALF_H),
                            0, 24);
            }
        }

        /* P-item spawn — also pause during boss + stage clear */
        if (!g_boss.active && g_state.gs != GS_STAGE_CLEAR) {
            g_item_spawn_t_ms += dt;
            if (g_item_spawn_t_ms >= 5000) {
                g_item_spawn_t_ms -= 5000;
                /* (existing P spawn code) */
                i16 ix = (i16)(PWUP_HALF_W + (i16)(next_rand() % (256 - PWUP_W)));
                PowerWeapon col = (PowerWeapon)(next_rand() % 3);
                item_spawn_power(ix, (i16)(-PWUP_HALF_H), col);
            }
        }
```

- [ ] **Step 3: 보스 update + collide + render 통합**

기존 update 블록에 boss_update + collide:

```c
        if (g_player.alive) {
            enemy_update_all(dt);
            effect_update_all(dt);
            item_update_all(dt);
            boss_update(dt);                         /* NEW */
        }

        if (g_player.alive) {
            collide_player_bullets_vs_enemies();
            collide_enemy_bullets_vs_player();
            collide_player_bullets_vs_items();
            collide_player_vs_items();
            collide_player_bullets_vs_boss();        /* NEW */
            collide_boss_vs_player();                /* NEW */
        }

        /* If a boss died this frame, mark stage clear. */
        if (g_state.gs == GS_BOSS_FIGHT && !g_boss.active) {
            g_state.gs = GS_STAGE_CLEAR;
            g_state.gs_t_ms = 0;
        }
```

렌더에 boss_render 추가 (effect_render 직전, 폭발 위로 보스가 보이지 않게):

```c
        bullet_render_player();
        enemy_render_air();
        boss_render();                          /* NEW: boss above enemies */
        player_render();
        bullet_render_enemy();
        item_render();
        effect_render();
        hud_render();
```

- [ ] **Step 4: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
ls -l SRC/GAME.EXE
```

Expected: 0 errors.

- [ ] **Step 5: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "Wire BOSS — spawn at 30s + freeze gameplay on STAGE_CLEAR

g_state.stage_t_ms accumulates during play. At 30000ms, if no boss
is active and we're not already in STAGE_CLEAR, boss_spawn_tank2()
fires and gs transitions to GS_BOSS_FIGHT. Popcorn and P-item
spawning are gated on (!g_boss.active && gs != GS_STAGE_CLEAR) so
the boss fight is uncluttered.

When boss.active drops to 0 during BOSS_FIGHT (i.e., death sequence
finished), gs flips to GS_STAGE_CLEAR. The HUD shows it; the rest
of the loop continues to render but no new spawns happen.

Render order: boss draws above air enemies, below the player and
below ENV (item/bullet/effect/hud)."
```

---

## Task 6: HUD — 보스 HP 바 + STAGE CLEAR 텍스트

**Files:**
- Modify: `SRC/HUD.CPP`

보스 HP 바: 화면 상단 x=32~287에 1px 외곽 + 채워진 부분. 보스 HP 비율에 비례.

STAGE_CLEAR 시 화면 가운데 큰 색 사각형 표시 (실제 텍스트는 폰트 없어서 단순 직사각형).

- [ ] **Step 1: HUD.CPP 확장**

기존 `hud_render()` 끝에 추가:

```c
    /* Boss HP bar (top of play area, only when boss is active or dying). */
    if (g_boss.active) {
        int bar_x = 32;             /* play area left edge in screen coords */
        int bar_w = 256;            /* play area width */
        int bar_y = 0;
        int bar_h = 4;
        /* outline */
        gfx_rect(bar_x, bar_y, bar_w, bar_h, 15);
        /* fill proportional to HP */
        int fill_w = (int)((u32)bar_w * g_boss.hp / g_boss.hp_max);
        if (fill_w > 0) gfx_fill_rect(bar_x + 1, bar_y + 1, fill_w - 2, bar_h - 2, 25);
    }

    /* STAGE CLEAR overlay: green rectangle at center of play area. */
    if (g_state.gs == GS_STAGE_CLEAR) {
        gfx_fill_rect(64, 88, 192, 24, 32);   /* placeholder green block */
        gfx_rect(64, 88, 192, 24, 15);
        /* Could draw "STAGE 1 CLEAR" text once font is wired (Plan 8+). */
    }
```

새 include:

```c
#include "BOSS.H"
#include "STATE.H"   /* (already via HUD.H) */
```

- [ ] **Step 2: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -5
```

- [ ] **Step 3: 커밋**

```bash
git add SRC/HUD.CPP
git commit -m "HUD — boss HP bar + STAGE CLEAR overlay

Boss HP bar at top of play area (x=32..287, y=0..3): white outline,
red fill scaled to current HP / hp_max. Visible only while boss is
active (including death sequence).

STAGE CLEAR placeholder: green filled rectangle 192x24 centered.
A proper text banner is deferred to Plan 8 when the FONT module
gets wired up (or hand-rolled letters added)."
```

---

## Task 7: 시각 검증

```bash
./run.sh
```

확인 사항:
- 30초 동안 평소 게임플레이 (popcorn + P 아이템)
- 30초 시점에 popcorn/P 스폰 멈추고 화면 위에서 **Tank2 보스 등장** (위에서 천천히 슬라이드 다운)
- 보스가 자리잡으면 좌우로 천천히 슬라이드, 가장자리에서 반대 방향
- 화면 상단에 **빨간 HP 바** 표시 (white 외곽 + red 채움)
- 보스가 페이즈별 발사:
  - 100~66% HP: 1.5초마다 1탄
  - 66~33% HP: 1.5초마다 3탄팬
  - 33~0% HP: 0.9초마다 5탄팬
- 보스에 직접 닿으면 데미지 (오토봄/잔기-1)
- 보스가 격파되면 **다중 폭발 1.5초간** 재생
- 폭발 끝나면 화면 가운데 **녹색 사각형 (STAGE CLEAR placeholder)** 표시
- 그 후 더 이상 적 안 나옴 (게임 freeze 상태) — ESC로 종료

만약:
- 보스가 화면 위에 안 나타남 → boss_render의 sy < 0 체크가 너무 strict. 진입하는 동안 안 보이는 건 의도지만 화면 안에 들어왔는데도 안 보이면 좌표 계산 확인.
- HP 바가 너무 짧음 → fill_w 계산식 버그.
- 30초보다 빨리/늦게 등장 → g_state.stage_t_ms accumulate 위치 확인.

---

## Self-Review Checklist

- [ ] **Spec coverage** (M10 부분):
  - 단일 보스 (Tank2): ✓
  - 3 페이즈 (HP 100%/66%/33%): ✓
  - 페이즈별 발사 패턴: ✓ (1/3/5 fan)
  - 보스 사망 시 다중 폭발: ✓
  - STAGE_CLEAR 트리거: ✓ (placeholder)
  - 보스 HP 바: ✓
  - 정식 STAGE CLEAR FSM (보너스 점수 + 다음 스테이지): ✗ (Plan 8 FSM)
  - 헤드 회전 시각: ✗ (단일 프레임)
  - 보스 인트로 시퀀스 ("WARNING!"): ✗ (Plan 8 FSM)

---

## Open Items (Plan 8 인계)

- 정식 FSM (TITLE / STAGE_INTRO / BOSS_INTRO 3초 카운트다운 / STAGE_CLEAR 보너스 점수 / GAME_OVER / CONTINUE_PROMPT)
- 보스 헤드 회전 (사용자 추적): 사전 회전 프레임 + 가까운 각도 선택
- 보스 머리/본체 부분 파괴 (각 파츠별 HP — 실제 R2 정통)
- 보스 등장 시 "WARNING!" 텍스트 + 빨간 화면 깜빡임 (FONT 필요)
- 무기별 데미지 차등 (Vulcan=1, Laser=2, Plasma=3) — 보스 HP를 spec 200~2000으로 맞춤
- STAGE 시스템 (시간축 스크립트 — 보스 스폰을 명시적으로)
