# Raiden 2 Clone — Plan 5: HUD (점수/잔기/봄 표시)

> **For agentic workers:** Use superpowers:subagent-driven-development to execute. Steps use checkbox (`- [ ]`) syntax.

**Goal:** 좌측 32px HUD 영역에 점수(7자리), 잔기, 봄 보유 수를 hand-rolled 3x5 bitmap digit으로 표시. 신규 모듈 1개 (HUD).

**Architecture:** FONT 자산 파이프라인 우회 — 코드 내 `digit3x5[10][5]` 비트맵 테이블로 0~9 숫자만 렌더. 잔기/봄은 작은 아이콘 (gfx_fill_rect로 사각형) + 수치. HUD 함수는 매 프레임 마지막에 호출돼 어떤 게임 객체보다 위에 그려짐.

**Tech Stack:** Watcom C++, GFX 프리미티브 그대로 (gfx_fill_rect / gfx_pixel).

**Spec 참조:** `docs/superpowers/specs/2026-04-28-raiden2-clone-design.md` §3 (HUD 레이아웃)

---

## File Map

### NEW FILES

| 경로 | 책임 |
|---|---|
| `SRC/HUD.H` / `SRC/HUD.CPP` | digit 비트맵 + draw_digit/draw_number + hud_render |

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `SRC/GAME.CPP` | 메인 루프 끝에 hud_render() 호출 (gfx_flip 직전) |

### NOT TOUCHED

- 다른 모듈은 그대로
- 폰트 자산 파이프라인은 미사용 (Plan 5는 hand-rolled)
- 우측 HUD는 후속 plan에서 (메달/스테이지 표시 등)

---

## Task 1: SRC/HUD.H + HUD.CPP — 3x5 digit + 렌더 헬퍼

**Files:**
- Create: `SRC/HUD.H`, `SRC/HUD.CPP`

좌측 HUD (x=0..31, y=0..199) 안에 다음 표시:

```
y=4..8:    SCORE 7-digit (right-aligned at x=30)
y=12..16:  LIVES — ship icon (4x4) at x=4 + digit at x=12
y=20..24:  BOMBS — bomb icon (4x4) at x=4 + digit at x=12
```

3x5 digit + 1 px gap = 4 px per digit. 7 digits = 28 px (fits in 32 wide HUD with 2 px right margin and 2 px left padding to be precise — total used 28 px, so x=2 to x=30 — leaves 2 px margin on each side).

- [ ] **Step 1: HUD.H 작성**

```c
#ifndef HUD_H_INCLUDED
#define HUD_H_INCLUDED

#include "STATE.H"

/* Render the left-side HUD (x=0..31, y=0..199):
 *   - 7-digit score at the top
 *   - lives (ship icon + digit)
 *   - bombs (bomb icon + digit)
 *
 * Reads g_state.score / .lives / .bombs. Call once per frame, after
 * all gameplay rendering, before gfx_flip. */
void hud_render(void);

#endif
```

- [ ] **Step 2: HUD.CPP 작성**

```c
#include "HUD.H"
#include "GFX.H"

/* HUD layout constants (left-side HUD: x=0..31). */
#define HUD_W           32
#define HUD_BG_COLOR     0   /* black background — leaves it as gfx_clear set */
#define HUD_TEXT_COLOR  15   /* bright white from grayscale ramp */
#define HUD_LIFE_COLOR  12   /* bright cyan-ish (ship icon) */
#define HUD_BOMB_COLOR  25   /* bright red (bomb icon) */

/* 3x5 bitmap font for digits 0-9. Each row is a 3-bit mask:
 * bit 2 = leftmost pixel, bit 0 = rightmost. */
static const unsigned char digit3x5[10][5] = {
    /* 0 */ { 7, 5, 5, 5, 7 },
    /* 1 */ { 2, 6, 2, 2, 7 },
    /* 2 */ { 7, 1, 7, 4, 7 },
    /* 3 */ { 7, 1, 7, 1, 7 },
    /* 4 */ { 5, 5, 7, 1, 1 },
    /* 5 */ { 7, 4, 7, 1, 7 },
    /* 6 */ { 7, 4, 7, 5, 7 },
    /* 7 */ { 7, 1, 2, 4, 4 },
    /* 8 */ { 7, 5, 7, 5, 7 },
    /* 9 */ { 7, 5, 7, 1, 7 },
};

/* Draw a single digit at top-left (x, y). Each pixel is a 1x1 fill. */
static void draw_digit(int x, int y, int d, unsigned char color)
{
    int row, col;
    if (d < 0 || d > 9) return;
    for (row = 0; row < 5; row++) {
        unsigned char bits = digit3x5[d][row];
        for (col = 0; col < 3; col++) {
            if (bits & (4 >> col)) gfx_pixel(x + col, y + row, color);
        }
    }
}

/* Draw an unsigned integer right-aligned so its rightmost digit's right
 * edge is at x_right (exclusive). Pads with leading zeros up to digits.
 * Digit cell is 4 px (3 + 1 gap), 5 px tall. */
static void draw_number(int x_right, int y, u32 value, int digits,
                        unsigned char color)
{
    int i;
    int dx = x_right - 3;   /* leftmost-x of rightmost digit */
    for (i = 0; i < digits; i++) {
        int d = (int)(value % 10);
        value /= 10;
        draw_digit(dx, y, d, color);
        dx -= 4;            /* 3 wide + 1 gap */
    }
}

/* Tiny ship icon (4x4) — simple triangular silhouette. */
static void draw_ship_icon(int x, int y, unsigned char color)
{
    /*  .X..      (row 0)
     *  XXX.      (row 1)
     *  X.X.      (row 2)
     *  X.X.      (row 3)
     */
    gfx_pixel(x + 1, y + 0, color);
    gfx_pixel(x + 0, y + 1, color);
    gfx_pixel(x + 1, y + 1, color);
    gfx_pixel(x + 2, y + 1, color);
    gfx_pixel(x + 0, y + 2, color);
    gfx_pixel(x + 2, y + 2, color);
    gfx_pixel(x + 0, y + 3, color);
    gfx_pixel(x + 2, y + 3, color);
}

/* Tiny bomb icon (4x4) — a filled diamond. */
static void draw_bomb_icon(int x, int y, unsigned char color)
{
    /*  .X..
     *  XXX.
     *  XXX.
     *  .X..
     */
    gfx_pixel(x + 1, y + 0, color);
    gfx_pixel(x + 0, y + 1, color);
    gfx_pixel(x + 1, y + 1, color);
    gfx_pixel(x + 2, y + 1, color);
    gfx_pixel(x + 0, y + 2, color);
    gfx_pixel(x + 1, y + 2, color);
    gfx_pixel(x + 2, y + 2, color);
    gfx_pixel(x + 1, y + 3, color);
}

void hud_render(void)
{
    /* Score row: 7 digits, right edge at x=30 (HUD_W - 2). y=4..8. */
    draw_number(30, 4, g_state.score, 7, HUD_TEXT_COLOR);

    /* Lives row: ship icon at x=4 + digit at x=12. y=12..16. */
    draw_ship_icon(4, 12, HUD_LIFE_COLOR);
    draw_digit(12, 12, (int)g_state.lives, HUD_TEXT_COLOR);

    /* Bombs row: bomb icon at x=4 + digit at x=12. y=20..24. */
    draw_bomb_icon(4, 20, HUD_BOMB_COLOR);
    draw_digit(12, 20, (int)g_state.bombs, HUD_TEXT_COLOR);
}
```

> **참고**: `gfx_pixel` API는 GFX.H에 이미 존재 (`void gfx_pixel(int x, int y, unsigned char color);`).
>
> 색 인덱스 (HUD_TEXT_COLOR=15, HUD_LIFE_COLOR=12, HUD_BOMB_COLOR=25)는 PALETTE.H의 grayscale + color ramps 추정. 실제 색이 어색하면 조정 가능.

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
ls -l SRC/GAME.EXE
```

Expected: 0 errors. HUD.CPP가 새로 컴파일됨 (총 18 → 19 modules).

- [ ] **Step 4: 커밋**

```bash
git add SRC/HUD.H SRC/HUD.CPP
git commit -m "Add HUD module — 3x5 bitmap digits, score/lives/bombs

Hand-rolled to avoid the FONT module's asset dependency
(ENG_HGDIARY.FNT etc.). digit3x5[10][5] table covers 0-9 in 3 px
wide x 5 px tall. draw_digit/draw_number/draw_ship_icon/draw_bomb_icon
helpers compose the left-side 32px HUD layout:

  y=4..8 :  7-digit score (right-aligned at x=30, leading zeros)
  y=12..16: ship icon + lives digit
  y=20..24: bomb icon + bombs digit

Reads g_state.score/.lives/.bombs. hud_render is called by GAME.CPP
in Task 2."
```

---

## Task 2: GAME.CPP — hud_render 호출

**Files:**
- Modify: `SRC/GAME.CPP`

매 프레임 모든 게임 렌더 다음, gfx_flip 직전에 hud_render() 호출.

- [ ] **Step 1: 새 include 추가**

`SRC/GAME.CPP`의 includes에 추가 (다른 게임 모듈 옆):

```c
#include "HUD.H"
```

- [ ] **Step 2: 렌더 순서에 hud_render 추가**

기존 렌더 블록 (effect_render 다음):

```c
        gfx_clear(0);
        bg_render();
        enemy_render_ground();
        bullet_render_player();
        enemy_render_air();
        player_render();
        bullet_render_enemy();
        effect_render();
```

다음으로 변경 (effect_render 다음에 hud_render 추가):

```c
        gfx_clear(0);
        bg_render();
        enemy_render_ground();
        bullet_render_player();
        enemy_render_air();
        player_render();
        bullet_render_enemy();
        effect_render();
        hud_render();              /* NEW: HUD on top of everything */
```

> **참고**: HUD는 좌측 32px 영역 (x=0..31)에만 그리므로 게임 영역 (x=32..287)과 겹치지 않음. 그래도 렌더 순서상 가장 마지막이 안전 (만약 향후 HUD가 풀 너비로 확장되어도).

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
ls -l SRC/GAME.EXE
```

Expected: 0 errors.

- [ ] **Step 4: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "Wire hud_render into main loop

Called after effect_render and before gfx_flip — HUD draws on top
of all gameplay. Plan 5 only paints the left 32px column; the right
column is reserved for medal/stage display in future plans."
```

---

## Task 3: 시각 검증

- [ ] **Step 1: 사용자 검증**

```bash
./run.sh
```

확인 사항:
- 좌측 32px HUD에 다음 표시:
  - **상단**: 7자리 점수 (0000000으로 시작, 적 격파 시 0000100, 0000200, ... 으로 증가)
  - **중간**: 작은 ship 아이콘 + 잔기 수 (시작 3)
  - **하단**: 작은 bomb 아이콘 + 봄 수 (시작 2)
- 적 격파 시 점수가 +100 증가하는 게 시각적으로 보임
- 사망/오토봄 시 잔기/봄 수가 줄어드는 게 보임
- 봄 발동 시 봄 수가 1 줄어드는 게 보임
- 게임 영역 (x=32..287)은 그대로 적 + 플레이어 정상 동작
- ESC 정상 종료

- [ ] **Step 2: git log 점검**

```bash
git log --oneline -5
```

Expected: Plan 5의 2개 커밋이 main 위에.

---

## Self-Review Checklist

- [ ] **Spec coverage**:
  - 점수 7자리: ✓
  - 잔기 표시: ✓ (아이콘 + 숫자)
  - 봄 표시: ✓
  - 무기명/레벨 표시: ✗ (Plan 6 무기 다양화와 함께)
  - 메달 가치 표시: ✗ (Plan 6 메달 시스템과 함께)
  - 스테이지 표시: ✗ (Plan 7 STAGE 시스템과 함께)
- [ ] **함수 시그니처 매칭**:
  - hud_render() → void
  - 내부 헬퍼는 모두 static
- [ ] **빌드 시스템**: HUD.CPP 자동 컴파일
- [ ] **렌더 순서**: 마지막 호출, gfx_flip 직전

---

## Open Items (Plan 6 인계)

- 우측 HUD 활용 (메달 가치, 스테이지 표시)
- 무기 종류/레벨 표시
- 콤마 자릿수 구분 (1,234,567 같은 형식 — 가독성)
- 1UP 임계값 도달 시 깜빡임 강조
- "1UP" / "BOMB" / "LIVES" 같은 라벨 텍스트 (3x5 폰트에 알파벳 추가 또는 폰트 모듈 도입)
