# Raiden 2 Clone — Plan 3: 적 탄막 + 사망/부활 + 봄/오토봄

> **For agentic workers:** Use superpowers:subagent-driven-development to execute. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 적이 플레이어 조준탄을 발사. 플레이어가 적탄에 맞으면 봄을 자동 소비(오토봄)하거나 잔기 -1. 수동 봄(X 키)으로 화면 청소 + 무적. 잔기 0이면 게임 정지.

**Architecture:** Plan 2의 ENEMY/COLLIDE/BULLET 위에 적 탄 풀 추가, PAT_AIM_PLAYER AI 패턴 추가, PLAYER에 데미지/사망/무적/봄 로직 추가, COLLIDE에 적탄 vs 플레이어 검사 추가. 새 모듈 0개 — 기존 모듈만 확장.

**Tech Stack:** Watcom C++, DOSBox.

**Spec 참조:** `docs/superpowers/specs/2026-04-28-raiden2-clone-design.md` §4.5 (봄), §4.6 (사망/부활), §6 (탄), §7 (충돌)

---

## File Map

### MODIFIED FILES (모두 기존 모듈 확장 — 신규 모듈 0)

| 경로 | 변경 |
|---|---|
| `SRC/BULLET.H` / `SRC/BULLET.CPP` | 적탄 풀 (256 슬롯) + spawn/update/render/clear 함수 추가 |
| `SRC/ENEMY.H` | `fire_cooldown` u8 → u16 (255ms 한계 해소), 새 패턴 enum 추가 |
| `SRC/ENEMY_AI.H` / `SRC/ENEMY_AI.CPP` | `enemy_ai_aim_player()` 함수 추가 |
| `SRC/PLAYER.H` / `SRC/PLAYER.CPP` | 무적 타이머 + 깜빡임 렌더 + `player_die()` + `player_fire_bomb()` |
| `SRC/COLLIDE.H` / `SRC/COLLIDE.CPP` | `collide_enemy_bullets_vs_player()` 추가 (오토봄 트리거 포함) |
| `SRC/GAME.CPP` | 스폰 스케줄러를 PAT_AIM_PLAYER 변종으로, 적탄 update/render, X 키 봄 입력, 게임오버 freeze |

### NOT TOUCHED

- HUD/폰트 (Plan 4)
- 무기 다양화 (Laser/Plasma) (Plan 5)
- 메달/아이템 (Plan 6)
- STAGE 시스템 (Plan 7)
- 보스 (Plan 8)

---

## Task 1: BULLET 모듈 확장 — 적 탄 풀

**Files:**
- Modify: `SRC/BULLET.H`, `SRC/BULLET.CPP`

256-슬롯 적 탄 풀 추가. 플레이어 풀과는 별도 (사이즈/처리 다름). 한 가지 적탄 종류 (`BUL_ENEMY_AIMED`) 만 Plan 3에서 사용. 4x4 사각형 빨강 그림.

- [ ] **Step 1: BULLET.H 확장**

`SRC/BULLET.H`에서 `BulletKind` enum에 적탄 종류 추가:

```c
typedef enum {
    BUL_PLAYER_VULCAN = 0,
    BUL_ENEMY_AIMED   = 1
    /* future: BUL_PLAYER_LASER, BUL_PLAYER_PLASMA, BUL_HOMING,
     *         BUL_ENEMY_DOT, BUL_ENEMY_RICE, BUL_ENEMY_BIG, BUL_LASER_SHORT */
} BulletKind;
```

상수 추가 (MAX_PLAYER_BULLETS 옆):

```c
#define MAX_ENEMY_BULLETS 256
```

extern 선언 추가 (g_pbullets 옆):

```c
extern Bullet g_ebullets[MAX_ENEMY_BULLETS];
```

함수 선언 추가 (bullet_render_player 다음):

```c
/* Spawn an enemy bullet. Returns 1 on success, 0 if pool full. */
int  bullet_spawn_enemy(BulletKind kind, i16 x, i16 y, i16 vx_q4, i16 vy_q4);

/* Advance all active enemy bullets by 1 frame. */
void bullet_update_all_enemy(u32 dt_ms);

/* Render all active enemy bullets. */
void bullet_render_enemy(void);

/* Deactivate all enemy bullets. Used by the bomb. */
void bullet_clear_all_enemy(void);
```

- [ ] **Step 2: BULLET.CPP 확장**

기존 `bullet_init()`에 enemy 풀도 초기화 추가:

```c
void bullet_init(void)
{
    int i;
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) g_pbullets[i].active = 0;
    for (i = 0; i < MAX_ENEMY_BULLETS;  i++) g_ebullets[i].active = 0;
}
```

새 정의 추가 (파일 끝에):

```c
Bullet g_ebullets[MAX_ENEMY_BULLETS];

int bullet_spawn_enemy(BulletKind kind, i16 x, i16 y, i16 vx_q4, i16 vy_q4)
{
    int i;
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        Bullet *b = &g_ebullets[i];
        if (b->active) continue;
        b->active = 1;
        b->kind = (u8)kind;
        b->color_idx = 27;     /* light red — enemy bullets stand out from yellow player */
        b->size_w = 4;
        b->size_h = 4;
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

void bullet_update_all_enemy(u32 dt_ms)
{
    int i;
    (void)dt_ms;
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        Bullet *b = &g_ebullets[i];
        if (!b->active) continue;

        b->sx += b->vx;
        b->sy += b->vy;
        b->x  += b->sx >> 4;  b->sx &= 0x0F;
        b->y  += b->sy >> 4;  b->sy &= 0x0F;

        /* slack: 8px past play area on each side */
        if (b->y + b->size_h < -8 || b->y > 200 + 8 ||
            b->x + b->size_w < -8 || b->x > 256 + 8) {
            b->active = 0;
        }
    }
}

void bullet_render_enemy(void)
{
    int i;
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        const Bullet *b = &g_ebullets[i];
        if (!b->active) continue;
        gfx_fill_rect(32 + b->x, b->y, b->size_w, b->size_h, b->color_idx);
    }
}

void bullet_clear_all_enemy(void)
{
    int i;
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) g_ebullets[i].active = 0;
}
```

> **참고**: `b->color_idx = 27`은 빨강 램프 밝은 쪽 추정. 실제 구동 후 색이 어색하면 조정 가능.

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
```

Expected: 0 errors. BULLET.CPP 라인 수 늘어남.

- [ ] **Step 4: 커밋**

```bash
git add SRC/BULLET.H SRC/BULLET.CPP
git commit -m "Extend BULLET — add enemy bullet pool (256 slots)

New BUL_ENEMY_AIMED kind, 4x4 red squares. Pool is separate from
the player pool because (a) different sizes (256 vs 64), (b) future
plans add per-pool processing for collision and bomb-clear. New
functions: bullet_spawn_enemy, bullet_update_all_enemy,
bullet_render_enemy, bullet_clear_all_enemy. The clear function is
used by the player bomb to wipe the screen."
```

---

## Task 2: ENEMY 구조체 — fire_cooldown u16

**Files:**
- Modify: `SRC/ENEMY.H`

PAT_AIM_PLAYER가 1500ms 같은 발사 간격을 쓰려면 u8 (255ms 한계) 부족. u16으로 확대.

- [ ] **Step 1: ENEMY.H 수정**

`u8 fire_cooldown;` → `u16 fire_cooldown;`

EnemyPattern enum 확장:

```c
typedef enum {
    PAT_STRAIGHT_DOWN = 0,
    PAT_AIM_PLAYER    = 1
    /* future: PAT_SINE, PAT_DIVE, PAT_FORMATION_V, ... */
} EnemyPattern;
```

> **참고**: ENEMY.CPP의 `enemy_spawn()`에서 `e->fire_cooldown = 0;` 초기화는 이미 있음 — u16으로도 동일하게 동작.

- [ ] **Step 2: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -8
```

Expected: 0 errors. Enemy 구조체 사이즈 1바이트 증가 (메모리 영향 미미).

- [ ] **Step 3: 커밋**

```bash
git add SRC/ENEMY.H
git commit -m "Extend Enemy.fire_cooldown to u16 + add PAT_AIM_PLAYER

u8 fire_cooldown maxes at 255ms, but Plan 3's aim-player pattern
fires every 1500ms. Bumped to u16 (max 65s, plenty). Adds 1 byte
per enemy slot (32 enemies x 1 = 32 bytes total — negligible).

PAT_AIM_PLAYER added to the EnemyPattern enum; the matching AI
function is supplied in Task 3."
```

---

## Task 3: ENEMY_AI — PAT_AIM_PLAYER 패턴

**Files:**
- Modify: `SRC/ENEMY_AI.H`, `SRC/ENEMY_AI.CPP`
- Modify: `SRC/ENEMY.CPP` (AI 디스패치에 새 패턴 추가)

적이 1500ms마다 플레이어 방향으로 1발 조준탄 발사. Chebyshev 거리 기반 단순 정규화 (sqrt 없음, prototype에 충분).

- [ ] **Step 1: ENEMY_AI.H에 함수 선언 추가**

기존 `enemy_ai_straight_down` 다음:

```c
/* Per-frame AI hook: PAT_AIM_PLAYER.
 *
 * Holds spawn-time vy. Periodically (every 1500ms) fires a single
 * aimed bullet toward the current player center. Direction is computed
 * with Chebyshev normalization (max(|dx|,|dy|)) — close enough to
 * Euclidean for prototype scope; bullets at extreme angles fly slightly
 * faster, which is acceptable for aim-shot enemies. */
void enemy_ai_aim_player(struct Enemy *e, u32 dt_ms);
```

- [ ] **Step 2: ENEMY_AI.CPP에 구현 추가**

새 include 추가 (BULLET, PLAYER 필요):

```c
#include "ENEMY_AI.H"
#include "ENEMY.H"
#include "BULLET.H"
#include "PLAYER.H"
```

기존 `enemy_ai_straight_down` 다음에 추가:

```c
#define AIM_FIRE_INTERVAL_MS  1500
#define AIM_BULLET_SPEED_Q4   32      /* 2 px/frame total magnitude */

static i16 abs_i16(i16 v) { return v < 0 ? (i16)(-v) : v; }

void enemy_ai_aim_player(struct Enemy *e, u32 dt_ms)
{
    /* tick the cooldown */
    if (e->fire_cooldown > dt_ms) {
        e->fire_cooldown -= (u16)dt_ms;
        return;
    }
    e->fire_cooldown = 0;

    /* aim from enemy center to player center */
    i16 dx = (i16)(g_player.x - e->x);
    i16 dy = (i16)(g_player.y - e->y);
    i16 ax = abs_i16(dx);
    i16 ay = abs_i16(dy);
    i16 m = ax > ay ? ax : ay;
    if (m == 0) return;   /* on top of player — skip this fire */

    /* normalize to AIM_BULLET_SPEED_Q4 magnitude using Chebyshev */
    i16 vx_q4 = (i16)(((i32)dx * AIM_BULLET_SPEED_Q4) / m);
    i16 vy_q4 = (i16)(((i32)dy * AIM_BULLET_SPEED_Q4) / m);

    /* fire one bullet from enemy center */
    bullet_spawn_enemy(BUL_ENEMY_AIMED, e->x - 2, e->y - 2, vx_q4, vy_q4);

    e->fire_cooldown = AIM_FIRE_INTERVAL_MS;
}
```

- [ ] **Step 3: ENEMY.CPP의 AI 디스패치 갱신**

`enemy_update_all()`에서 단일 호출:

```c
        enemy_ai_straight_down(e, dt_ms);
```

위 한 줄을 `switch`로 변경:

```c
        switch (e->ai_id) {
        case PAT_STRAIGHT_DOWN:
            enemy_ai_straight_down(e, dt_ms);
            break;
        case PAT_AIM_PLAYER:
            enemy_ai_aim_player(e, dt_ms);
            break;
        default:
            /* unknown pattern — fall through (no-op) */
            break;
        }
```

- [ ] **Step 4: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
```

Expected: 0 errors. ENEMY_AI.CPP에 새 함수가 컴파일됨.

- [ ] **Step 5: 커밋**

```bash
git add SRC/ENEMY_AI.H SRC/ENEMY_AI.CPP SRC/ENEMY.CPP
git commit -m "Add PAT_AIM_PLAYER AI — aimed shots toward player

New enemy_ai_aim_player function fires one BUL_ENEMY_AIMED toward
the player center every 1500ms. Direction is Chebyshev-normalized
(max(|dx|,|dy|)) at speed 2 px/frame — prototype-grade aim, will
upgrade to Euclidean (sqrt or LUT) in a later plan if needed.

ENEMY.CPP now dispatches by ai_id with a switch (formerly hardcoded
to straight_down)."
```

---

## Task 4: PLAYER — 무적 타이머 + 깜빡임 + 사망 + 봄

**Files:**
- Modify: `SRC/PLAYER.H`, `SRC/PLAYER.CPP`

플레이어 무적 시간/깜빡임/`player_die()`/`player_fire_bomb()` 추가. 사망 시 폭발 이펙트 + 잔기 감소.

- [ ] **Step 1: PLAYER.H 확장**

기존 상수 다음에 추가:

```c
/* Invincibility durations (ms). Plan 3 spec: respawn 2s, bomb 2.5s. */
#define PLAYER_INV_RESPAWN_MS  2000
#define PLAYER_INV_BOMB_MS     2500

/* Blink period during invincibility (ms): toggle visibility every
 * BLINK_MS. Player visible at !((inv_ms / BLINK_MS) & 1). */
#define PLAYER_BLINK_MS         80
```

`Player` struct에 새 필드:

```c
typedef struct {
    i16 x, y;
    i16 vx;
    u32 anim_t_ms;
    u8  anim_phase;
    u8  alive;            /* 1 = present, 0 = dead (game over freeze) */
    u32 inv_ms;           /* invincibility ms remaining; 0 = vulnerable */
} Player;
```

새 함수 선언:

```c
/* Trigger player damage. If invincible, no-op. If bombs > 0, autobomb
 * (clears bullets, kills enemies, sets bomb invincibility, decrements
 * bombs). Else: lives--, spawn explosion at player center, set respawn
 * invincibility. If lives == 0, sets alive = 0 (caller decides what
 * "game over" means — Plan 3 freezes update loop). */
void player_take_damage(void);

/* Manual bomb (X key). If bombs == 0 or already invincible, no-op.
 * Else: clears enemy bullets, kills all enemies (with explosions),
 * sets bomb invincibility, bombs--. */
void player_fire_bomb(void);
```

- [ ] **Step 2: PLAYER.CPP 확장**

새 include:

```c
#include "PLAYER.H"
#include "GFX.H"
#include "INPUT.H"
#include "SPRITE.H"
#include "BULLET.H"
#include "ENEMY.H"
#include "EFFECT.H"
```

`player_init()` 마지막에 inv_ms 초기화 추가:

```c
    g_player.inv_ms = 0;
    if (spr_load("SPR_PSHP.SPR", &g_pshp) != SPR_OK) return -1;
    return 0;
```

`player_update()` 시작에서 사망 시 freeze + 무적 타이머 감산:

```c
void player_update(u32 dt_ms)
{
    if (!g_player.alive) return;

    if (g_player.inv_ms > dt_ms) g_player.inv_ms -= dt_ms;
    else                          g_player.inv_ms = 0;

    int dx = 0, dy = 0;
    /* ... 기존 입력/이동 코드 ... */
}
```

`player_render()` 시작에서 깜빡임 처리:

```c
void player_render(void)
{
    if (!g_player.alive) return;

    /* invincibility blink: hide every other BLINK_MS slice */
    if (g_player.inv_ms > 0 && ((g_player.inv_ms / PLAYER_BLINK_MS) & 1)) return;

    int frame_idx;
    /* ... 기존 frame 선택 + 렌더 ... */
}
```

파일 끝에 새 함수 추가:

```c
/* Local helper: kill every active air enemy and spawn an explosion
 * at each. Used by autobomb and manual bomb. */
static void blast_all_enemies(void)
{
    int i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &g_enemies[i];
        if (!e->active) continue;
        if (e->plane != PLANE_AIR) continue;
        e->active = 0;
        effect_spawn_explosion(e->x, e->y);
        /* future: g_state.score += per-type bonus */
    }
}

void player_take_damage(void)
{
    if (g_player.inv_ms > 0) return;     /* already invincible */

    if (g_state.bombs > 0) {
        /* autobomb */
        g_state.bombs--;
        bullet_clear_all_enemy();
        blast_all_enemies();
        g_player.inv_ms = PLAYER_INV_BOMB_MS;
        return;
    }

    /* die */
    effect_spawn_explosion(g_player.x, g_player.y);
    if (g_state.lives > 0) g_state.lives--;
    if (g_state.lives == 0) {
        g_player.alive = 0;
        return;
    }
    g_player.inv_ms = PLAYER_INV_RESPAWN_MS;
}

void player_fire_bomb(void)
{
    if (g_state.bombs == 0) return;
    if (g_player.inv_ms > 0) return;     /* don't waste during invincibility */
    g_state.bombs--;
    bullet_clear_all_enemy();
    blast_all_enemies();
    g_player.inv_ms = PLAYER_INV_BOMB_MS;
}
```

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -12
```

Expected: 0 errors. PLAYER.CPP가 BULLET/ENEMY/EFFECT를 의존하지만 모두 선언됨.

- [ ] **Step 4: 커밋**

```bash
git add SRC/PLAYER.H SRC/PLAYER.CPP
git commit -m "Add player damage / death / bomb / autobomb

New Player.inv_ms (u32 ms remaining of invincibility) and .alive
(0 freezes update/render — game over).

player_take_damage() implements the spec section 4.5 hit handling:
ignore if already invincible; if bombs > 0 trigger autobomb (consume
1 bomb, clear enemy bullets, kill all air enemies, set 2.5s
invincibility); else lives-- and 2s invincibility. lives == 0 sets
alive = 0.

player_fire_bomb() is the manual bomb (X key in Plan 3 — wired in
Task 6). Same effect as autobomb minus the death/lives path.

Render uses a simple blink during invincibility (toggle every
80ms)."
```

---

## Task 5: COLLIDE — 적탄 vs 플레이어

**Files:**
- Modify: `SRC/COLLIDE.H`, `SRC/COLLIDE.CPP`

플레이어 중앙 4x4 히트박스 vs 적탄 AABB. 적중 시 `player_take_damage()` 호출 (autobomb 로직은 player 모듈이 처리).

- [ ] **Step 1: COLLIDE.H에 함수 선언 추가**

기존 `collide_player_bullets_vs_enemies` 다음:

```c
/* Per-frame: every active enemy bullet vs player 4x4 hitbox at
 * (g_player.x, g_player.y) center. On hit, deactivate the bullet and
 * call player_take_damage() (which handles invincibility / autobomb /
 * death). */
void collide_enemy_bullets_vs_player(void);
```

- [ ] **Step 2: COLLIDE.CPP에 구현 추가**

새 include:

```c
#include "COLLIDE.H"
#include "BULLET.H"
#include "ENEMY.H"
#include "EFFECT.H"
#include "PLAYER.H"
```

기존 `collide_player_bullets_vs_enemies` 함수 다음에 추가:

```c
#define PLAYER_HITBOX_HALF  2   /* 4x4 hitbox: 2px each side of center */

void collide_enemy_bullets_vs_player(void)
{
    int i;

    if (!g_player.alive) return;
    if (g_player.inv_ms > 0) return;      /* invincible: skip checks */

    int p_l = g_player.x - PLAYER_HITBOX_HALF;
    int p_t = g_player.y - PLAYER_HITBOX_HALF;
    int p_r = g_player.x + PLAYER_HITBOX_HALF;
    int p_b = g_player.y + PLAYER_HITBOX_HALF;

    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        Bullet *b = &g_ebullets[i];
        if (!b->active) continue;

        int b_l = b->x;
        int b_t = b->y;
        int b_r = b->x + b->size_w;
        int b_b = b->y + b->size_h;

        if (b_r <= p_l || b_l >= p_r || b_b <= p_t || b_t >= p_b) continue;

        /* hit */
        b->active = 0;
        player_take_damage();
        /* if autobomb fires, all bullets are cleared; the loop's b->active
         * is now 0 and we naturally skip the rest. Safe to continue. */
        return;   /* one damage per frame max */
    }
}
```

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
```

Expected: 0 errors.

- [ ] **Step 4: 커밋**

```bash
git add SRC/COLLIDE.H SRC/COLLIDE.CPP
git commit -m "Add enemy bullet vs player collision (4x4 hitbox)

Sweeps MAX_ENEMY_BULLETS=256 vs the player 4x4 center hitbox. On
hit: bullet deactivates, player_take_damage() is called (handles
invincibility/autobomb/death). At most one damage event per frame
(returns after the first hit).

Skips the entire sweep when player is dead or already invincible —
saves ~256 checks during the 2-second post-respawn blink."
```

---

## Task 6: GAME.CPP 통합 — 새 패턴 적 + 봄 키 + 적탄 처리 + 게임오버

**Files:**
- Modify: `SRC/GAME.CPP`

스폰 스케줄러를 PAT_AIM_PLAYER 변종을 섞도록 수정. X 키 봄. 적탄 update/render. 적탄 vs 플레이어 충돌. 게임오버 (lives=0) 처리.

- [ ] **Step 1: 스폰 스케줄러 — 절반은 직진, 절반은 조준**

기존 스폰 블록을 다음으로 교체 (RNG 비트로 패턴 선택, 1초마다 1대):

```c
        g_spawn_t_ms += dt;
        if (g_spawn_t_ms >= 1000) {
            g_spawn_t_ms -= 1000;
            i16 sx = (i16)(POPCORN_HALF_W + (i16)(next_rand() % (256 - POPCORN_W)));
            EnemyPattern pat = (next_rand() & 1) ? PAT_AIM_PLAYER : PAT_STRAIGHT_DOWN;
            enemy_spawn(E_POPCORN_A, PLANE_AIR, pat,
                        sx, (i16)(-POPCORN_HALF_H),
                        0, /*vy_q4*/ 24);   /* 1.5 px/frame downward */
        }
```

- [ ] **Step 2: 메인 루프 — 적탄 update/render + 충돌 + 봄 입력 + 게임오버 freeze**

기존 update/render 블록을 다음으로 확장 (변경 부분만 강조):

```c
    while (!input_key(KEY_ESC)) {
        u32 now = timer_ms();
        u32 dt = now - g_last_ms;
        g_last_ms = now;

        /* spawn scheduler (위 Step 1) */
        ...

        /* X key bomb — edge-trigger via simple held flag */
        static int g_x_was_held = 0;
        int x_held = input_key(KEY_X);
        if (x_held && !g_x_was_held) player_fire_bomb();
        g_x_was_held = x_held;

        bg_update(dt);
        if (g_player.alive) {
            player_update(dt);
            weapon_fire_tick(dt, input_key(KEY_Z));
        }
        bullet_update_all(dt);              /* player bullets */
        bullet_update_all_enemy(dt);        /* NEW: enemy bullets */
        if (g_player.alive) {
            enemy_update_all(dt);           /* freeze enemies on game over */
            effect_update_all(dt);
        }

        if (g_player.alive) {
            collide_player_bullets_vs_enemies();
            collide_enemy_bullets_vs_player();   /* NEW */
        }

        gfx_clear(0);
        bg_render();
        enemy_render_ground();
        bullet_render_player();
        enemy_render_air();
        player_render();
        bullet_render_enemy();              /* NEW: enemy bullets above player */
        effect_render();

        gfx_vsync();
        gfx_flip();
    }
```

> **렌더 순서 (spec section 6.2 갱신)**:
> 1. 배경
> 2. 지상 적 (배경 위)
> 3. 플레이어 탄
> 4. 공중 적
> 5. 플레이어
> 6. 적탄 (적/플레이어 위 — 가시성)
> 7. 이펙트

> **게임오버 freeze**: lives=0 → `g_player.alive = 0` 셋되고, 위 코드가 player_update/enemy_update/effect_update/collide를 모두 skip. 적 탄은 계속 흐르되 enemy 새로 안 움직임. 나름의 "엔드 카드" 느낌. 정식 GAME_OVER FSM은 후속 plan.

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -12
ls -l SRC/GAME.EXE
```

Expected: 0 errors. GAME.EXE 갱신.

- [ ] **Step 4: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "Wire enemy bullets, bomb input, and game-over freeze

Spawn scheduler now alternates between PAT_STRAIGHT_DOWN and
PAT_AIM_PLAYER (random per-spawn). X key triggers manual bomb
(edge-detected via static was-held flag). Main loop adds enemy
bullet update/render and enemy_bullets_vs_player collision.

When g_player.alive becomes 0 (lives=0), the loop skips player /
enemy / effect / collide updates — bullets in flight finish their
arcs, scroll keeps running. ESC still exits cleanly.

Plan 3 vertical slice complete: player can die, autobomb saves
on hit, X bombs the screen, lives=0 freezes the game."
```

---

## Task 7: 시각 검증 + 마무리

- [ ] **Step 1: 사용자 시각 검증**

```bash
./run.sh
```

확인 사항:
- 적이 1초마다 등장, 약 50%는 정지/직진, 50%는 1.5초마다 플레이어 조준탄 발사
- 적탄은 4x4 빨강 사각형, 플레이어 중앙으로 향함
- 적탄에 맞으면:
  - **봄 보유 시** (시작 2개): 화면의 모든 적/적탄 폭발 + 무적 ~2.5초 (깜빡임)
  - **봄 없을 시**: 폭발 + 잔기 -1 + 같은 자리 부활 + 무적 ~2초 (깜빡임)
- 무적 중에는 적/적탄에 면역 + 깜빡임 가시화
- X 키로 수동 봄 (봄 ≥1, 무적 아닐 때): autobomb과 동일 효과
- 잔기 0이 되면 게임 정지 (적/플레이어 안 움직임, 배경만 스크롤)
- ESC 정상 종료

- [ ] **Step 2: git log 점검**

```bash
git log --oneline -10
```

Expected: Plan 3의 6개 커밋이 main 위에.

---

## Self-Review Checklist

- [ ] **Spec coverage**:
  - 적탄 풀 + 한 종류 (BUL_ENEMY_AIMED): ✓
  - PAT_AIM_PLAYER: ✓
  - 4x4 플레이어 히트박스: ✓
  - 무적 타이머 + 깜빡임: ✓
  - 사망/부활/잔기 감소: ✓
  - 봄 (수동 + 자동): ✓
  - 화면 클리어 + 적 즉사 + 무적: ✓
  - 게임오버 (lives=0): freeze는 임시; FSM은 Plan 5+
  - 1UP은 Plan 6+, 메달은 Plan 6+, 보스 데미지는 Plan 8+
- [ ] **타입 일관성**:
  - inv_ms: u32 (충분히 크기), Plan 1의 g_state.invincible_frames는 u8이지만 사용 안 됨 (Player 구조체 inv_ms로 관리)
  - fire_cooldown: u16
  - 충돌 모두 AABB
- [ ] **함수 시그니처 매칭**:
  - bullet_spawn_enemy(BulletKind, i16, i16, i16, i16) → int
  - player_take_damage(void) → void
  - player_fire_bomb(void) → void
  - collide_enemy_bullets_vs_player(void) → void
- [ ] **빌드 시스템**: 신규 *.CPP 0개 (모두 기존 모듈 확장)
- [ ] **렌더 순서**: bg → ground → p_bullets → air → player → e_bullets → effects (적탄이 player 위에 있어 가시성 좋음)

---

## Open Items (Plan 4 인계)

- HUD (점수/잔기/봄/메달 사이드 표시) — FONT 자산 셋업 필요
- 패럴랙스 클라우드 (Plan 1에서 보류한 것)
- BGM/SFX (ST00.vgm 재생 + 발사/폭발/봄 SFX)
- 정식 GAME_OVER FSM + CONTINUE 프롬프트
- 1UP 임계값 자동 가산
- 무기 종류별 데미지 (Vulcan=1, Laser=2, Plasma=3) — Plan 5 무기 다양화 시
