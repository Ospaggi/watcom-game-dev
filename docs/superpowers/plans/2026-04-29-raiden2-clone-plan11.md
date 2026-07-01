# Raiden 2 Clone — Plan 11: 메달 시스템 + B/H/1UP 픽업

> **For agentic workers:** Use superpowers:subagent-driven-development. `- [ ]` checkbox.

**Goal:** 라이덴2 정통 점수 깊이 — 적 격파 시 메달 드롭 (cascade 가치 100→10000), 봄/호밍/1UP 아이템 픽업, 봄 발동 시 화면 메달 자동 수거. 우측 HUD에 메달 가치 표시.

**Architecture:** ITEM 확장 (ITEM_MEDAL_S/L, ITEM_BOMB, ITEM_HOMING, ITEM_1UP). 신규 모듈 0. ENEMY 사망/BOSS 사망 시 ITEM 드롭. 모든 아이템 시각화는 GFX 프리미티브 + 3x5 letter 글리프로 (새 SPR 없음).

**Spec 참조:** §9.2 (B), §9.3 (H), §9.4 (메달), §9.5 (1UP), §9.6 (점수)

---

## File Map

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `SRC/ITEM.H` / `SRC/ITEM.CPP` | ItemKind 확장 (MEDAL_S/L, BOMB, HOMING, 1UP). spawn 함수들 + 렌더 분기. medal cascade 헬퍼 |
| `SRC/STATE.H` | (변경 없음 — `medal_level` 이미 존재) |
| `SRC/COLLIDE.CPP` | `collide_player_vs_items()` 새 종류별 픽업 효과 |
| `SRC/ENEMY.CPP` 또는 `SRC/COLLIDE.CPP` | 적 격파 시 메달 드롭 |
| `SRC/BOSS.CPP` | 보스 사망 시 5 메달 + B + H + 1UP 드롭 |
| `SRC/PLAYER.CPP` | 봄 발동 시 화면 메달 자동 수거 |
| `SRC/HUD.CPP` | 우측 HUD에 메달 가치 표시 |
| `SRC/GAME.CPP` | (작은 변경) |

### NOT TOUCHED

- 진짜 라이덴2 정통 메달 cascade (놓치면 한 단계 강하): 단순화 — 화면 밖 = 그냥 사라짐
- 호밍 미사일 자동 발사: 후속 plan
- 메달 흡수 곡선 (자석 효과): 단순 AABB 픽업

---

## Task 1: ITEM 확장 — 새 ItemKind + spawn 함수 + 렌더

**Files:**
- Modify: `SRC/ITEM.H`, `SRC/ITEM.CPP`

**Step 1: ITEM.H — ItemKind enum 확장 + 새 spawn 함수 + medal cascade 정의**

```c
typedef enum {
    ITEM_POWER    = 0,
    ITEM_BOMB     = 1,
    ITEM_HOMING   = 2,
    ITEM_1UP      = 3,
    ITEM_MEDAL_S  = 4,    /* small (silver) — value tier from medal_level */
    ITEM_MEDAL_L  = 5     /* large (gold)   — 2x value */
} ItemKind;

#define MEDAL_LEVEL_MAX 6   /* 0..6 = 7 tiers */
extern const u32 medal_value_table[MEDAL_LEVEL_MAX + 1];

/* Sizes used by collision + render (centered). */
#define BOMB_W   12
#define BOMB_H   12
#define HOMING_W 12
#define HOMING_H 12
#define ONEUP_W  16
#define ONEUP_H   8
#define MEDAL_S_W 8
#define MEDAL_S_H 8
#define MEDAL_L_W 12
#define MEDAL_L_H 12
```

새 spawn 함수 선언 (file 내 functions):

```c
int item_spawn_bomb  (i16 x, i16 y);
int item_spawn_homing(i16 x, i16 y);
int item_spawn_1up   (i16 x, i16 y);
int item_spawn_medal (i16 x, i16 y, int large);  /* 1 = large, 0 = small */
```

**Step 2: ITEM.CPP — medal_value_table + spawn 구현 + 렌더 확장**

```c
const u32 medal_value_table[MEDAL_LEVEL_MAX + 1] = {
    100, 200, 500, 1000, 2000, 5000, 10000
};

int item_spawn_bomb(i16 x, i16 y)
{
    int i;
    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &g_items[i];
        if (it->active) continue;
        it->active = 1;
        it->kind = (u8)ITEM_BOMB;
        it->pw_color = 0;
        it->frame = 0;
        it->anim_t_ms = 0;
        it->cycle_cd_ms = 0;
        it->spawn_t_ms = 0;
        it->x = x; it->y = y;
        it->vy_q4 = 6;
        it->spawn_x = x;
        return 1;
    }
    return 0;
}

int item_spawn_homing(i16 x, i16 y)
{
    /* same as bomb but kind=HOMING */
    int i;
    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &g_items[i];
        if (it->active) continue;
        it->active = 1;
        it->kind = (u8)ITEM_HOMING;
        it->pw_color = 0;
        it->frame = 0;
        it->anim_t_ms = 0;
        it->cycle_cd_ms = 0;
        it->spawn_t_ms = 0;
        it->x = x; it->y = y;
        it->vy_q4 = 6;
        it->spawn_x = x;
        return 1;
    }
    return 0;
}

int item_spawn_1up(i16 x, i16 y)
{
    int i;
    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &g_items[i];
        if (it->active) continue;
        it->active = 1;
        it->kind = (u8)ITEM_1UP;
        it->pw_color = 0;
        it->frame = 0;
        it->anim_t_ms = 0;
        it->cycle_cd_ms = 0;
        it->spawn_t_ms = 0;
        it->x = x; it->y = y;
        it->vy_q4 = 5;
        it->spawn_x = x;
        return 1;
    }
    return 0;
}

int item_spawn_medal(i16 x, i16 y, int large)
{
    int i;
    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &g_items[i];
        if (it->active) continue;
        it->active = 1;
        it->kind = (u8)(large ? ITEM_MEDAL_L : ITEM_MEDAL_S);
        it->pw_color = 0;
        it->frame = 0;
        it->anim_t_ms = 0;
        it->cycle_cd_ms = 0;
        it->spawn_t_ms = 0;
        it->x = x; it->y = y;
        it->vy_q4 = 8;     /* medals fall slightly faster than P */
        it->spawn_x = x;
        return 1;
    }
    return 0;
}
```

**Step 3: ITEM.CPP 렌더 — kind별 분기 (기존 P 렌더는 ITEM_POWER로만)**

기존 `item_render()` 본문을 다음으로:

```c
/* Forward decl from HUD.CPP — using a local copy for medal/bomb/homing/1up
 * letter rendering. */
static const unsigned char letter_local[26][5] = {
    {7,5,7,5,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,6,4,7},
    {7,4,6,4,4},{7,4,5,5,7},{5,5,7,5,5},{7,2,2,2,7},{1,1,1,5,7},
    {5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{5,7,7,7,5},{7,5,5,5,7},
    {7,5,7,4,4},{7,5,5,7,1},{6,5,6,5,5},{7,4,7,1,7},{7,2,2,2,2},
    {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},
    {7,1,2,4,7},
};

static void draw_letter_at(int x, int y, char ch, unsigned char color)
{
    if (ch < 'A' || ch > 'Z') return;
    int idx = ch - 'A';
    int row, col;
    for (row = 0; row < 5; row++) {
        unsigned char b = letter_local[idx][row];
        for (col = 0; col < 3; col++) {
            if (b & (4 >> col)) gfx_pixel(x + col, y + row, color);
        }
    }
}

static void render_item_power(const Item *it, int sx, int sy)
{
    unsigned char c = outline_color(it->pw_color);
    gfx_rect(sx - 1, sy - 1, PWUP_W + 2, PWUP_H + 2, c);
    gfx_draw_csprite(g_spr_pwup.frames[it->frame], sx, sy);
}

static void render_item_box(int sx, int sy, int w, int h, unsigned char fill,
                            unsigned char outline, char letter)
{
    gfx_fill_rect(sx, sy, w, h, fill);
    gfx_rect(sx, sy, w, h, outline);
    /* letter centered (3x5 glyph) */
    int lx = sx + (w - 3) / 2;
    int ly = sy + (h - 5) / 2;
    draw_letter_at(lx, ly, letter, outline);
}

void item_render(void)
{
    int i;
    for (i = 0; i < MAX_ITEMS; i++) {
        const Item *it = &g_items[i];
        if (!it->active) continue;

        switch (it->kind) {
        case ITEM_POWER: {
            int sx = PLAY_X0 + it->x - PWUP_HALF_W;
            int sy = it->y - PWUP_HALF_H;
            if (sx < 0 || sy < 0 || sx + PWUP_W > 320 || sy + PWUP_H > 200) continue;
            render_item_power(it, sx, sy);
            break;
        }
        case ITEM_BOMB: {
            int sx = PLAY_X0 + it->x - BOMB_W / 2;
            int sy = it->y - BOMB_H / 2;
            render_item_box(sx, sy, BOMB_W, BOMB_H,
                            27 /* yellow-ish placeholder */, 15, 'B');
            break;
        }
        case ITEM_HOMING: {
            int sx = PLAY_X0 + it->x - HOMING_W / 2;
            int sy = it->y - HOMING_H / 2;
            render_item_box(sx, sy, HOMING_W, HOMING_H,
                            105 /* orange-ish placeholder */, 15, 'H');
            break;
        }
        case ITEM_1UP: {
            int sx = PLAY_X0 + it->x - ONEUP_W / 2;
            int sy = it->y - ONEUP_H / 2;
            gfx_fill_rect(sx, sy, ONEUP_W, ONEUP_H, 158 /* cyan */);
            gfx_rect(sx, sy, ONEUP_W, ONEUP_H, 15);
            /* "1UP" — 3x5 chars in 16x8 box */
            draw_letter_at(sx + 9, sy + 1, 'U', 15);
            draw_letter_at(sx + 12 + 1, sy + 1, 'P', 15);   /* shift right */
            /* '1' digit at left */
            /* (digits handled inline via letter_local can't do — skip; HUD
             * already shows lives, this overlay is short-lived). */
            break;
        }
        case ITEM_MEDAL_S: {
            int sx = PLAY_X0 + it->x - MEDAL_S_W / 2;
            int sy = it->y - MEDAL_S_H / 2;
            gfx_fill_rect(sx, sy, MEDAL_S_W, MEDAL_S_H, 23 /* silver-ish */);
            gfx_rect(sx, sy, MEDAL_S_W, MEDAL_S_H, 15);
            break;
        }
        case ITEM_MEDAL_L: {
            int sx = PLAY_X0 + it->x - MEDAL_L_W / 2;
            int sy = it->y - MEDAL_L_H / 2;
            gfx_fill_rect(sx, sy, MEDAL_L_W, MEDAL_L_H, 27 /* gold-ish */);
            gfx_rect(sx, sy, MEDAL_L_W, MEDAL_L_H, 15);
            break;
        }
        }
    }
}
```

> **참고**: `outline_color()`, `g_spr_pwup` 는 ITEM.CPP에 이미 있는 static들. 수정 없이 그대로 사용.

**Step 4: 빌드 + 커밋**

```bash
./build.sh
cat BUILD.LOG | tail -5
git add SRC/ITEM.H SRC/ITEM.CPP
git commit -m "Extend ITEM — bomb/homing/1up/medal kinds + per-kind render

New ItemKind: BOMB, HOMING, 1UP, MEDAL_S, MEDAL_L. Each has its own
spawn function. Render now branches by kind:
  POWER  : original sprite + colored outline
  BOMB   : 12x12 yellow box with 'B' letter
  HOMING : 12x12 orange box with 'H' letter
  1UP    : 16x8 cyan box with 'UP' letters (1 deferred — HUD shows
           lives)
  MEDAL_S: 8x8 silver box
  MEDAL_L: 12x12 gold box

Adds medal_value_table[7] = {100, 200, 500, 1000, 2000, 5000, 10000}
for the cascade. Local letter glyph table mirrors HUD's set."
```

---

## Task 2: 적/보스 사망 시 아이템 드롭

**Files:**
- Modify: `SRC/COLLIDE.CPP` (popcorn 격파 시점), `SRC/BOSS.CPP` (보스 dying 진입 시)

**Step 1: COLLIDE.CPP — popcorn 사망 시 메달/B/H/1UP 드롭**

기존 popcorn 사망 분기:

```c
            } else {
                e->active = 0;
                effect_spawn_explosion(e->x, e->y);
                g_state.score += 100;
                sfx_play(SFX_HIT);
            }
```

다음으로 변경:

```c
            } else {
                i16 dx = e->x;
                i16 dy = e->y;
                e->active = 0;
                effect_spawn_explosion(dx, dy);
                g_state.score += 100;
                sfx_play(SFX_HIT);

                /* Drop item (deterministic-ish via simple LCG seeded by position) */
                static u32 drop_rng = 0xDEADBEEFUL;
                drop_rng = drop_rng * 1103515245UL + 12345UL + (u32)dx;
                u32 r = drop_rng & 0xFF;
                if (r < 5) {
                    item_spawn_1up(dx, dy);                 /* 5/256 ~ 2% */
                } else if (r < 20) {
                    item_spawn_homing(dx, dy);              /* 15/256 ~ 6% */
                } else if (r < 35) {
                    item_spawn_bomb(dx, dy);                /* 15/256 ~ 6% */
                } else {
                    item_spawn_medal(dx, dy, 0);            /* small medal — rest */
                }
            }
```

**Step 2: BOSS.CPP — 사망 시 일괄 드롭 (Tank2 처치)**

`boss_take_damage()`의 hp == 0 분기 (dying 시작 시):

```c
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
    sfx_play(SFX_BOMB);

    /* Big haul: 5 large medals + 1 each B/H/1UP, scattered around boss. */
    int i;
    for (i = 0; i < 5; i++) {
        i16 ox = (i16)(-30 + i * 15);
        item_spawn_medal((i16)(g_boss.x + ox), (i16)(g_boss.y + 10), 1);
    }
    item_spawn_bomb  ((i16)(g_boss.x - 20), (i16)(g_boss.y - 10));
    item_spawn_homing((i16)(g_boss.x),       (i16)(g_boss.y - 10));
    item_spawn_1up   ((i16)(g_boss.x + 20), (i16)(g_boss.y - 10));

    return 1;
}
```

`#include "ITEM.H"` 추가.

**Step 3: 빌드 + 커밋**

```bash
./build.sh
cat BUILD.LOG | tail -5
git add SRC/COLLIDE.CPP SRC/BOSS.CPP
git commit -m "Enemy + boss death drop items

popcorn: ~75% small medal, 6% B, 6% H, 2% 1UP, ~10% nothing.
boss death: 5 large medals + 1 each of B / H / 1UP scattered."
```

---

## Task 3: COLLIDE — 새 픽업 효과 + 메달 cascade

**Files:**
- Modify: `SRC/COLLIDE.CPP`

**Step 1: `collide_player_vs_items()` — kind별 분기 + 메달 cascade**

기존 P 픽업 한 가지였던 함수를 switch/case로:

```c
void collide_player_vs_items(void)
{
    int ii;

    if (!g_player.alive) return;

    int p_l = g_player.x - PLAYER_HALF_W;
    int p_t = g_player.y - PLAYER_HALF_H;
    int p_r = g_player.x + PLAYER_HALF_W;
    int p_b = g_player.y + PLAYER_HALF_H;

    for (ii = 0; ii < MAX_ITEMS; ii++) {
        Item *it = &g_items[ii];
        if (!it->active) continue;

        int hw, hh;
        switch (it->kind) {
        case ITEM_POWER:    hw = PWUP_HALF_W;     hh = PWUP_HALF_H;     break;
        case ITEM_BOMB:     hw = BOMB_W / 2;      hh = BOMB_H / 2;      break;
        case ITEM_HOMING:   hw = HOMING_W / 2;    hh = HOMING_H / 2;    break;
        case ITEM_1UP:      hw = ONEUP_W / 2;     hh = ONEUP_H / 2;     break;
        case ITEM_MEDAL_S:  hw = MEDAL_S_W / 2;   hh = MEDAL_S_H / 2;   break;
        case ITEM_MEDAL_L:  hw = MEDAL_L_W / 2;   hh = MEDAL_L_H / 2;   break;
        default:            continue;
        }
        int e_l = it->x - hw;
        int e_t = it->y - hh;
        int e_r = it->x + hw;
        int e_b = it->y + hh;
        if (p_r <= e_l || p_l >= e_r || p_b <= e_t || p_t >= e_b) continue;

        /* Pickup */
        switch (it->kind) {
        case ITEM_POWER:
            g_state.weapon = it->pw_color;
            if (g_state.weapon_level < 4) g_state.weapon_level++;
            g_state.score += 10000;
            break;
        case ITEM_BOMB:
            if (g_state.bombs < 7) {
                g_state.bombs++;
                g_state.score += 5000;
            } else {
                g_state.score += 50000;
            }
            break;
        case ITEM_HOMING:
            g_state.score += 30000;
            break;
        case ITEM_1UP:
            if (g_state.lives < 9) g_state.lives++;
            g_state.score += 30000;
            break;
        case ITEM_MEDAL_S: {
            u32 v = medal_value_table[g_state.medal_level];
            g_state.score += v;
            if (g_state.medal_level < MEDAL_LEVEL_MAX) g_state.medal_level++;
            break;
        }
        case ITEM_MEDAL_L: {
            u32 v = medal_value_table[g_state.medal_level] * 2;
            g_state.score += v;
            if (g_state.medal_level < MEDAL_LEVEL_MAX) g_state.medal_level++;
            break;
        }
        }
        it->active = 0;
        sfx_play(SFX_PWR);
    }
}
```

**Step 2: 빌드 + 커밋**

```bash
git add SRC/COLLIDE.CPP
git commit -m "COLLIDE — per-kind pickup effects (B/H/1UP/medals)

New pickup effects:
  BOMB   : bombs +1 (cap 7), +5000 score (or +50000 at cap)
  HOMING : +30000 score (homing weapon level deferred)
  1UP    : lives +1 (cap 9), +30000 score
  MEDAL_S: +medal_value_table[medal_level], level++ (cap 6)
  MEDAL_L: 2x value, level++

POWER pickup logic preserved (weapon switch + level up)."
```

---

## Task 4: PLAYER 봄 — 화면 메달 자동 수거

**Files:**
- Modify: `SRC/PLAYER.CPP`

**Step 1: blast_all_enemies() — 화면 모든 메달 수거 (점수만, 레벨 변화 없음)**

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
    /* Bomb damages active boss */
    if (g_boss.active && !g_boss.dying) {
        boss_take_damage(50);
    }
    /* Auto-collect all medals on screen (spec section 9.4). Only medals,
     * not B/H/1UP/POWER — those still need manual pickup. */
    for (i = 0; i < MAX_ITEMS; i++) {
        Item *it = &g_items[i];
        if (!it->active) continue;
        if (it->kind == ITEM_MEDAL_S) {
            g_state.score += medal_value_table[g_state.medal_level];
            if (g_state.medal_level < MEDAL_LEVEL_MAX) g_state.medal_level++;
            it->active = 0;
        } else if (it->kind == ITEM_MEDAL_L) {
            g_state.score += medal_value_table[g_state.medal_level] * 2;
            if (g_state.medal_level < MEDAL_LEVEL_MAX) g_state.medal_level++;
            it->active = 0;
        }
    }
    sfx_play(SFX_BOMB);
}
```

추가 include: `#include "ITEM.H"`. (이미 있을 수 있음 — 확인.)

**Step 2: 빌드 + 커밋**

```bash
git add SRC/PLAYER.CPP
git commit -m "Bomb auto-collects medals on screen (spec 9.4)

blast_all_enemies sweeps the item pool and grants medal value (with
level cascade) for any MEDAL_S / MEDAL_L item active on screen.
B / H / 1UP / POWER items are NOT swept — players still need to
fly to pick those up."
```

---

## Task 5: HUD — 우측에 메달 가치 + 1UP/B 카운터

**Files:**
- Modify: `SRC/HUD.CPP`

**Step 1: 우측 HUD 사용 — y=4 메달 가치 (5자리), y=12 "MED" 라벨**

`hud_render()` 끝에 추가:

```c
    /* Right HUD (x=288..319) — medal value of next pickup. */
    {
        u32 mv = medal_value_table[g_state.medal_level];
        /* "MED" label at top */
        draw_text(290, 4, "MED", HUD_TEXT_COLOR);
        /* value: up to 5 digits, right-aligned at x=319 */
        draw_number(318, 12, mv, 5, HUD_TEXT_COLOR);
    }
```

`#include "ITEM.H"` 추가.

**Step 2: 빌드 + 커밋**

```bash
git add SRC/HUD.CPP
git commit -m "HUD — right side shows next medal value

x=290..318 column. Top row 'MED' label, second row 5-digit medal
value (medal_value_table[medal_level]). Updates live as the
cascade advances on pickup."
```

---

## Task 6: 시각 검증

```bash
./run.sh
```

확인 사항:
- popcorn 격파 시 작은 메달 (회색-은) 떨어짐, 가끔 B(노랑) / H(주황) / 1UP(시안)
- 메달 픽업 → HUD 우측 "MED" 값 증가 (100 → 200 → ...)
- 봄 발동 → 화면의 메달 자동 수거 (점수 +)
- 보스 격파 → 5 큰 메달 (금) + B + H + 1UP 동시 드롭
- 잔기 0 후 콘티뉴 시 메달 레벨 0으로 리셋 (state_reset_for_continue 동작)

---

## Open Items (Plan 12+)

- 메달 화면 밖 흘리면 한 단계 강하 (spec 9.4 "흘리면 한 단계 강하")
- 호밍 미사일 자동 발사 (spec 4.4)
- 2면, 3면 + 보스 (MG, Kl)
- 진짜 STAGE 시스템 (시간축 스크립트)
