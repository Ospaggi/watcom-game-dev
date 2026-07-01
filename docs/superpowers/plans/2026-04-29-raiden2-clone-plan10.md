# Raiden 2 Clone — Plan 10: 패럴랙스 클라우드

> **For agentic workers:** Use superpowers:subagent-driven-development. Steps use `- [ ]` checkbox.

**Goal:** 사막 배경 위에 반투명 구름 레이어 추가. 다른 속도로 스크롤되어 깊이감 생김 (구름이 더 가까워서 더 빨리 스크롤).

**Architecture:** 신규 모듈 0. IMG 모듈에 `img_blit_vscroll_trans()` 추가 (투명 색 인덱스 처리), BG 모듈이 두 레이어 (desert + clouds) 로드+렌더, convert.sh에서 clouds-transparent.png 변환.

**Spec 참조:** Plan 1 spec section 1-B (배경 자산), Plan 1 보류 항목 "패럴랙스 클라우드 — Plan 2에서 추가"

---

## File Map

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `convert.sh` | `clouds-transparent.png` → `IMG_DCLD.IMG` 변환 |
| `SRC/IMG.H` / `SRC/IMG.CPP` | 신규 함수 `img_blit_vscroll_trans()` (투명 색 0 처리) |
| `SRC/BG.H` / `SRC/BG.CPP` | 두 번째 Img (clouds) 로드 + 렌더, 다른 스크롤 속도 |

### GENERATED FILES

| 경로 | 출처 |
|---|---|
| `SRC/IMG_DCLD.IMG` | `clouds-transparent.png` (256x103) — 투명 알파 픽셀은 인덱스 0 |

---

## Task 1: clouds-transparent.png → SRC/IMG_DCLD.IMG

**Files:**
- Modify: `convert.sh`

**Step 1: convert.sh 갱신**

기존 desert 변환 다음에:

```bash
echo "Converting cloud layer..."
bun tools/mkimg.ts \
  ASSETS/raw/ansimuz_spaceship/extracted/Spaceship-shooter-gamekit/Assets/Desert/backgrounds/clouds-transparent.png \
  SRC/IMG_DCLD.IMG
```

**Step 2: 실행 확인**

```bash
./convert.sh
ls -lh SRC/IMG_DCLD.IMG
```

Expected: ~26 KB (256×103 + 4 헤더).

**Step 3: 커밋**

```bash
git add convert.sh
git commit -m "Convert clouds-transparent.png to IMG_DCLD.IMG

Cloud layer for parallax effect. 256x103, alpha pixels mapped to
palette index 0 (transparent) by mkimg. Used by BG module's second
scrolling layer in Plan 10."
```

---

## Task 2: IMG 모듈 — img_blit_vscroll_trans

**Files:**
- Modify: `SRC/IMG.H`, `SRC/IMG.CPP`

기존 `img_blit_vscroll`은 memcpy로 opaque 복사. 투명 픽셀(idx=0)은 무시하는 변형 추가.

**Step 1: IMG.H에 새 함수 선언 추가**

기존 `img_blit_vscroll` 다음:

```c
/* Same as img_blit_vscroll but treats palette index 0 as transparent
 * — pixels with index 0 are not written. Slower than the opaque path
 * (per-pixel branch) but preserves what's already in the back-buffer. */
void img_blit_vscroll_trans(const Img *im, int dx0, int dx1,
                            int dy0, int dy1, int src_y);
```

**Step 2: IMG.CPP에 구현 추가**

파일 끝에:

```c
void img_blit_vscroll_trans(const Img *im, int dx0, int dx1,
                            int dy0, int dy1, int src_y) {
    unsigned char *dst, *src_row;
    int y, sy, run, x;

    if (!im || !im->pix) return;
    if (dy0 < 0) dy0 = 0;
    if (dy1 > GFX_H) dy1 = GFX_H;
    if (dx0 < 0) dx0 = 0;
    if (dx1 > GFX_W) dx1 = GFX_W;
    if (dx1 - dx0 > im->w) dx1 = dx0 + im->w;
    if (dy1 <= dy0 || dx1 <= dx0) return;

    run = dx1 - dx0;
    sy  = src_y % im->h;
    if (sy < 0) sy += im->h;

    dst = gfx_buffer() + (unsigned long)dy0 * GFX_W + dx0;
    for (y = dy0; y < dy1; y++, dst += GFX_W) {
        src_row = im->pix + (unsigned long)sy * im->w;
        for (x = 0; x < run; x++) {
            unsigned char p = src_row[x];
            if (p) dst[x] = p;
        }
        sy++;
        if (sy >= im->h) sy = 0;
    }
}
```

**Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -5
```

**Step 4: 커밋**

```bash
git add SRC/IMG.H SRC/IMG.CPP
git commit -m "Add img_blit_vscroll_trans for transparent layered scroll

Identical clipping/wrap semantics as img_blit_vscroll, but skips
pixels with palette index 0 (transparent). Used by BG.CPP's cloud
parallax layer on top of the desert background."
```

---

## Task 3: BG 모듈 — clouds 레이어 + 다른 속도

**Files:**
- Modify: `SRC/BG.H`, `SRC/BG.CPP`

기존 BG는 desert 단일 레이어. clouds 레이어 추가. 클라우드는 더 빠르게 스크롤 (90 px/sec, desert 60 px/sec).

**Step 1: BG.CPP 수정**

```c
#include <stdlib.h>
#include "BG.H"
#include "GFX.H"
#include "IMG.H"

#define PLAY_X0    32
#define PLAY_X1    288
#define PLAY_Y0    0
#define PLAY_Y1    200
#define BG_SPEED_PX_SEC      60
#define CLOUDS_SPEED_PX_SEC  90

static Img      g_bg;
static Img      g_clouds;
static u32      g_bg_scroll_q16;
static u32      g_cloud_scroll_q16;

int bg_init(void)
{
    g_bg_scroll_q16 = 0;
    g_cloud_scroll_q16 = 0;
    if (img_load("IMG_DSRT.IMG", &g_bg) != IMG_OK) return -1;
    if (img_load("IMG_DCLD.IMG", &g_clouds) != IMG_OK) {
        img_free(&g_bg);
        return -2;
    }
    return 0;
}

void bg_close(void)
{
    img_free(&g_clouds);
    img_free(&g_bg);
}

static void scroll_advance(u32 *q16, int speed_px_sec, int img_h, u32 dt_ms)
{
    u32 inc = (u32)speed_px_sec * 65536UL * dt_ms / 1000UL;
    u32 wrap = (u32)img_h * 65536UL;
    inc %= wrap;
    if (*q16 >= inc) *q16 -= inc;
    else             *q16 = wrap - (inc - *q16);
}

void bg_update(u32 dt_ms)
{
    scroll_advance(&g_bg_scroll_q16,    BG_SPEED_PX_SEC,    g_bg.h,    dt_ms);
    scroll_advance(&g_cloud_scroll_q16, CLOUDS_SPEED_PX_SEC, g_clouds.h, dt_ms);
}

void bg_render(void)
{
    int bg_sy = (int)(g_bg_scroll_q16 >> 16);
    img_blit_vscroll(&g_bg, PLAY_X0, PLAY_X1, PLAY_Y0, PLAY_Y1, bg_sy);

    int cl_sy = (int)(g_cloud_scroll_q16 >> 16);
    img_blit_vscroll_trans(&g_clouds, PLAY_X0, PLAY_X1, PLAY_Y0, PLAY_Y1, cl_sy);
}
```

> **참고**: `BG_SPEED_MS`를 `BG_SPEED_PX_SEC`로 이름 정확화 (실제로 px/sec).

**Step 2: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -5
ls -l SRC/GAME.EXE
```

**Step 3: 커밋**

```bash
git add SRC/BG.CPP
git commit -m "BG — add cloud parallax layer (transparent, faster scroll)

IMG_DCLD.IMG (256x103, transparent index 0) loaded alongside the
desert background. Renders after the desert via the new
img_blit_vscroll_trans, at 90 px/sec (vs 60 px/sec for desert) so
clouds appear closer to the camera and visibly slide faster.

Uses a shared scroll_advance helper for both layers to avoid the
duplicated subpixel q16 wrap math."
```

---

## Task 4: 시각 검증

```bash
./run.sh
```

확인 사항:
- 배경에 사막 + **반투명 구름 레이어** 둘 다 보임
- 구름이 사막보다 **더 빨리** 흘러감 (시각적 깊이)
- 구름의 "투명 영역"으로 사막이 비쳐 보임 (검은색 패치 X, 진짜 투명)
- 게임 동작 (적/탄/플레이어) 이전과 동일

---

## Self-Review Checklist

- [ ] **Spec coverage**: §1-B clouds 레이어 사용
- [ ] 투명도 작동 확인
- [ ] 두 레이어 다른 속도

---

## Open Items (Plan 11+)

- 메달 시스템 + B/H/1UP 픽업
- 2면, 3면 + 보스
- 실제 STAGE 시스템 (시간축 스크립트)
- Boss 헤드 회전, BOSS_INTRO 화면 깜빡임
