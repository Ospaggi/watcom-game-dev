# Raiden 2 Clone — Plan 8: 정식 FSM (TITLE → PLAYING → ... → CONTINUE)

> **For agentic workers:** Use superpowers:subagent-driven-development. Steps use `- [ ]` checkbox tracking.

**Goal:** 게임 흐름을 정식 상태머신으로. TITLE 화면 → STAGE_INTRO → PLAYING → BOSS_INTRO ("WARNING!") → BOSS_FIGHT → STAGE_CLEAR (보너스 점수 카운트업) → CONTINUE_PROMPT (10초 카운트다운) → 다시 TITLE. 게임이 처음부터 끝까지 흐름이 자연스럽게.

**Architecture:** GAME.CPP의 단일 메인 루프 안에서 `g_state.gs` 상태 머신 분기. 각 상태별 update/render 분기. HUD는 상태에 따라 다르게 표시. 신규 모듈 0 — GAME.CPP + HUD.CPP만 확장.

**Tech Stack:** Watcom C++, GFX 그대로.

**Spec 참조:** `docs/superpowers/specs/2026-04-28-raiden2-clone-design.md` §12 (상태머신)

---

## File Map

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `SRC/HUD.H` / `SRC/HUD.CPP` | 3x5 글리프에 알파벳 일부 (S/T/A/G/E/W/R/N/I/L/V/C/O/P/U) 추가 + `hud_draw_text()` 헬퍼 + 상태별 오버레이 함수 (`hud_render_title`, `hud_render_stage_intro`, `hud_render_warning`, `hud_render_stage_clear`, `hud_render_continue`, `hud_render_game_over`) |
| `SRC/GAME.CPP` | `g_state.gs` 분기로 update/render 처리. 상태 전이 로직 (gs_t_ms 사용). state_reset에서 GS_TITLE로 시작. PLAYING 외 상태에서는 게임 객체들 일부 freeze. |
| `SRC/STATE.H` | (변경 없음 — 기존 GameStateId enum 그대로 사용) |

### NOT TOUCHED

- 보스 인트로 시각 효과 (빨간 화면 깜빡임): 후속 plan
- 1UP 임계값 자동 가산 알림: 후속 plan
- 풀 한글 폰트 모듈 (FONT.CPP) 통합: 후속 plan

---

## Task 1: HUD 알파벳 글리프 + 텍스트 렌더 헬퍼

**Files:**
- Modify: `SRC/HUD.H`, `SRC/HUD.CPP`

3x5 글리프에 영문 대문자 추가. 모든 26자는 과해서 이 plan에서 필요한 글자만:

필요한 단어 + 글자:
- "STAGE 1 START" → S, T, A, G, E, 1, R
- "WARNING" → W, A, R, N, I, G
- "STAGE CLEAR" → S, T, A, G, E, C, L, R
- "CONTINUE" → C, O, N, T, I, U, E
- "GAME OVER" → G, A, M, E, O, V, R
- "TITLE" / "PRESS Z" → T, I, L, E, P, R, S, Z
- "RAIDEN 2" 또는 "STG-1" 같은 타이틀 텍스트

총 글자 set: A B C D E G I L M N O P R S T U V W Z → 약 19개

각 3x5 비트맵으로 encode. Plan 5의 digit3x5 옆에 letter3x5 추가.

- [ ] **Step 1: HUD.CPP — 알파벳 글리프 테이블 추가**

기존 `digit3x5[10][5]` 다음에 추가:

```c
/* 3x5 bitmap font for uppercase letters used by FSM overlays.
 * Each row is a 3-bit mask: bit 2 = leftmost, bit 0 = rightmost. */
static const unsigned char letter3x5[26][5] = {
    /* A */ { 7, 5, 7, 5, 5 },
    /* B */ { 6, 5, 6, 5, 6 },
    /* C */ { 7, 4, 4, 4, 7 },
    /* D */ { 6, 5, 5, 5, 6 },
    /* E */ { 7, 4, 6, 4, 7 },
    /* F */ { 7, 4, 6, 4, 4 },
    /* G */ { 7, 4, 5, 5, 7 },
    /* H */ { 5, 5, 7, 5, 5 },
    /* I */ { 7, 2, 2, 2, 7 },
    /* J */ { 1, 1, 1, 5, 7 },
    /* K */ { 5, 5, 6, 5, 5 },
    /* L */ { 4, 4, 4, 4, 7 },
    /* M */ { 5, 7, 7, 5, 5 },
    /* N */ { 5, 7, 7, 7, 5 },
    /* O */ { 7, 5, 5, 5, 7 },
    /* P */ { 7, 5, 7, 4, 4 },
    /* Q */ { 7, 5, 5, 7, 1 },
    /* R */ { 6, 5, 6, 5, 5 },
    /* S */ { 7, 4, 7, 1, 7 },
    /* T */ { 7, 2, 2, 2, 2 },
    /* U */ { 5, 5, 5, 5, 7 },
    /* V */ { 5, 5, 5, 5, 2 },
    /* W */ { 5, 5, 7, 7, 5 },
    /* X */ { 5, 5, 2, 5, 5 },
    /* Y */ { 5, 5, 2, 2, 2 },
    /* Z */ { 7, 1, 2, 4, 7 },
};

/* Draw uppercase letter at (x, y). Returns the cell width consumed (4 px = 3 + 1 gap). */
static int draw_letter(int x, int y, char ch, unsigned char color)
{
    if (ch == ' ') return 4;
    if (ch < 'A' || ch > 'Z') return 4;
    int row, col;
    unsigned char *bits;
    int idx = ch - 'A';
    for (row = 0; row < 5; row++) {
        unsigned char b = letter3x5[idx][row];
        for (col = 0; col < 3; col++) {
            if (b & (4 >> col)) gfx_pixel(x + col, y + row, color);
        }
    }
    return 4;
}
```

> **참고**: Q, X, Y, J, K, F, H 등 plan 8에서 안 쓰는 글자도 정의 — 후속 plan에서 재사용 가능. unused 변수 `bits` 경고 가능 — 위 코드는 idx 배열 직접 참조하므로 bits 변수 제거 필요. 실제 코드에선:

```c
static int draw_letter(int x, int y, char ch, unsigned char color)
{
    if (ch == ' ') return 4;
    if (ch < 'A' || ch > 'Z') return 4;
    int row, col;
    int idx = ch - 'A';
    for (row = 0; row < 5; row++) {
        unsigned char b = letter3x5[idx][row];
        for (col = 0; col < 3; col++) {
            if (b & (4 >> col)) gfx_pixel(x + col, y + row, color);
        }
    }
    return 4;
}
```

`hud_draw_text()` 헬퍼 추가 — 문자열 렌더 (대문자 + 숫자 + 공백):

```c
/* Draw a string at (x, y) — supports uppercase letters, digits, space. */
static int draw_text(int x, int y, const char *str, unsigned char color)
{
    int dx = x;
    while (*str) {
        char ch = *str++;
        if (ch == ' ') {
            dx += 4;
        } else if (ch >= '0' && ch <= '9') {
            draw_digit(dx, y, ch - '0', color);
            dx += 4;
        } else if (ch >= 'A' && ch <= 'Z') {
            dx += draw_letter(dx, y, ch, color);
        } else {
            dx += 4;  /* unknown char = blank */
        }
    }
    return dx - x;
}

static int text_width(const char *str)
{
    int w = 0;
    while (*str++) w += 4;
    return w;
}

/* Draw text horizontally centered at given y, in the play area (x=32..287). */
static void draw_text_centered(int y, const char *str, unsigned char color)
{
    int w = text_width(str);
    int x = 32 + (256 - w) / 2;
    draw_text(x, y, str, color);
}
```

- [ ] **Step 2: HUD.H에 새 외부 함수 선언 추가**

기존 `hud_render` 다음에 추가:

```c
/* State-specific overlay renderers — call instead of (or alongside)
 * the regular hud_render() based on g_state.gs. */
void hud_render_title(void);
void hud_render_stage_intro(void);
void hud_render_boss_warning(void);
void hud_render_stage_clear(void);
void hud_render_continue_prompt(int seconds_left);
void hud_render_game_over(void);
```

- [ ] **Step 3: HUD.CPP에 상태별 렌더 함수 추가**

기존 `hud_render()`의 STAGE_CLEAR placeholder 블록은 제거 (전용 함수로 이동).

```c
void hud_render_title(void)
{
    /* Black background — caller did gfx_clear */
    draw_text_centered(60,  "RAIDEN 2 CLONE",  HUD_TEXT_COLOR);
    draw_text_centered(80,  "STAGE 1 DEMO",    25);                   /* red highlight */
    draw_text_centered(120, "PRESS Z TO START", HUD_TEXT_COLOR);
    draw_text_centered(150, "ESC TO QUIT",      HUD_LIFE_COLOR);
}

void hud_render_stage_intro(void)
{
    draw_text_centered(80,  "STAGE 1",       HUD_TEXT_COLOR);
    draw_text_centered(100, "DESERT",         25);
    draw_text_centered(130, "GET READY",      HUD_TEXT_COLOR);
}

void hud_render_boss_warning(void)
{
    /* WARNING text in red, blinking via odd-frame alpha. Caller blanks
     * play area for blinking effect; we just draw the text. */
    draw_text_centered(80,  "WARNING",       25);
    draw_text_centered(100, "BIG ENEMY",      HUD_TEXT_COLOR);
    draw_text_centered(120, "APPROACHING",    HUD_TEXT_COLOR);
}

void hud_render_stage_clear(void)
{
    draw_text_centered(60,  "STAGE 1 CLEAR",  HUD_TEXT_COLOR);
    /* Show running score (counts up over the freeze period). */
    int sx = 32 + (256 - 7 * 4) / 2;
    draw_number(sx + 7 * 4 - 3, 90, g_state.score, 7, HUD_TEXT_COLOR);
}

void hud_render_continue_prompt(int seconds_left)
{
    draw_text_centered(70,  "CONTINUE",       HUD_TEXT_COLOR);
    /* big seconds digit centered */
    int dx = 32 + (256 - 3) / 2;
    draw_digit(dx, 90, seconds_left, 25);
    draw_text_centered(110, "PRESS Z",        HUD_LIFE_COLOR);
}

void hud_render_game_over(void)
{
    draw_text_centered(80, "GAME OVER", HUD_TEXT_COLOR);
}
```

- [ ] **Step 4: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -8
```

Expected: 0 errors. HUD.CPP 라인 수 ~200 → 350 정도.

- [ ] **Step 5: 커밋**

```bash
git add SRC/HUD.H SRC/HUD.CPP
git commit -m "HUD — alphabet glyphs + state-specific overlay renderers

Adds 3x5 bitmap font for A-Z (mirrors Plan 5's digit3x5[]), draw_letter
+ draw_text + draw_text_centered helpers. New public functions for
each FSM overlay state: hud_render_title / stage_intro / boss_warning
/ stage_clear / continue_prompt / game_over.

Plan 5's STAGE CLEAR placeholder block in hud_render() is moved into
hud_render_stage_clear() and removed from the always-on path.

GAME.CPP wires these to g_state.gs in Task 2."
```

---

## Task 2: GAME.CPP — FSM 상태 처리

**Files:**
- Modify: `SRC/GAME.CPP`

`g_state.gs`로 분기. 상태별로 update/render/input 다르게.

상태 전이 다이어그램:

```
TITLE ─[Z]─→ STAGE_INTRO (1.5s) ─[end]─→ PLAYING
PLAYING ─[stage_t_ms ≥ 30s]─→ BOSS_INTRO (3s) ─[end]─→ BOSS_FIGHT
BOSS_FIGHT ─[boss dead, dying done]─→ STAGE_CLEAR (5s, score countup) ─[end]─→ ENDING
PLAYING/BOSS_FIGHT ─[lives = 0]─→ GAME_OVER (2s) ─[end]─→ CONTINUE_PROMPT (10s)
CONTINUE_PROMPT ─[Z]─→ PLAYING (점수 0, 잔기 3, 봄 2, 위치 유지)
CONTINUE_PROMPT ─[10s 타임아웃 또는 ESC]─→ TITLE
ENDING (3면 클리어 후) ─[fade]─→ TITLE  (Plan 8: STAGE_CLEAR로 끝, ENDING 미사용)
```

Plan 8에서는 1면만 있으므로 STAGE_CLEAR 다음은 그냥 freeze (ESC로 TITLE 복귀) 또는 TITLE로 자동.

**핵심 결정**:
- 상태별 update — 매 상태에 맞는 update 호출
- 상태별 input — 상태에 따라 어떤 키 처리
- 상태별 render — hud_render_* 함수 호출 + 게임 객체 렌더 여부

- [ ] **Step 1: state_reset에 초기 GS_TITLE**

```c
static void state_reset(void)
{
    g_state.score = 0;
    g_state.next_extend = 1000000UL;
    g_state.lives = 3;
    g_state.bombs = 2;
    g_state.weapon = PW_VULCAN;
    g_state.weapon_level = 1;
    g_state.medal_level = 0;
    g_state.invincible_frames = 0;
    g_state.stage = 1;
    g_state.continues_used = 0;
    g_state.stage_t_ms = 0;
    g_state.gs = GS_TITLE;          /* CHANGED: was GS_PLAYING */
    g_state.gs_t_ms = 0;
    g_spawn_t_ms = 0;
    g_item_spawn_t_ms = 0;
    g_boss.active = 0;
}
```

추가로 `state_reset_for_continue()` 헬퍼 — CONTINUE 시 호출, 점수만 0, 다른 건 유지하지 않고 진행 위치만:

```c
static void state_reset_for_continue(void)
{
    g_state.score = 0;
    g_state.lives = 3;
    g_state.bombs = 2;
    g_state.weapon = PW_VULCAN;
    g_state.weapon_level = 1;
    g_state.medal_level = 0;
    g_state.invincible_frames = 0;
    g_state.continues_used++;
    g_state.gs = GS_PLAYING;
    g_state.gs_t_ms = 0;
    /* Keep: stage, stage_t_ms (resume same point) */
    g_player.alive = 1;
    g_player.inv_ms = PLAYER_INV_RESPAWN_MS;
}
```

- [ ] **Step 2: 메인 루프 — 상태별 분기**

기존 메인 루프 전체를 다음 구조로 교체:

```c
while (!input_key(KEY_ESC) || g_state.gs != GS_TITLE) {
    u32 now = timer_ms();
    u32 dt = now - g_last_ms;
    g_last_ms = now;

    g_state.gs_t_ms += dt;

    /* BGM loop */
    if (!snd_playing()) snd_play();

    /* X key edge detection (only meaningful in PLAYING/BOSS_FIGHT) */
    static int g_x_was_held = 0;
    int x_held = input_key(KEY_X);
    int x_pressed = (x_held && !g_x_was_held);
    g_x_was_held = x_held;

    /* Z key edge detection (used by TITLE/CONTINUE_PROMPT) */
    static int g_z_was_held = 0;
    int z_held = input_key(KEY_Z);
    int z_pressed = (z_held && !g_z_was_held);
    g_z_was_held = z_held;

    switch (g_state.gs) {

    case GS_TITLE:
        /* Background still scrolls — game shouldn't feel dead */
        bg_update(dt);
        if (z_pressed) {
            state_reset();
            g_state.gs = GS_STAGE_INTRO;
            g_state.gs_t_ms = 0;
        }
        if (input_key(KEY_ESC)) goto loop_exit;   /* full quit only from TITLE */
        gfx_clear(0);
        bg_render();
        hud_render_title();
        break;

    case GS_STAGE_INTRO:
        bg_update(dt);
        if (g_state.gs_t_ms >= 1500) {
            g_state.gs = GS_PLAYING;
            g_state.gs_t_ms = 0;
            g_state.stage_t_ms = 0;
        }
        gfx_clear(0);
        bg_render();
        hud_render_stage_intro();
        break;

    case GS_PLAYING:
        /* Boss trigger */
        g_state.stage_t_ms += dt;
        if (g_state.stage_t_ms >= 30000 && !g_boss.active) {
            g_state.gs = GS_BOSS_INTRO;
            g_state.gs_t_ms = 0;
            break;
        }
        /* Spawn scaffolds */
        g_spawn_t_ms += dt;
        if (g_spawn_t_ms >= 1000) {
            g_spawn_t_ms -= 1000;
            i16 sx = (i16)(POPCORN_HALF_W + (i16)(next_rand() % (256 - POPCORN_W)));
            EnemyPattern pat = (next_rand() & 1) ? PAT_AIM_PLAYER : PAT_STRAIGHT_DOWN;
            enemy_spawn(E_POPCORN_A, PLANE_AIR, pat, sx, (i16)(-POPCORN_HALF_H), 0, 24);
        }
        g_item_spawn_t_ms += dt;
        if (g_item_spawn_t_ms >= 5000) {
            g_item_spawn_t_ms -= 5000;
            i16 ix = (i16)(PWUP_HALF_W + (i16)(next_rand() % (256 - PWUP_W)));
            PowerWeapon col = (PowerWeapon)(next_rand() % 3);
            item_spawn_power(ix, (i16)(-PWUP_HALF_H), col);
        }
        /* Input + updates + collide (full play) */
        if (x_pressed) player_fire_bomb();
        bg_update(dt);
        if (g_player.alive) {
            player_update(dt);
            weapon_fire_tick(dt, z_held);
        }
        bullet_update_all(dt);
        bullet_update_all_enemy(dt);
        if (g_player.alive) {
            enemy_update_all(dt);
            effect_update_all(dt);
            item_update_all(dt);
            collide_player_bullets_vs_enemies();
            collide_enemy_bullets_vs_player();
            collide_player_bullets_vs_items();
            collide_player_vs_items();
        }
        /* Death -> GAME_OVER */
        if (!g_player.alive) {
            g_state.gs = GS_GAME_OVER;
            g_state.gs_t_ms = 0;
        }
        gfx_clear(0);
        bg_render();
        bullet_render_player();
        enemy_render_air();
        player_render();
        bullet_render_enemy();
        item_render();
        effect_render();
        hud_render();
        break;

    case GS_BOSS_INTRO:
        /* 3-second WARNING. Bullets continue, enemies finish, no new spawn. */
        bg_update(dt);
        bullet_update_all(dt);
        bullet_update_all_enemy(dt);
        enemy_update_all(dt);
        effect_update_all(dt);
        item_update_all(dt);
        if (g_player.alive) {
            player_update(dt);
            collide_enemy_bullets_vs_player();
        }
        if (g_state.gs_t_ms >= 3000) {
            boss_spawn_tank2();
            g_state.gs = GS_BOSS_FIGHT;
            g_state.gs_t_ms = 0;
        }
        gfx_clear(0);
        bg_render();
        bullet_render_player();
        enemy_render_air();
        if (g_player.alive) player_render();
        bullet_render_enemy();
        effect_render();
        hud_render_boss_warning();
        hud_render();   /* score/lives/bombs still relevant */
        break;

    case GS_BOSS_FIGHT:
        /* Same as PLAYING but no popcorn/P spawn, boss is alive. */
        if (x_pressed) player_fire_bomb();
        bg_update(dt);
        if (g_player.alive) {
            player_update(dt);
            weapon_fire_tick(dt, z_held);
        }
        bullet_update_all(dt);
        bullet_update_all_enemy(dt);
        if (g_player.alive) {
            enemy_update_all(dt);
            effect_update_all(dt);
            item_update_all(dt);
            boss_update(dt);
            collide_player_bullets_vs_enemies();
            collide_enemy_bullets_vs_player();
            collide_player_bullets_vs_boss();
            collide_boss_vs_player();
        }
        /* Boss dead? */
        if (!g_boss.active) {
            g_state.gs = GS_STAGE_CLEAR;
            g_state.gs_t_ms = 0;
        }
        if (!g_player.alive) {
            g_state.gs = GS_GAME_OVER;
            g_state.gs_t_ms = 0;
        }
        gfx_clear(0);
        bg_render();
        bullet_render_player();
        enemy_render_air();
        boss_render();
        if (g_player.alive) player_render();
        bullet_render_enemy();
        effect_render();
        hud_render();
        break;

    case GS_STAGE_CLEAR:
        /* 5-second freeze with score countup overlay. */
        bg_update(dt);
        effect_update_all(dt);
        if (g_state.gs_t_ms >= 5000) {
            /* For Plan 8: just go back to TITLE (only 1 stage). */
            g_state.gs = GS_TITLE;
            g_state.gs_t_ms = 0;
        }
        gfx_clear(0);
        bg_render();
        effect_render();
        hud_render_stage_clear();
        break;

    case GS_GAME_OVER:
        /* 2-second "GAME OVER" then go to CONTINUE prompt. */
        bg_update(dt);
        if (g_state.gs_t_ms >= 2000) {
            g_state.gs = GS_CONTINUE_PROMPT;
            g_state.gs_t_ms = 0;
        }
        gfx_clear(0);
        bg_render();
        hud_render_game_over();
        break;

    case GS_CONTINUE_PROMPT: {
        /* 10-second countdown. Z = continue, timeout/ESC = back to TITLE. */
        bg_update(dt);
        int sec_left = 10 - (int)(g_state.gs_t_ms / 1000);
        if (sec_left < 0) sec_left = 0;
        if (z_pressed) {
            state_reset_for_continue();
            break;
        }
        if (g_state.gs_t_ms >= 10000) {
            g_state.gs = GS_TITLE;
            g_state.gs_t_ms = 0;
            break;
        }
        gfx_clear(0);
        bg_render();
        hud_render_continue_prompt(sec_left);
        break;
    }

    case GS_ENDING:
        /* unused in Plan 8 */
        g_state.gs = GS_TITLE;
        g_state.gs_t_ms = 0;
        break;
    }

    gfx_vsync();
    gfx_flip();
}
loop_exit:;
```

> **참고**: `goto loop_exit;` 는 ESC가 TITLE 외 상태에서 무시되도록 막기 위함. ESC는 오직 TITLE 화면에서만 게임 종료. 다른 상태에서 ESC를 눌러도 무시.
>
> 또는 더 간단하게: ESC = 항상 TITLE로 복귀, TITLE에서 ESC = 게임 종료. 이게 더 자연스러움. 위 코드는 TITLE에서만 종료, 다른 상태는 ESC 무시 (혹은 PAUSE — 추후).

대안 단순 방식: `if (input_key(KEY_ESC) && g_state.gs == GS_TITLE) break;` — 그러면 main 루프 조건이 단순.

- [ ] **Step 2 (대안 권장 단순화 버전)**: `while (1)` + 명시적 break

위 복잡한 main loop 헤더 대신:

```c
while (1) {
    /* ... ESC 처리 안에서 분기 ... */

    if (input_key(KEY_ESC) && g_state.gs == GS_TITLE) break;
    /* (ESC in other states ignored) */

    /* ... 나머지 동일 ... */
}
```

`goto loop_exit` 는 제거하고 `break`로 단순화.

- [ ] **Step 3: 빌드 + 시각 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
```

Expected: 0 errors. GAME.CPP가 ~280줄 정도로 늘어남.

- [ ] **Step 4: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "GAME.CPP — full FSM (TITLE / INTRO / PLAYING / BOSS / CLEAR / OVER / CONTINUE)

State-driven main loop. Transitions:
  TITLE   --[Z]-->        STAGE_INTRO (1.5s)
  STAGE_INTRO --[end]-->  PLAYING
  PLAYING --[30s]-->      BOSS_INTRO (3s WARNING text)
  BOSS_INTRO --[end]-->   BOSS_FIGHT (boss spawns)
  BOSS_FIGHT --[dead]-->  STAGE_CLEAR (5s score overlay)
  STAGE_CLEAR --[end]-->  TITLE (Plan 8: only 1 stage)
  PLAYING/BOSS --[lives=0]--> GAME_OVER (2s)
  GAME_OVER --[end]-->    CONTINUE_PROMPT (10s)
  CONTINUE_PROMPT --[Z]--> PLAYING (score=0, lives=3, bombs=2,
                           stage time preserved, +invincibility)
  CONTINUE_PROMPT --[timeout]--> TITLE

ESC quits only from TITLE (ignored in gameplay states).
state_reset() now starts at GS_TITLE; state_reset_for_continue()
preserves stage progress while clearing player state."
```

---

## Task 3: 시각 검증

```bash
./run.sh
```

확인 사항:
- 게임 시작 시 **TITLE 화면** ("RAIDEN 2 CLONE / STAGE 1 DEMO / PRESS Z TO START / ESC TO QUIT")
- Z 누르면 **STAGE INTRO** ("STAGE 1 / DESERT / GET READY") 1.5초간
- 자동으로 **PLAYING** — 정상 게임플레이
- 30초 시점에 **WARNING** ("WARNING / BIG ENEMY / APPROACHING") 3초간
- 자동으로 **보스 등장**, 보스전 진행
- 보스 격파 시 **STAGE CLEAR** ("STAGE 1 CLEAR" + 점수 표시) 5초간
- 자동으로 TITLE로 돌아감
- 게임 중 죽으면 (잔기 0) **GAME OVER** 2초 → **CONTINUE 카운트다운 10초** ("CONTINUE / 9 / PRESS Z")
- 카운트 중 Z 누르면 같은 위치에서 재개 (점수 0, 잔기 3, 봄 2)
- 카운트 만료 시 TITLE로 복귀
- ESC는 TITLE에서만 종료

---

## Self-Review Checklist

- [ ] **Spec coverage** (M12 풀):
  - TITLE: ✓
  - STAGE_INTRO: ✓
  - BOSS_INTRO ("WARNING"): ✓
  - STAGE_CLEAR (점수 표시): ✓ (보너스 점수 카운트업은 단순화)
  - GAME_OVER: ✓
  - CONTINUE_PROMPT (10초 카운트다운): ✓
  - ENDING: deferred (3면 도달 시; Plan 8 미사용)

---

## Open Items (Plan 9+)

- 보너스 점수 카운트업 (BOMB BONUS / LIFE BONUS 등 spec section 5.5)
- 보스 인트로 화면 깜빡임 (빨간 화면 + WARNING 큰 글자)
- 1UP 임계값 도달 시 효과음 + 효과
- 진짜 STAGE 시스템 (시간축 스폰 스크립트 — 1면 콘텐츠를 정밀 정의)
- 무기 레벨 1~4
- 메달 시스템 + B/H/1UP 픽업
- 2면, 3면 + 보스
