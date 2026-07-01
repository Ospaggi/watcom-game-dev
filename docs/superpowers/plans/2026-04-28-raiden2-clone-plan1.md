# Raiden 2 Clone — Plan 1: Vertical Slice 1 (자산 + 플레이어 + Vulcan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Mode 13h 화면에서 사막 배경이 세로 스크롤되는 가운데 플레이어 우주선이 8방향 이동하고 Z를 누르면 Vulcan 탄을 발사한다. 적/충돌/아이템/봄은 후속 plan.

**Architecture:** 신규 5개 게임 모듈(STATE.H, BG, PLAYER, WEAPON, BULLET) 추가, 기존 GAME.CPP는 `_workspace/`로 백업 후 신규 작성. 자산 파이프라인 확장 (mksprite `--bin --grid`로 SPR 생성, mkimg로 IMG 생성, convert.sh 갱신).

**Tech Stack:** Watcom C++ (DOS 32-bit flat, WCL386), TypeScript+Bun (호스트 자산 변환), DOSBox.

**Spec 참조:** `docs/superpowers/specs/2026-04-28-raiden2-clone-design.md`

---

## File Map

### NEW FILES

| 경로 | 책임 |
|---|---|
| `tools/inspect_sheet.ts` | PNG 시트의 프레임 분할 시각 검사용 (디버그 헬퍼) |
| `SRC/STATE.H` | 전역 게임 상태 구조체 + 타입 정의 (`GameState`, `GameStateId`) |
| `SRC/BG.H` / `SRC/BG.CPP` | 배경 수직 스크롤 (`bg_init`, `bg_update`, `bg_render`) |
| `SRC/PLAYER.H` / `SRC/PLAYER.CPP` | 플레이어 우주선 이동 + 애니메이션 + 발사 트리거 |
| `SRC/WEAPON.H` / `SRC/WEAPON.CPP` | Vulcan L1 발사 함수 (`weapon_fire_vulcan_l1`) |
| `SRC/BULLET.H` / `SRC/BULLET.CPP` | 플레이어 탄 풀 (64 슬롯) + 업데이트/렌더 |

### GENERATED FILES (자산 파이프라인 출력)

| 경로 | 출처 |
|---|---|
| `SRC/SPR_PSHP.SPR` | `ASSETS/raw/ansimuz_spaceship/.../ship.png` (80x48, 5x2 grid, 16x24 frames) |
| `SRC/IMG_DSRT.IMG` | `ASSETS/raw/ansimuz_spaceship/.../desert-backgorund.png` (256x272) |

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `SRC/GAME.CPP` | 완전 재작성 (기존 별/미사일 데모는 `_workspace/GAME_OLD.CPP`로 백업) |
| `convert.sh` | mksprite + mkimg 호출 추가 |
| `.gitignore` | `_workspace/` 이미 등록되어 있음, 추가 변경 불필요 |

### NOT TOUCHED (이 plan에서)

- 엔진 모듈 전체 (`GFX/INPUT/TIMER/SOUND/SFX/SPRITE/IMG/FONT.CPP/H`) — 수정 없음
- 패럴랙스 클라우드 — Plan 2에서 추가 (Plan 1은 desert 단일 레이어만)
- 적/탄막/아이템/봄/HUD — 후속 plans

---

## Task 1: 워크스페이스 셋업 + 기존 GAME.CPP 백업

**Files:**
- Create: `_workspace/GAME_OLD.CPP` (백업용, 빌드에서 제외됨)
- Modify: `SRC/GAME.CPP` (제거, Task 7에서 신규 작성)

기존 `SRC/GAME.CPP`는 별/미사일 데모. 새 게임으로 대체하기 전 백업한다. `_workspace/`는 `.gitignore`에 이미 등록되어 있어 빌드(`WCL386 *.CPP`)에 포함되지 않는다.

- [ ] **Step 1: 기존 GAME.CPP를 백업으로 이동**

```bash
mkdir -p _workspace
mv SRC/GAME.CPP _workspace/GAME_OLD.CPP
```

- [ ] **Step 2: 백업 확인**

```bash
ls _workspace/GAME_OLD.CPP
ls SRC/*.CPP | wc -l   # 기존 10개에서 GAME.CPP 빠져 9개
```

Expected: `_workspace/GAME_OLD.CPP` 존재 (.gitignore에 등록되어 있어 git 추적 X), `SRC/*.CPP`는 9개 (FONT/GFX/IMG/INPUT/SCRNCAP/SFX/SOUND/SPRITE/TIMER).

> **참고**: 이 시점에 빌드 시도 시 `main` 함수가 없어 link 에러 — 정상이다. Task 7에서 새 GAME.CPP 작성 후 빌드 가능.

- [ ] **Step 3: 커밋 (백업은 git 미포함, 히스토리로만 보존)**

```bash
git rm SRC/GAME.CPP
git commit -m "Remove old smoke-test GAME.CPP before Raiden 2 clone rewrite

The previous GAME.CPP was a parallax + missile demo using only GFX
primitives. We are replacing it with a new modular game. Local
backup in _workspace/GAME_OLD.CPP (gitignored — not committed).
Original is preserved in git history (commits 528910f and 58fe144)
for reference."
```

---

## Task 2: ship.png 프레임 분할 검증 헬퍼 (`tools/inspect_sheet.ts`)

**Files:**
- Create: `tools/inspect_sheet.ts`

향후 다른 스프라이트 시트 처리 시 재사용할 수 있는 디버그 헬퍼. PNG의 알파 채널 프로젝션을 ASCII로 출력해 빈 행/열을 식별한다.

- [ ] **Step 1: 헬퍼 스크립트 작성**

`tools/inspect_sheet.ts`:

```typescript
#!/usr/bin/env bun
/**
 * inspect_sheet.ts - Visualize alpha channel of a PNG to spot frame boundaries.
 *
 * Prints a per-column "non-transparent pixel count" bar, plus an ASCII
 * mini-map of the image (one char per pixel, scaled).
 *
 * Usage: bun tools/inspect_sheet.ts <image.png>
 */
import { readFileSync } from "fs";
import { inflateSync } from "zlib";

function readPNG(path: string): { w: number; h: number; rgba: Uint8Array } {
  const buf = readFileSync(path);
  const w = (buf[16]<<24)|(buf[17]<<16)|(buf[18]<<8)|buf[19];
  const h = (buf[20]<<24)|(buf[21]<<16)|(buf[22]<<8)|buf[23];
  const colorType = buf[25];
  if (buf[24] !== 8 || colorType !== 6) {
    console.error(`only 8-bit RGBA supported (got bit=${buf[24]} type=${colorType})`);
    process.exit(1);
  }

  const idatChunks: Buffer[] = [];
  let off = 8;
  while (off < buf.length) {
    const len = (buf[off]<<24)|(buf[off+1]<<16)|(buf[off+2]<<8)|buf[off+3];
    const type = String.fromCharCode(buf[off+4], buf[off+5], buf[off+6], buf[off+7]);
    if (type === "IDAT") idatChunks.push(buf.slice(off+8, off+8+len));
    if (type === "IEND") break;
    off += 12 + len;
  }
  const data = inflateSync(Buffer.concat(idatChunks));

  const rgba = new Uint8Array(w * h * 4);
  let di = 0, si = 0;
  let prior = new Uint8Array(w * 4);
  for (let y = 0; y < h; y++) {
    const filter = data[si++];
    const row = data.slice(si, si + w * 4);
    si += w * 4;
    const out = new Uint8Array(row);
    for (let x = 0; x < w * 4; x++) {
      const a = x >= 4 ? out[x-4] : 0;
      const b = prior[x];
      const c = x >= 4 ? prior[x-4] : 0;
      switch (filter) {
        case 0: break;
        case 1: out[x] = (out[x] + a) & 0xFF; break;
        case 2: out[x] = (out[x] + b) & 0xFF; break;
        case 3: out[x] = (out[x] + ((a + b) >> 1)) & 0xFF; break;
        case 4: {
          const p = a + b - c;
          const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
          const pred = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
          out[x] = (out[x] + pred) & 0xFF;
          break;
        }
      }
    }
    rgba.set(out, di);
    di += w * 4;
    prior = out;
  }
  return { w, h, rgba };
}

const path = Bun.argv[2];
if (!path) { console.error("usage: bun inspect_sheet.ts <image.png>"); process.exit(1); }

const { w, h, rgba } = readPNG(path);
console.log(`${path}: ${w}x${h}`);

console.log("\nColumn alpha density (chars = non-transparent pixel count, 0=empty):");
let header = "  ";
for (let x = 0; x < w; x++) header += (x % 10 === 0) ? Math.floor(x/10) : " ";
console.log(header);
let sub = "  ";
for (let x = 0; x < w; x++) sub += (x % 10).toString();
console.log(sub);

let bar = "  ";
for (let x = 0; x < w; x++) {
  let count = 0;
  for (let y = 0; y < h; y++) if (rgba[(y*w+x)*4 + 3] >= 128) count++;
  bar += count === 0 ? "." : count < 5 ? "·" : count < 12 ? ":" : count < 20 ? "+" : "#";
}
console.log(bar);

console.log("\nMini-map (one char per pixel, '.' transparent, '#' opaque):");
for (let y = 0; y < h; y++) {
  let row = (y % 10 === 0 ? y.toString().padStart(3) + " " : "    ");
  for (let x = 0; x < w; x++) {
    row += rgba[(y*w+x)*4 + 3] >= 128 ? "#" : ".";
  }
  console.log(row);
}
```

- [ ] **Step 2: 실행해서 ship.png 프레임 경계 확인**

```bash
bun tools/inspect_sheet.ts ASSETS/raw/ansimuz_spaceship/extracted/Spaceship-shooter-gamekit/Assets/spritesheets/ship.png
```

Expected: 80x80 미니맵에서 5×2 그리드 보임 — 컬럼 0/2/4 (16/16/16 px) 영역에 우주선 픽셀, 컬럼 1/3 (16/16 px) 영역은 모두 `.` (빈 영역). 행은 0~24, 24~48 두 줄로 나뉨.

이 결과는 Godot 프로젝트의 import 메타데이터(이미 확인 완료: `region = Rect2(0,0,16,24)` ~ `Rect2(64,24,16,24)`)와 일치해야 함. 프레임 16x24, 5x2 grid. 컬럼 1, 3은 빈 프레임.

- [ ] **Step 3: 커밋**

```bash
git add tools/inspect_sheet.ts
git commit -m "Add inspect_sheet.ts helper for PNG frame boundary inspection

Prints column alpha density bar + ASCII mini-map. Used to verify frame
layouts before mksprite --grid conversion. ship.png confirmed as 16x24
frames in 5x2 grid (columns 1,3 are intentionally empty)."
```

---

## Task 3: ship.png → SPR_PSHP.SPR 변환

**Files:**
- Generated: `SRC/SPR_PSHP.SPR`
- Modify: `convert.sh` (mksprite 호출 추가)

5x2 그리드로 변환. 결과 .SPR에는 10개 프레임이 들어가지만 인덱스 0/2/4/5/7/9만 실제 사용 (1/3/6/8은 비어있는 프레임 = 컴파일된 빈 코드, 약간의 메모리 낭비지만 단순함).

프레임 인덱스 매핑 (PLAYER.H에서 정의):
- frame 0 = LEFT_0 (왼쪽 기울기, 애니 0)
- frame 2 = IDLE_0 (정면, 애니 0)
- frame 4 = RIGHT_0 (오른쪽 기울기, 애니 0)
- frame 5 = LEFT_1 (왼쪽 기울기, 애니 1)
- frame 7 = IDLE_1
- frame 9 = RIGHT_1

- [ ] **Step 1: 수동으로 mksprite 한번 실행해서 출력 확인**

```bash
cd /Users/gcjjyy/lab/watcom-game-dev
bun tools/mksprite.ts --bin --grid 5x2 \
  ASSETS/raw/ansimuz_spaceship/extracted/Spaceship-shooter-gamekit/Assets/spritesheets/ship.png \
  SPR_PSHP
```

Expected stderr: `/* SRC/SPR_PSHP.SPR: 80x48 sheet, 5x2 grid, 10 frames of 16x24 */`
Expected: `SRC/SPR_PSHP.SPR` 파일 생성, 크기 수십~수백 바이트 (컴파일된 머신코드).

- [ ] **Step 2: SPR 파일 생성 확인**

```bash
ls -lh SRC/SPR_PSHP.SPR
file SRC/SPR_PSHP.SPR
```

Expected: 파일 존재, 100~3000 바이트 정도 (정확한 크기는 투명 픽셀 비율에 따라).

- [ ] **Step 3: convert.sh에 mksprite 호출 추가**

`convert.sh` 수정 — 기존 "Done." 직전에 새 단계 추가:

```bash
#!/bin/bash
# Engine asset pipeline. Generates PALETTE.H and nothing else.
# Extend with mksprite/mkimg/mkfont/mksfx calls as assets are added.

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

set -e

echo "Generating palette..."
bun tools/mkpalette.ts

echo "Converting SFX..."
bun tools/mksfx.ts "/Users/gcjjyy/Documents/게임개발/fx_sounds/ui-sound4.ogg" SRC/FIRE.SFX

echo "Copying VGM..."
cp "/Users/gcjjyy/lab/oscc/imsplay/public/18 Tyrian, The Level.vgm" SRC/TYRIAN.VGM

echo "Converting player ship sprite..."
bun tools/mksprite.ts --bin --grid 5x2 \
  ASSETS/raw/ansimuz_spaceship/extracted/Spaceship-shooter-gamekit/Assets/spritesheets/ship.png \
  SPR_PSHP

echo "Done."
```

- [ ] **Step 4: convert.sh 재실행해서 모든 자산 변환 OK 확인**

```bash
./convert.sh
```

Expected: 4단계 모두 에러 없이 완료, "Done." 출력.

```bash
ls SRC/PALETTE.H SRC/FIRE.SFX SRC/TYRIAN.VGM SRC/SPR_PSHP.SPR
```

Expected: 4개 파일 모두 존재.

- [ ] **Step 5: 커밋**

```bash
git add convert.sh
git commit -m "Convert player ship PNG to compiled SPR

Add mksprite step for ship.png (80x48 5x2 grid, 16x24 frames). Frames
indexed 0,2,4 (anim 0) and 5,7,9 (anim 1) for LEFT/IDLE/RIGHT poses.
Empty frames 1,3,6,8 (sheet has gap columns) are minimal-cost no-ops."
```

> SRC/SPR_PSHP.SPR 자체는 .gitignore에 추가하거나 빌드 산출물로 취급. 현재 .gitignore에는 SRC/FONT.BIN 등 생성 파일이 포함되어 있으므로 SPR도 추가하자.

- [ ] **Step 6: .gitignore에 생성 SPR/IMG 패턴 추가**

`.gitignore`에 다음 줄 추가 (적절한 위치):

```
SRC/*.SPR
SRC/*.IMG
```

```bash
git add .gitignore
git commit -m "Gitignore generated SPR and IMG asset files in SRC/

These are build outputs from convert.sh (mksprite/mkimg). Source PNGs
live in ASSETS/."
```

---

## Task 4: desert 배경 → SRC/IMG_DSRT.IMG

**Files:**
- Generated: `SRC/IMG_DSRT.IMG`
- Modify: `convert.sh`

배경은 256x272로 화면 폭 256과 일치. `mkimg.ts` 가변 크기 모드 (4바이트 헤더 + 픽셀)로 변환. `Img` 구조체로 로드.

- [ ] **Step 1: mkimg 한 번 수동 실행 확인**

```bash
bun tools/mkimg.ts \
  ASSETS/raw/ansimuz_spaceship/extracted/Spaceship-shooter-gamekit/Assets/Desert/backgrounds/desert-backgorund.png \
  SRC/IMG_DSRT.IMG
```

Expected: `SRC/IMG_DSRT.IMG` 생성, 크기 = 4 (헤더) + 256×272 (픽셀) = **69,636 바이트**.

- [ ] **Step 2: 결과 확인**

```bash
ls -l SRC/IMG_DSRT.IMG  # 69636 바이트 정확
```

- [ ] **Step 3: convert.sh에 mkimg 추가**

`convert.sh`에서 mksprite 단계 다음에 추가:

```bash
echo "Converting desert background..."
bun tools/mkimg.ts \
  ASSETS/raw/ansimuz_spaceship/extracted/Spaceship-shooter-gamekit/Assets/Desert/backgrounds/desert-backgorund.png \
  SRC/IMG_DSRT.IMG
```

- [ ] **Step 4: 다시 ./convert.sh 실행 확인**

```bash
./convert.sh
ls SRC/IMG_DSRT.IMG
```

Expected: 파일 존재, 69636 바이트.

- [ ] **Step 5: 커밋**

```bash
git add convert.sh
git commit -m "Convert desert background PNG to raw indexed IMG

256x272 desert-backgorund.png → SRC/IMG_DSRT.IMG (4-byte header +
69632 bytes pixel data). Loaded via img_load() at runtime, scrolled
vertically with img_blit_vscroll()."
```

---

## Task 5: SRC/STATE.H — 게임 상태 + 타입 정의

**Files:**
- Create: `SRC/STATE.H`

전역 상태 컨테이너. 이 plan에서는 score/lives/weapon만 사용하지만 spec에 정의된 전체 필드를 미리 둠 (후속 plan에서 채움). `extern GameState g_state;` 선언만 — 실제 정의는 GAME.CPP에서.

- [ ] **Step 1: SRC/STATE.H 작성**

```c
#ifndef STATE_H_INCLUDED
#define STATE_H_INCLUDED

/* Common integer typedefs used by the game modules.
 * Watcom 32-bit flat: int = 32-bit. */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;
typedef signed char    i8;
typedef signed short   i16;
typedef signed long    i32;

/* Power-up weapon kinds (Vulcan/Laser/Plasma). */
typedef enum {
    PW_VULCAN = 0,
    PW_LASER  = 1,
    PW_PLASMA = 2
} PowerWeapon;

/* High-level game state machine. Plan 1 only uses GS_PLAYING. */
typedef enum {
    GS_TITLE = 0,
    GS_STAGE_INTRO,
    GS_PLAYING,
    GS_BOSS_INTRO,
    GS_BOSS_FIGHT,
    GS_STAGE_CLEAR,
    GS_GAME_OVER,
    GS_CONTINUE_PROMPT,
    GS_ENDING
} GameStateId;

/* Global game state. Single instance (g_state) lives in GAME.CPP. */
typedef struct {
    u32 score;
    u32 next_extend;        /* next 1UP threshold (default 1,000,000) */
    u8  lives;              /* starting 3 */
    u8  bombs;              /* starting 2, cap 7 */
    u8  weapon;             /* PowerWeapon */
    u8  weapon_level;       /* 1~4 */
    u8  medal_level;        /* 0~6 (index into medal_value_table) */
    u8  invincible_frames;  /* nonzero = invincible (decremented per frame) */
    u8  stage;              /* 1~3 */
    u8  continues_used;
    u32 stage_t_ms;         /* ms since stage start (drives spawn script) */
    GameStateId gs;
    u32 gs_t_ms;            /* ms since current GS entered */
} GameState;

extern GameState g_state;

#endif
```

- [ ] **Step 2: 컴파일 가능한지 헤더 단독 검증 (옵션 — Watcom 빌드는 Task 7 후 가능)**

이 시점엔 아직 build 못 함 (main 없음). syntax는 다음 task에서 사용시 자동 검증.

- [ ] **Step 3: 커밋**

```bash
git add SRC/STATE.H
git commit -m "Add STATE.H — global game state and shared typedefs

Defines u8/u16/u32/i16 etc for use across game modules, the
PowerWeapon enum (Vulcan/Laser/Plasma), GameStateId enum (full FSM
from spec, mostly unused in Plan 1), and the GameState struct holding
score/lives/bombs/weapon/medal/etc. The single g_state instance is
defined in GAME.CPP."
```

---

## Task 6: SRC/BG.H + BG.CPP — 배경 수직 스크롤

**Files:**
- Create: `SRC/BG.H`, `SRC/BG.CPP`

`Img` 로드 → `img_blit_vscroll`로 매 프레임 스크롤. 플레이 영역은 화면 좌표 (32, 0) ~ (288, 200) (256x200).

스크롤 속도: 1 px/frame (~ 60 px/sec at 60 fps).

- [ ] **Step 1: BG.H 작성**

`SRC/BG.H`:

```c
#ifndef BG_H_INCLUDED
#define BG_H_INCLUDED

#include "STATE.H"

/* Initialize: load IMG_DSRT.IMG, reset scroll offset.
 * Returns 0 on success, nonzero on failure. */
int  bg_init(void);

/* Free loaded resources. */
void bg_close(void);

/* Advance scroll position by `dt_ms` (ms since last frame). */
void bg_update(u32 dt_ms);

/* Render current scroll state into the back-buffer.
 * Always covers the play area (x=32..288, y=0..200). */
void bg_render(void);

#endif
```

- [ ] **Step 2: BG.CPP 작성**

`SRC/BG.CPP`:

```c
#include <stdlib.h>
#include "BG.H"
#include "GFX.H"
#include "IMG.H"

#define PLAY_X0    32
#define PLAY_X1    288
#define PLAY_Y0    0
#define PLAY_Y1    200
#define BG_SPEED_MS  60   /* pixels per second of vertical scroll */

static Img      g_bg;
static u32      g_scroll_q16;   /* current scroll y in 1/65536 px (subpixel) */

int bg_init(void)
{
    g_scroll_q16 = 0;
    if (img_load("IMG_DSRT.IMG", &g_bg) != IMG_OK) return -1;
    return 0;
}

void bg_close(void)
{
    img_free(&g_bg);
}

void bg_update(u32 dt_ms)
{
    /* advance scroll: BG_SPEED_MS px/sec → (BG_SPEED_MS * dt_ms / 1000) px */
    /* in q16: (BG_SPEED_MS * 65536 / 1000) per ms ≈ 3932 per ms (60 px/s) */
    g_scroll_q16 += (u32)BG_SPEED_MS * 65536UL * dt_ms / 1000UL;
}

void bg_render(void)
{
    int src_y = (int)((g_scroll_q16 >> 16) % (u32)g_bg.h);
    img_blit_vscroll(&g_bg, PLAY_X0, PLAY_X1, PLAY_Y0, PLAY_Y1, src_y);
}
```

> **포인터 주의**: `img_load` 의 인자에서 `path`는 작업 디렉토리 기준 상대경로. `run.sh`가 `CD SRC`로 들어간 뒤 GAME.EXE 실행하므로 `IMG_DSRT.IMG`만으로 OK (SRC 안에 있음).

- [ ] **Step 3: 빌드 시도 — 아직 main 없음 (link 에러 예상)**

이 시점엔 빌드를 굳이 해보지 않아도 됨. Task 7에서 GAME.CPP 작성한 다음 통합 빌드.

- [ ] **Step 4: 커밋**

```bash
git add SRC/BG.H SRC/BG.CPP
git commit -m "Add BG module — vertical-scroll background

Loads IMG_DSRT.IMG via img_load() and renders to play area
(x=32..288, y=0..200) using img_blit_vscroll(). Subpixel scroll
tracked in q16 to avoid drift over long sessions. Default speed
60 px/sec; pulled from BG_SPEED_MS define for easy tuning."
```

---

## Task 7: 신규 SRC/GAME.CPP — 엔진 init + BG 루프 (시각 검증 #1)

**Files:**
- Create: `SRC/GAME.CPP` (신규 — Task 1에서 삭제됨)

엔진 모듈 init/close + 메인 루프. Plan 1의 첫 번째 시각 검증 지점: 배경만 스크롤되는 화면.

- [ ] **Step 1: 신규 GAME.CPP 작성 (배경만)**

`SRC/GAME.CPP`:

```c
/* Raiden 2 Clone — main entry.
 *
 * Plan 1: Vertical Slice 1 (assets + player + Vulcan). This file
 * grows incrementally — initially only the background scrolls. */

#include "GFX.H"
#include "INPUT.H"
#include "TIMER.H"
#include "SOUND.H"
#include "SFX.H"
#include "STATE.H"
#include "BG.H"

GameState g_state;

static u32 g_last_ms;

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
    g_state.gs = GS_PLAYING;
    g_state.gs_t_ms = 0;
}

int main(void)
{
    /* engine init order matters: timer → input → snd → sfx → gfx
     * (sound chains through INT 8 to the timer ISR) */
    timer_init();
    input_init();
    snd_init();
    sfx_init();
    gfx_init();

    state_reset();

    if (bg_init() != 0) {
        gfx_close();
        sfx_close();
        snd_close();
        input_close();
        timer_close();
        return 1;
    }

    g_last_ms = timer_ms();

    while (!input_key(KEY_ESC)) {
        u32 now = timer_ms();
        u32 dt = now - g_last_ms;
        g_last_ms = now;

        bg_update(dt);

        gfx_clear(0);
        bg_render();

        gfx_vsync();
        gfx_flip();
    }

    bg_close();
    gfx_close();
    sfx_close();
    snd_close();
    input_close();
    timer_close();
    return 0;
}
```

- [ ] **Step 2: 빌드**

```bash
./build.sh
```

Expected: BUILD.LOG에 errors 0, warnings 가능 (있어도 OK), `SRC/GAME.EXE` 생성됨.

```bash
cat BUILD.LOG | tail -20
ls -l SRC/GAME.EXE
```

- [ ] **Step 3: 시각 검증 #1 — 배경 스크롤**

```bash
./run.sh
```

Expected:
- DOSBox 창에 320x200 화면 표시
- 가운데 256x200 영역에 사막 배경이 **위에서 아래로 스크롤** (실제로는 카메라가 위로 올라가는 느낌)
- 좌우 32px씩은 까만색 (HUD 자리, 아직 미구현)
- 스크롤이 끊기지 않고 부드러움 (60 fps 가까이)
- 한 바퀴 돌고 다시 같은 그림 반복 (img.h가 256 wrapping)
- ESC 누르면 정상 종료

만약:
- 까만 화면 → IMG_DSRT.IMG 경로 또는 로드 실패. CD SRC 후 실행되는지 확인
- 색이 이상함 → 팔레트 매칭 문제, mkimg가 palette.json 적용 OK인지 확인
- 화면 폭 문제 → BG.CPP의 PLAY_X0/X1 확인

- [ ] **Step 4: 스크린캡 (선택)**

`./capture.sh`가 있으면 캡처해 시각 결과 기록.

- [ ] **Step 5: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "Add new GAME.CPP main loop — background-only smoke test

First visible Plan 1 milestone: scrolling desert background fills
the 256x200 play area centered in the 320x200 screen. State.gs
is set to GS_PLAYING but only background renders. ESC quits.

This is the foundation for adding PLAYER, WEAPON, BULLET in
subsequent tasks."
```

---

## Task 8: SRC/PLAYER.H + PLAYER.CPP — 플레이어 이동 + 애니메이션

**Files:**
- Create: `SRC/PLAYER.H`, `SRC/PLAYER.CPP`

스프라이트 로드, 8방향 이동(2 px/frame), 화면 경계 클램프, 좌/우 기울기 프레임 + 애니메이션.

- [ ] **Step 1: PLAYER.H 작성**

`SRC/PLAYER.H`:

```c
#ifndef PLAYER_H_INCLUDED
#define PLAYER_H_INCLUDED

#include "STATE.H"

/* Frame indices in the SPR_PSHP.SPR sheet (5x2 grid, indexes 1/3/6/8 are empty).
 * Animation: alternate between *_0 and *_1 every PLAYER_ANIM_PERIOD_MS. */
#define PSHP_FRAME_LEFT_0   0
#define PSHP_FRAME_IDLE_0   2
#define PSHP_FRAME_RIGHT_0  4
#define PSHP_FRAME_LEFT_1   5
#define PSHP_FRAME_IDLE_1   7
#define PSHP_FRAME_RIGHT_1  9

/* Player movement parameters. */
#define PLAYER_SPEED_PX        2
#define PLAYER_X_MIN          16
#define PLAYER_X_MAX         240   /* play-area-relative (0..256) */
#define PLAYER_Y_MIN          16
#define PLAYER_Y_MAX         190
#define PLAYER_W              16
#define PLAYER_H              24
#define PLAYER_ANIM_PERIOD_MS 100  /* 10 fps animation */

/* Player position in *play-area* coordinates (not screen coords).
 * Render code adds PLAY_X0 (=32) to convert. */
typedef struct {
    i16 x, y;            /* top-left of sprite, play-area coords */
    i16 vx;              /* current frame's x velocity (for tilt frame selection) */
    u32 anim_t_ms;       /* accumulated time for animation */
    u8  anim_phase;      /* 0 or 1 */
    u8  alive;           /* 1 = present, 0 = waiting for respawn (Plan 1: always 1) */
} Player;

extern Player g_player;

int  player_init(void);
void player_close(void);
void player_update(u32 dt_ms);
void player_render(void);

#endif
```

- [ ] **Step 2: PLAYER.CPP 작성**

`SRC/PLAYER.CPP`:

```c
#include "PLAYER.H"
#include "GFX.H"
#include "INPUT.H"
#include "SPRITE.H"

#define PLAY_X0    32   /* play area left edge in screen coords */

Player g_player;
static Sprite g_pshp;

int player_init(void)
{
    g_player.x = (PLAYER_X_MIN + PLAYER_X_MAX) / 2 - PLAYER_W / 2;
    g_player.y = PLAYER_Y_MAX - PLAYER_H;
    g_player.vx = 0;
    g_player.anim_t_ms = 0;
    g_player.anim_phase = 0;
    g_player.alive = 1;
    if (spr_load("SPR_PSHP.SPR", &g_pshp) != SPR_OK) return -1;
    return 0;
}

void player_close(void)
{
    spr_free(&g_pshp);
}

void player_update(u32 dt_ms)
{
    int dx = 0, dy = 0;
    if (input_key(KEY_LEFT))  dx -= PLAYER_SPEED_PX;
    if (input_key(KEY_RIGHT)) dx += PLAYER_SPEED_PX;
    if (input_key(KEY_UP))    dy -= PLAYER_SPEED_PX;
    if (input_key(KEY_DOWN))  dy += PLAYER_SPEED_PX;

    g_player.vx = (i16)dx;
    g_player.x += dx;
    g_player.y += dy;

    /* clamp to play area */
    if (g_player.x < PLAYER_X_MIN) g_player.x = PLAYER_X_MIN;
    if (g_player.x > PLAYER_X_MAX - PLAYER_W) g_player.x = PLAYER_X_MAX - PLAYER_W;
    if (g_player.y < PLAYER_Y_MIN) g_player.y = PLAYER_Y_MIN;
    if (g_player.y > PLAYER_Y_MAX - PLAYER_H) g_player.y = PLAYER_Y_MAX - PLAYER_H;

    /* animation timer */
    g_player.anim_t_ms += dt_ms;
    if (g_player.anim_t_ms >= PLAYER_ANIM_PERIOD_MS) {
        g_player.anim_t_ms -= PLAYER_ANIM_PERIOD_MS;
        g_player.anim_phase ^= 1;
    }
}

void player_render(void)
{
    int frame_idx;

    if (g_player.vx < 0) {
        frame_idx = g_player.anim_phase ? PSHP_FRAME_LEFT_1  : PSHP_FRAME_LEFT_0;
    } else if (g_player.vx > 0) {
        frame_idx = g_player.anim_phase ? PSHP_FRAME_RIGHT_1 : PSHP_FRAME_RIGHT_0;
    } else {
        frame_idx = g_player.anim_phase ? PSHP_FRAME_IDLE_1  : PSHP_FRAME_IDLE_0;
    }

    /* compiled sprites need x>=0, y>=0, fit on screen — player is clamped
     * to (16..240, 16..166) which is fully within 320x200 ✓ */
    int screen_x = PLAY_X0 + g_player.x;
    int screen_y = g_player.y;
    gfx_draw_csprite(g_pshp.frames[frame_idx], screen_x, screen_y);
}
```

> **검증 포인트**: 플레이어 좌표는 *play-area* (0~256)이고 렌더 시점에 +32(`PLAY_X0`) 시프트. PLAYER.H의 X_MAX(240)는 play-area 좌표이므로 화면에선 240+32=272. 플레이어 폭 16을 빼면 우측 끝 정확히 (256, ?). y는 그대로 화면 좌표 (HUD가 좌우라 y는 시프트 없음).

- [ ] **Step 3: 커밋 (아직 통합 전)**

```bash
git add SRC/PLAYER.H SRC/PLAYER.CPP
git commit -m "Add PLAYER module — 8-way movement + tilt animation

Loads SPR_PSHP.SPR (10-frame compiled sprite). Selects LEFT/IDLE/RIGHT
pose based on current vx, alternates between 2 anim frames every
100ms. Position is clamped to play-area-relative (16..240, 16..190)
and shifted by +PLAY_X0=32 at render time. Uses gfx_draw_csprite
(no clipping needed — clamps guarantee on-screen)."
```

---

## Task 9: PLAYER 통합 + 시각 검증 #2 (이동)

**Files:**
- Modify: `SRC/GAME.CPP`

플레이어를 메인 루프에 통합. 빌드 + DOSBox에서 화살표키 이동 확인.

- [ ] **Step 1: GAME.CPP에 PLAYER 통합**

`SRC/GAME.CPP` 수정:

기존 `#include "BG.H"` 다음 줄에 추가:

```c
#include "PLAYER.H"
```

`bg_init()` 다음에 player init 추가:

```c
    if (bg_init() != 0) {
        gfx_close();
        sfx_close();
        snd_close();
        input_close();
        timer_close();
        return 1;
    }

    if (player_init() != 0) {
        bg_close();
        gfx_close();
        sfx_close();
        snd_close();
        input_close();
        timer_close();
        return 2;
    }
```

메인 루프에 player update/render 추가:

```c
    while (!input_key(KEY_ESC)) {
        u32 now = timer_ms();
        u32 dt = now - g_last_ms;
        g_last_ms = now;

        bg_update(dt);
        player_update(dt);

        gfx_clear(0);
        bg_render();
        player_render();

        gfx_vsync();
        gfx_flip();
    }
```

shutdown에 player_close 추가 (LIFO 순서):

```c
    player_close();
    bg_close();
    gfx_close();
    sfx_close();
    snd_close();
    input_close();
    timer_close();
```

- [ ] **Step 2: 빌드**

```bash
./build.sh
cat BUILD.LOG | tail -20
ls -l SRC/GAME.EXE
```

Expected: 빌드 성공, GAME.EXE 갱신.

- [ ] **Step 3: 시각 검증 #2 — 플레이어 이동**

```bash
./run.sh
```

Expected:
- 배경이 여전히 스크롤됨 (Task 7 결과 유지)
- 플레이 영역 하단에 작은 우주선 (16x24) 표시
- **화살표키 누르면 부드럽게 이동** (8방향)
  - 왼쪽: 좌측 기울기 + 애니메이션 (0.1초마다 프레임 토글)
  - 오른쪽: 우측 기울기 + 애니메이션
  - 정지/위/아래만: 정면 자세 + 애니메이션
- 화면 경계 (256폭, 200높이)에서 멈춤 — 잘려 나가지 않음
- ESC 정상 종료

만약:
- 우주선 안 보임 → SPR_PSHP.SPR 로드 실패. spr_load 리턴 체크
- 색이 깨져 보임 → mksprite가 palette.json 적용 OK인지 확인
- 떨림/깜빡임 → gfx_vsync 호출 위치 확인
- 화면 밖으로 나감 → PLAYER_X_MAX/Y_MAX 클램프 로직 검증

- [ ] **Step 4: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "Integrate PLAYER into main loop — movement working

Player ship now visible and movable with arrow keys over scrolling
desert background. Init/shutdown ordering preserved (LIFO). Tilt
animation works for left/right motion, idle pose otherwise.

Plan 1 visual milestone #2 reached."
```

---

## Task 10: SRC/BULLET.H + BULLET.CPP — 플레이어 탄 풀

**Files:**
- Create: `SRC/BULLET.H`, `SRC/BULLET.CPP`

64-슬롯 탄 풀. 작은 사각형 탄(2x6)을 `gfx_fill_rect`로 그림. 화면 밖으로 나가면 비활성화.

이 plan에서는 플레이어 탄만 다룸. 적 탄 풀은 Plan 2.

- [ ] **Step 1: BULLET.H 작성**

`SRC/BULLET.H`:

```c
#ifndef BULLET_H_INCLUDED
#define BULLET_H_INCLUDED

#include "STATE.H"

/* Bullet kinds (Plan 1: only Vulcan). Spec defines more (LASER/PLASMA/HOMING/...);
 * those are added in later plans. */
typedef enum {
    BUL_PLAYER_VULCAN = 0
} BulletKind;

#define MAX_PLAYER_BULLETS 64

/* x,y in play-area coords (0..256, 0..200). Render adds PLAY_X0 shift. */
typedef struct {
    u8  active;
    u8  kind;
    u8  color_idx;
    u8  size_w;
    u8  size_h;
    i16 x, y;
    i16 vx, vy;          /* 1/16 px/frame subpixel */
    /* internal subpixel accumulator */
    i16 sx, sy;
} Bullet;

extern Bullet g_pbullets[MAX_PLAYER_BULLETS];

void bullet_init(void);

/* Allocate a slot and initialize as a player Vulcan bullet at (x, y) with
 * velocity (vx, vy) in 1/16 px/frame units. Returns 1 on success, 0 if pool full. */
int  bullet_spawn_player(BulletKind kind, i16 x, i16 y, i16 vx_q4, i16 vy_q4);

/* Advance all active bullets by 1 frame. (dt_ms accepted but not used yet —
 * we step in fixed frames here. Future plans may switch to dt-scaled.) */
void bullet_update_all(u32 dt_ms);

/* Render all active player bullets. */
void bullet_render_player(void);

#endif
```

- [ ] **Step 2: BULLET.CPP 작성**

`SRC/BULLET.CPP`:

```c
#include "BULLET.H"
#include "GFX.H"

#define PLAY_X0    32
#define PLAY_W    256
#define PLAY_H    200

Bullet g_pbullets[MAX_PLAYER_BULLETS];

void bullet_init(void)
{
    int i;
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) g_pbullets[i].active = 0;
}

int bullet_spawn_player(BulletKind kind, i16 x, i16 y, i16 vx_q4, i16 vy_q4)
{
    int i;
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        Bullet *b = &g_pbullets[i];
        if (b->active) continue;
        b->active = 1;
        b->kind = (u8)kind;
        b->color_idx = 25;     /* bright red — same as old missile demo (COL_MISSILE);
                                  swap to a yellow-ramp index after palette inspection */
        b->size_w = 2;
        b->size_h = 6;
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

void bullet_update_all(u32 dt_ms)
{
    int i;
    (void)dt_ms;  /* fixed-step for now */
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        Bullet *b = &g_pbullets[i];
        if (!b->active) continue;

        /* subpixel: accumulate q4 velocity into sx,sy; integer carry → x,y */
        b->sx += b->vx;
        b->sy += b->vy;
        b->x  += b->sx >> 4;  b->sx &= 0x0F;
        b->y  += b->sy >> 4;  b->sy &= 0x0F;

        /* deactivate if out of play area (small slack) */
        if (b->y + b->size_h < 0 || b->y > PLAY_H ||
            b->x + b->size_w < 0 || b->x > PLAY_W) {
            b->active = 0;
        }
    }
}

void bullet_render_player(void)
{
    int i;
    for (i = 0; i < MAX_PLAYER_BULLETS; i++) {
        const Bullet *b = &g_pbullets[i];
        if (!b->active) continue;
        gfx_fill_rect(PLAY_X0 + b->x, b->y, b->size_w, b->size_h, b->color_idx);
    }
}
```

> **q4 단위 주의**: vx/vy는 1/16 px/frame 단위. 위로 발사 시 `vy_q4 = -96` 정도 (= 6 px/frame).

- [ ] **Step 3: 커밋 (아직 발사 트리거 없음)**

```bash
git add SRC/BULLET.H SRC/BULLET.CPP
git commit -m "Add BULLET module — player bullet pool (64 slots)

Subpixel velocity in q4 (1/16 px/frame) for smooth movement at
non-integer speeds. Plan 1 only handles Vulcan kind; renders as
2x6 yellow rectangles via gfx_fill_rect. Pool deactivates bullets
that exit the play area on any side."
```

---

## Task 11: SRC/WEAPON.H + WEAPON.CPP — Vulcan L1 발사

**Files:**
- Create: `SRC/WEAPON.H`, `SRC/WEAPON.CPP`

Vulcan L1: 정면 1탄, 발사 간격 80ms, 위로 6 px/frame. 후속 plan에서 L2~L4 + Laser/Plasma 추가.

- [ ] **Step 1: WEAPON.H 작성**

`SRC/WEAPON.H`:

```c
#ifndef WEAPON_H_INCLUDED
#define WEAPON_H_INCLUDED

#include "STATE.H"

/* Reset cooldown counters (call at game start / after death). */
void weapon_init(void);

/* Process per-frame fire input. Reads g_state.weapon, g_state.weapon_level,
 * and player position; spawns bullets via bullet_spawn_player(). */
void weapon_fire_tick(u32 dt_ms, int z_held);

#endif
```

- [ ] **Step 2: WEAPON.CPP 작성**

`SRC/WEAPON.CPP`:

```c
#include "WEAPON.H"
#include "BULLET.H"
#include "PLAYER.H"

#define VULCAN_PERIOD_MS  80
#define VULCAN_VY_Q4    (-96)   /* -6 px/frame upward */

static u32 g_vulcan_cd_ms;

void weapon_init(void)
{
    g_vulcan_cd_ms = 0;
}

static void fire_vulcan_l1(void)
{
    /* Single bullet from the player ship's nose (top-center). */
    i16 px = g_player.x + PLAYER_W / 2 - 1;   /* 2px wide bullet, centered */
    i16 py = g_player.y - 6;                  /* just above the ship */
    bullet_spawn_player(BUL_PLAYER_VULCAN, px, py, 0, VULCAN_VY_Q4);
}

void weapon_fire_tick(u32 dt_ms, int z_held)
{
    /* decrement cooldown */
    if (g_vulcan_cd_ms > dt_ms) {
        g_vulcan_cd_ms -= dt_ms;
    } else {
        g_vulcan_cd_ms = 0;
    }

    if (!z_held) return;

    if (g_vulcan_cd_ms > 0) return;

    /* Plan 1: only Vulcan L1. Spec lists L1-L4 spread; later plans add. */
    fire_vulcan_l1();
    g_vulcan_cd_ms = VULCAN_PERIOD_MS;
}
```

- [ ] **Step 3: 커밋 (아직 통합 전)**

```bash
git add SRC/WEAPON.H SRC/WEAPON.CPP
git commit -m "Add WEAPON module — Vulcan L1 firing

Single straight-up bullet at 6 px/frame from player ship nose, 80ms
cooldown (~12.5 shots/sec). Driven by Z-key held state from main loop.
Plan 1 implements only L1; L2-L4 spread + Laser + Plasma deferred."
```

---

## Task 12: WEAPON 통합 + 시각 검증 #3 (발사)

**Files:**
- Modify: `SRC/GAME.CPP`

Vulcan 발사를 메인 루프에 통합. 마지막 시각 검증.

- [ ] **Step 1: GAME.CPP에 BULLET + WEAPON 통합**

새 include:

```c
#include "BULLET.H"
#include "WEAPON.H"
```

main()에서 player_init() 다음에 추가:

```c
    bullet_init();
    weapon_init();
```

(이 둘은 실패할 수 없는 init이라 리턴값 체크 불필요.)

메인 루프 — `player_update(dt);` 다음에 weapon + bullet 추가:

```c
    while (!input_key(KEY_ESC)) {
        u32 now = timer_ms();
        u32 dt = now - g_last_ms;
        g_last_ms = now;

        bg_update(dt);
        player_update(dt);
        weapon_fire_tick(dt, input_key(KEY_Z));
        bullet_update_all(dt);

        gfx_clear(0);
        bg_render();
        bullet_render_player();
        player_render();

        gfx_vsync();
        gfx_flip();
    }
```

> **렌더 순서**: 배경 → 탄 → 플레이어. 플레이어가 탄 위에 그려져야 발사 시점에 자기 탄이 자기 앞을 막는 부자연스러운 화면 안 나옴.

shutdown은 변경 없음 (BULLET/WEAPON에는 close 없음, 정적 배열).

- [ ] **Step 2: 빌드**

```bash
./build.sh
cat BUILD.LOG | tail -20
```

Expected: 빌드 성공.

- [ ] **Step 3: 시각 검증 #3 — Vulcan 발사**

```bash
./run.sh
```

Expected:
- 배경 + 플레이어 이전과 동일
- **Z를 누르고 있으면 노란 작은 사각형(2x6)이 플레이어 위쪽에서 직진으로 위로 날아감**
- 발사 간격 약 80ms (1초에 12발 정도, 화면에 동시에 여러 발 보임)
- 탄이 화면 위쪽 끝에 도달하면 사라짐
- 플레이어를 좌우로 움직이면 발사 위치도 따라 움직임 (대각선으로 쏘는 것처럼 보일 수 있는데 탄 자체는 직진 — 의도된 동작)
- 화살표키 이동도 발사와 동시에 가능
- ESC 종료 정상

만약:
- 탄이 안 나옴 → weapon_fire_tick 호출 위치 + KEY_Z 인자 확인
- 탄이 너무 빠르거나 느림 → VULCAN_VY_Q4 값 조정
- 탄이 잘못된 위치에서 나옴 → fire_vulcan_l1의 px/py 계산 확인
- 탄이 안 사라지고 화면에 남음 → bullet_update_all의 deactivate 조건 확인

- [ ] **Step 4: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "Integrate WEAPON + BULLET — Vulcan firing working

Hold Z: streaming Vulcan bullets fire upward from player ship at
~12.5 shots/sec. Render order: background → bullets → player.

Plan 1 final visual milestone reached: scrolling background +
8-way movement + Vulcan firing. Ready for Plan 2 (enemies +
collision)."
```

---

## Task 13: 마무리 + Plan 2 인계 메모

**Files:**
- Create: `docs/superpowers/plans/2026-04-28-raiden2-clone-plan1-DONE.md` (선택)

이 plan 완료 시점의 최종 상태 요약.

- [ ] **Step 1: 최종 동작 확인**

다음을 한번씩 확인:

```bash
./build.sh                                       # 빌드 성공
./run.sh                                         # 게임 실행
# DOSBox에서: 화살표키, Z키 눌러 발사 확인, ESC로 종료
```

각 동작이 spec section 4.1, 4.2, 4.3 (Vulcan L1 부분)과 일치하는지 점검.

- [ ] **Step 2: 파일 인벤토리 확인**

```bash
echo "=== Game modules ==="
ls SRC/*.CPP SRC/*.H | grep -E "STATE|BG|PLAYER|WEAPON|BULLET|GAME"
echo "=== Generated assets ==="
ls SRC/SPR_*.SPR SRC/IMG_*.IMG
echo "=== Build output ==="
ls -lh SRC/GAME.EXE
echo "=== Tools ==="
ls tools/inspect_sheet.ts
```

Expected:
- SRC/STATE.H, SRC/BG.H/.CPP, SRC/PLAYER.H/.CPP, SRC/WEAPON.H/.CPP, SRC/BULLET.H/.CPP, SRC/GAME.CPP — 8 files
- SRC/SPR_PSHP.SPR, SRC/IMG_DSRT.IMG — 2 generated
- SRC/GAME.EXE — built
- tools/inspect_sheet.ts — helper

- [ ] **Step 3: git 로그 점검**

```bash
git log --oneline -15
```

Expected: 이 plan의 12-13개 커밋이 main에 있음.

- [ ] **Step 4: Plan 2 (적 + 충돌) 사전 메모 작성 — 선택**

다음 plan에서 작업할 항목 리스트를 한 줄씩:

```
Plan 2 후보 (M4-M5):
- ENEMY 모듈 + ENEMY_AI (popcorn 1종 + PAT_STRAIGHT_DOWN)
- 적 탄 풀 (BULLET 확장, MAX_ENEMY_BULLETS=256)
- 충돌 검사 (player_bullet vs air_enemy, enemy_bullet vs player_hitbox 4x4)
- 폭발 이펙트 (explosion.png 변환 + EFFECT 모듈)
- 점수 가산 (popcorn 격파 100점)
- 단순 패럴랙스 — clouds 추가 (img_blit_vscroll의 transparent 변형 또는 gfx_blit_trans 직접)
```

이 메모는 별도 파일로 만들지 않고 chat 응답 또는 다음 brainstorm에서 다룬다.

---

## Self-Review Checklist (구현 시작 전 본인 검증)

이 plan을 실제 따라가기 전 한 번 더 점검:

- [ ] **Spec 커버리지** (M1~M3 부분만):
  - M1 자산 파이프라인: Tasks 2-4 ✓
  - M2 PLAYER + 기본 화면: Tasks 6-9 ✓  
  - M3 WEAPON + BULLET (Vulcan only): Tasks 10-12 ✓
  - 적/충돌/아이템/봄: 의도적으로 제외 (후속 plan)
- [ ] **타입 일관성**: 
  - `i16 x, y` (Player, Bullet) — 일관
  - `u32 dt_ms` 모든 update 함수 — 일관
  - `Sprite` 구조체 사용 — engine SPRITE.H 그대로
- [ ] **함수 시그니처 매칭**:
  - bullet_spawn_player → BulletKind, i16 x4, return int
  - weapon_fire_tick → u32 dt_ms, int z_held
  - 모든 모듈 *_init() return int, *_close() return void
- [ ] **빌드 시스템**: WCL386 *.CPP 가 자동으로 신규 *.CPP 들을 잡음 (BUILD.BAT 변경 불필요)
- [ ] **자산 경로**: img_load/spr_load는 `CD SRC` 후 실행되므로 상대경로 OK

문제 발견 시 위 task에 인라인으로 수정.

---

## Open Items (이 Plan 범위 밖)

다음 plan에서 다룸:
- 패럴랙스 클라우드 (clouds.png + clouds-transparent.png)
- 적 시스템 (Enemy struct, AI 패턴, 21 air types + 4 ground types)
- 충돌 검사 (4종)
- HUD (점수/잔기/봄/메달 사이드 표시)
- 폭발 이펙트
- 음악 (ST00.vgm 로드 + 재생)
