# 6곡 완전 활용 확장 Implementation Plan

> **실행 방식(사용자 선호):** 이 계획은 **인라인 실행 + build/run 시각 검증**으로 진행한다.
> 서브에이전트/자동 커밋 없음. 각 태스크는 하나의 독립 검증 가능한 산출물로 끝나며,
> 커밋 여부는 사용자가 직접 결정한다.

**Goal:** 보유한 6개 VGM(ST00~03, ST06, ST07)을 전부 재생되게 만든다 — 메뉴 전용곡·Stage 4·엔딩 스태프롤을 추가하고, 배경을 절차적 우주 별 스크롤로 교체한다.

**Architecture:** 기존 FSM(`GAME.CPP`)·스테이지 스크립트(`STAGE.CPP`)·보스 모듈(`BOSS.CPP`)·HUD(`HUD.CPP`)·배경(`BG.CPP`)의 모듈 경계를 그대로 유지한 채, 각 모듈에 최소 침습으로 확장한다. 신규 아트는 최종보스 스프라이트 1개뿐.

**Tech Stack:** Watcom C/C++ 10.x (WCL386, DOS4GW), VGA Mode 13h, `#pragma aux` 인라인 asm, Bun/TypeScript 에셋 파이프라인(`mksprite.ts`), OPL2 VGM 재생(`SOUND`).

## Global Constraints

- STL·예외·RTTI 금지. C 표준 라이브러리만. 4-space 들여쓰기, K&R 브레이스, 소문자 함수명.
- 파일명은 DOS 8.3 대문자. 스프라이트는 `SPR_XXXX.SPR`, VGM은 `STxx.VGM`.
- 사운드 교체는 반드시 `snd_stop() → snd_load() → snd_play()` 순서. ISR init/close 순서 불변.
- 정수는 32-bit flat. 서브픽셀은 q4/q8 고정소수점 관행을 따른다.
- 검증은 자동 테스트가 없다 → `./build.sh` 후 `./run.sh`로 **시각/청각 실측**. 오디오 이상 시 `./sfxtest.sh`.
- 재생 파일은 `SRC/`에 있어야 한다(DOSBox가 SRC를 마운트해 실행).

## VGM 매핑 (목표 최종 상태)

| 용도 | VGM | 로드 지점 |
|------|-----|----------|
| 메인메뉴 (GS_TITLE) | `ST06.VGM` | init + 모든 타이틀 복귀 |
| Stage 1 | `ST00.VGM` | 게임 시작 / 스테이지 진입 |
| Stage 2 | `ST01.VGM` | 스테이지 클리어 후 진급 |
| Stage 3 | `ST02.VGM` | 〃 |
| Stage 4 | `ST03.VGM` | 〃 |
| 엔딩 (GS_ENDING) | `ST07.VGM` | GS_ENDING 진입 |

---

## Task 1: VGM 에셋 SRC 반입 (ST06, ST07)

**Files:**
- Copy: `ASSETS/raw/audio_vgm/ST06.vgm` → `SRC/ST06.VGM`
- Copy: `ASSETS/raw/audio_vgm/ST07.vgm` → `SRC/ST07.VGM`
- Modify (선택): `convert.sh` — VGM 복사 섹션이 있으면 ST06/ST07 추가

**Interfaces:**
- Produces: `SRC/ST06.VGM`, `SRC/ST07.VGM` 파일 존재 (이후 태스크의 `snd_load` 대상)

- [ ] **Step 1: 현재 SRC의 VGM 목록 확인**

Run: `ls SRC/*.VGM`
Expected: `ST00.VGM ST01.VGM ST02.VGM ST03.VGM TYRIAN.VGM` (ST06/07 없음)

- [ ] **Step 2: convert.sh에 VGM 복사 로직이 있는지 확인**

Run: `grep -niE "vgm|VGM|audio_vgm" convert.sh`
Expected: VGM 복사 라인이 있으면 그 패턴을 따르고, 없으면 Step 3의 수동 복사로 반입.

- [ ] **Step 3: 대문자 8.3 이름으로 SRC에 복사**

```bash
cp "ASSETS/raw/audio_vgm/ST06.vgm" SRC/ST06.VGM
cp "ASSETS/raw/audio_vgm/ST07.vgm" SRC/ST07.VGM
```

(convert.sh에 VGM 섹션이 있었다면 거기에 동일 복사 2줄을 추가해 재현성을 확보한다.)

- [ ] **Step 4: 반입 확인**

Run: `ls -la SRC/ST06.VGM SRC/ST07.VGM`
Expected: 두 파일 모두 존재, 크기 > 0 (약 108KB, 102KB).

---

## Task 2: 배경 교체 — 절차적 우주 별 스크롤 (BG.CPP)

스크롤 그리드(`SPR_GRID.SPR`)를 제거하고 반짝이는 별 스타필드로 교체한다. 전 스테이지 공통.
`BG.H`의 공개 함수 시그니처(`bg_init/bg_close/bg_update/bg_render/bg_reset_scroll`)는 불변.

**Files:**
- Modify: `SRC/BG.CPP` (전체 내부 재작성)

**Interfaces:**
- Consumes: `gfx_fill_rect(x,y,w,h,color)`, `gfx_pixel(x,y,color)` (GFX.H)
- Produces: 동일한 공개 API. 내부에 `Star g_stars[STAR_COUNT]` + 자체 LCG.

- [ ] **Step 1: BG.CPP를 스타필드 구현으로 교체**

`SRC/BG.CPP` 전체를 아래로 교체:

```c
#include "BG.H"
#include "GFX.H"

#define PLAY_X0    32
#define PLAY_Y0     0
#define PLAY_W    256
#define PLAY_H    200

#define STAR_COUNT 96

/* One drifting star. y is q8 fixed-point for smooth slow scroll.
 * base = grayscale brightness (palette 1..15); layer sets speed+base. */
typedef struct {
    i16 x;         /* 0..PLAY_W-1 (play-area local) */
    i32 y_q8;      /* 0..PLAY_H<<8 */
    i16 spd_pxs;   /* downward px/sec */
    u8  base;      /* base grayscale color */
    u8  phase;     /* twinkle phase offset */
} Star;

static Star g_stars[STAR_COUNT];
static u32  g_bg_rng = 0x13572468UL;
static u32  g_twinkle_t;

static u32 bg_rand(void)
{
    g_bg_rng = g_bg_rng * 1103515245UL + 12345UL;
    return g_bg_rng;
}

/* Assign layer parameters (0=far/slow/dim .. 2=near/fast/bright). */
static void star_seed(Star *s, int fresh_top)
{
    int layer = (int)(bg_rand() % 3);
    s->x = (i16)(bg_rand() % PLAY_W);
    if (fresh_top) s->y_q8 = 0;
    else           s->y_q8 = (i32)((bg_rand() % PLAY_H) << 8);
    s->phase = (u8)(bg_rand() & 0xFF);
    switch (layer) {
    case 0:  s->spd_pxs = 18; s->base = 7;  break;   /* far, dim */
    case 1:  s->spd_pxs = 40; s->base = 11; break;   /* mid */
    default: s->spd_pxs = 74; s->base = 15; break;   /* near, bright */
    }
}

int bg_init(void)
{
    int i;
    g_bg_rng = 0x13572468UL;
    g_twinkle_t = 0;
    for (i = 0; i < STAR_COUNT; i++) star_seed(&g_stars[i], 0);
    return 0;
}

void bg_close(void)
{
    /* No owned resources. */
}

void bg_update(u32 dt_ms)
{
    int i;
    g_twinkle_t += dt_ms;
    for (i = 0; i < STAR_COUNT; i++) {
        Star *s = &g_stars[i];
        s->y_q8 += (i32)(((u32)s->spd_pxs * 256UL * dt_ms) / 1000UL);
        if ((s->y_q8 >> 8) >= PLAY_H) {
            star_seed(s, 1);   /* recycle at top with a new x/layer */
        }
    }
}

void bg_render(void)
{
    int i;
    gfx_fill_rect(PLAY_X0, PLAY_Y0, PLAY_W, PLAY_H, 0);
    for (i = 0; i < STAR_COUNT; i++) {
        Star *s = &g_stars[i];
        int y = (int)(s->y_q8 >> 8);
        u8  c = s->base;
        /* Twinkle: periodically dim by a couple of shades. */
        if ((((g_twinkle_t >> 7) + s->phase) & 7) == 0) {
            c = (u8)(s->base > 5 ? s->base - 4 : 1);
        }
        gfx_pixel(PLAY_X0 + (int)s->x, y, c);
    }
}

void bg_reset_scroll(void)
{
    /* Re-seed the field on stage transitions for a fresh sky. */
    int i;
    for (i = 0; i < STAR_COUNT; i++) star_seed(&g_stars[i], 0);
}
```

- [ ] **Step 2: 빌드**

Run: `./build.sh`
Expected: `BUILD.LOG`에 컴파일 에러 없음, `SRC/GAME.EXE` 생성. (`SPR_GRID` 미참조 경고/에러 없어야 함 — BG.CPP에서 SPRITE 사용 전부 제거됨.)

- [ ] **Step 3: 실행 시각 검증**

Run: `./run.sh`
Expected: 타이틀·게임 화면 배경이 그리드 대신 **아래로 흐르는 별들**로 표시되고, 3계층 속도차 + 미세한 반짝임이 보인다. 플레이 영역(x=32~288) 밖으로 새지 않는다.

---

## Task 3: BGM 라우팅 — 메뉴 ST06 + 스테이지별 ST00~03 (GAME.CPP)

메뉴는 ST06, 각 스테이지는 ST00~03, 타이틀 복귀는 항상 ST06이 되도록 로딩 지점을 정리한다.
DRY를 위해 두 헬퍼를 도입한다.

**Files:**
- Modify: `SRC/GAME.CPP` (init BGM, GS_TITLE 시작 전이, GS_STAGE_CLEAR 진급, 타이틀 복귀 지점들)

**Interfaces:**
- Consumes: `snd_stop()`, `snd_load(const char*)`, `snd_play()` (SOUND.H)
- Produces: `static void bgm_menu(void)`, `static void bgm_stage(u8 stage)` — 이후 엔딩 태스크가 `bgm_menu()`를 재사용.

- [ ] **Step 1: BGM 헬퍼 2개 추가**

`GAME.CPP` 상단(파일 스코프, `next_rand` 근처)에 추가:

```c
/* Centralized BGM routing so every entry/return path stays consistent. */
static void bgm_menu(void)
{
    snd_stop();
    snd_load("ST06.VGM");
    snd_play();
}

static void bgm_stage(u8 stage)
{
    snd_stop();
    switch (stage) {
    case 1:  snd_load("ST00.VGM"); break;
    case 2:  snd_load("ST01.VGM"); break;
    case 3:  snd_load("ST02.VGM"); break;
    default: snd_load("ST03.VGM"); break;   /* stage 4 */
    }
    snd_play();
}
```

- [ ] **Step 2: init BGM을 메뉴곡으로 변경**

`GAME.CPP:84-86` 부근:

```c
    /* Load BGM and start playback (snd_load returns SND_OK = 0 on success). */
    snd_load("ST00.VGM");
    snd_play();
```

→ 로 교체:

```c
    /* Boot straight into the menu track. */
    bgm_menu();
```

- [ ] **Step 3: 게임 시작 시 Stage 1 곡 로드**

`GAME.CPP`의 `GS_TITLE` 케이스에서 `z_pressed`로 게임을 시작하는 블록:

```c
            if (z_pressed) {
                state_reset();
                bullet_clear_all_enemy();
                g_state.gs = GS_STAGE_INTRO;
                g_state.gs_t_ms = 0;
            }
```

→ 로 교체(스테이지 1 BGM 추가):

```c
            if (z_pressed) {
                state_reset();
                bullet_clear_all_enemy();
                bgm_stage(1);
                g_state.gs = GS_STAGE_INTRO;
                g_state.gs_t_ms = 0;
            }
```

- [ ] **Step 4: 스테이지 진급 BGM을 헬퍼로 교체**

`GAME.CPP:337-343` 부근:

```c
                if (g_state.stage < MAX_STAGES) {
                    g_state.stage++;
                    snd_stop();
                    if      (g_state.stage == 2) snd_load("ST01.VGM");
                    else if (g_state.stage == 3) snd_load("ST02.VGM");
                    else                          snd_load("ST00.VGM");
                    snd_play();
                    bg_reset_scroll();
```

→ 로 교체:

```c
                if (g_state.stage < MAX_STAGES) {
                    g_state.stage++;
                    bgm_stage(g_state.stage);
                    bg_reset_scroll();
```

- [ ] **Step 5: 타이틀 복귀 지점을 메뉴곡으로 통일**

(a) `GS_CONTINUE_PROMPT` 타임아웃(현재 `GAME.CPP:378-381` 부근):

```c
            if (g_state.gs_t_ms >= 10000) {
                g_state.gs = GS_TITLE;
                g_state.gs_t_ms = 0;
                break;
            }
```

→

```c
            if (g_state.gs_t_ms >= 10000) {
                bgm_menu();
                g_state.gs = GS_TITLE;
                g_state.gs_t_ms = 0;
                break;
            }
```

(b) `GS_ENDING`의 Z 복귀(현재 `GAME.CPP:390-396` 부근)에서 `snd_load("ST00.VGM")` 사용 중 →
이 블록은 **Task 6에서 크게 바뀌므로** 여기서는 건드리지 않는다(Task 6에서 `bgm_menu()`로 대체).

(c) `default:` 폴백(현재 `GAME.CPP:403-406` 부근):

```c
        default:
            g_state.gs = GS_TITLE;
            g_state.gs_t_ms = 0;
            break;
```

→

```c
        default:
            bgm_menu();
            g_state.gs = GS_TITLE;
            g_state.gs_t_ms = 0;
            break;
```

- [ ] **Step 6: 빌드 & 청각 검증**

Run: `./build.sh` 후 `./run.sh`
Expected: 타이틀에서 **ST06**이 재생. Z로 시작하면 **ST00**(Stage 1). 스테이지 클리어로 진급 시 Stage 2→**ST01**, Stage 3→**ST02**. (Stage 4는 Task 4 이후 도달 가능.) 게임오버 후 컨티뉴 타임아웃으로 타이틀 복귀 시 다시 ST06.

---

## Task 4: Stage 4 콘텐츠 — MAX_STAGES=4 + stage4_script + HUD 배열 확장

Stage 4를 새 최종 스테이지로 추가한다. 배경은 공통 스타필드, 미드보스는 기존 `E_MID_CI` 재사용,
메인보스는 Task 5에서 붙인다. 신규 스프라이트 없음.

**Files:**
- Modify: `SRC/GAME.CPP:28` — `#define MAX_STAGES 3` → `4`
- Modify: `SRC/STAGE.CPP` — `stage4_script[]` 추가 + `stage_init` case 4
- Modify: `SRC/HUD.CPP` — `hud_render_stage_intro` / `hud_render_stage_clear` 배열 4개로 확장
- Modify: `SRC/STATE.H:44` — `stage` 주석 `1~3` → `1~4`

**Interfaces:**
- Consumes: `StageEvent`, `E_*`/`AI_*` 매크로, `enemy_spawn` (STAGE.CPP 기존)
- Produces: `stage4_script`가 `stage_init(4)`로 선택됨. 130초 지점에서 GAME.CPP가 보스 페이즈로 전이(기존 로직).

- [ ] **Step 1: MAX_STAGES 증가**

`GAME.CPP:28`:

```c
#define MAX_STAGES 3
```
→
```c
#define MAX_STAGES 4
```

- [ ] **Step 2: STATE.H 주석 갱신**

`STATE.H`의 `u8  stage;` 라인 주석 `/* 1~3 */` → `/* 1~4 */`.

- [ ] **Step 3: stage4_script 추가**

`STAGE.CPP`의 `stage3_script[]` 정의 바로 뒤(그리고 `g_active_script` 선언 앞)에 추가.
기존 적/AI 매크로만 사용한다. 모선 내부 컨셉 = 밀도 높은 정예 + 링/스파이럴 다발.

```c
/* Stage 4 — Enemy mothership interior. ~130s, mid-boss = CI (reused),
 * main = final boss (Task 5). Identity: elite gauntlet, heavy rings. */
static const StageEvent stage4_script[] = {
    /* === Act 1 (0-40s) === */
    {    500, E_FIGHTER_AF,  AI_A3,  2, 0, 128, V_NORM },
    {   2000, E_BURSTER,     AI_F5,  4, 0, 128, V_SLOW },
    {   4000, E_BLUE,        AI_R16, 1, 0,  64, V_SLOW },
    {   4000, E_BLUE,        AI_R16, 1, 0, 192, V_SLOW },
    {   6500, E_MED_C,       AI_HOM, 1, 0,  80, V_SLOW },
    {   6500, E_MED_C,       AI_HOM, 1, 0, 176, V_SLOW },
    {   9000, E_POPCORN_B,   AI_A3,  6, 0, 128, V_NORM },
    {  11500, E_TURRET,      AI_SPI, 1, 0,  48, V_SLOW },
    {  11500, E_TURRET,      AI_SPI, 1, 0, 128, V_SLOW },
    {  11500, E_TURRET,      AI_SPI, 1, 0, 208, V_SLOW },
    {  15000, E_FIGHTER_AF,  AI_F7,  2, 0, 128, V_SLOW },
    {  18000, E_KAMIKAZE,    AI_DN,  1, 0,  32, V_DIVE },
    {  18000, E_KAMIKAZE,    AI_DN,  1, 0,  96, V_DIVE },
    {  18000, E_KAMIKAZE,    AI_DN,  1, 0, 160, V_DIVE },
    {  18000, E_KAMIKAZE,    AI_DN,  1, 0, 224, V_DIVE },
    {  21500, E_BLUE,        AI_R16, 1, 0, 128, V_SLOW },
    {  24000, E_MED_C,       AI_LSR, 1, 0,  80, V_SLOW },
    {  24000, E_MED_C,       AI_LSR, 1, 0, 176, V_SLOW },
    {  27500, E_BURSTER,     AI_R8,  4, 0, 128, V_SLOW },
    {  31000, E_FIGHTER_AF,  AI_HOM, 2, 0, 128, V_SLOW },
    {  34500, E_POPCORN_A,   AI_A3,  7, 0, 128, V_NORM },
    {  37500, E_TURRET,      AI_SPI, 1, 0,  48, V_SLOW },
    {  37500, E_TURRET,      AI_SPI, 1, 0, 208, V_SLOW },

    /* === Act 2 (40-65s): mid-boss CI (reused) === */
    {  41000, E_MID_CI,      AI_R16, 1, 0, 128, V_SLOW },

    /* === Act 3 (65-115s): peak density === */
    {  67000, E_FIGHTER_AF,  AI_F7,  3, 0, 128, V_SLOW },
    {  70000, E_BLUE,        AI_R16, 1, 0,  64, V_SLOW },
    {  70000, E_BLUE,        AI_R16, 1, 0, 192, V_SLOW },
    {  73500, E_MED_C,       AI_SPI, 1, 0,  64, V_SLOW },
    {  73500, E_MED_C,       AI_SPI, 1, 0, 192, V_SLOW },
    {  77000, E_BURSTER,     AI_AIM, 6, 0, 128, V_NORM },
    {  80500, E_KAMIKAZE,    AI_DN,  1, 0,  48, V_DIVE },
    {  80500, E_KAMIKAZE,    AI_DN,  1, 0, 144, V_DIVE },
    {  80500, E_KAMIKAZE,    AI_DN,  1, 0, 208, V_DIVE },
    {  84000, E_FIGHTER_AF,  AI_HOM, 3, 0, 128, V_SLOW },
    {  88000, E_BLUE,        AI_R16, 1, 0,  80, V_SLOW },
    {  88000, E_BLUE,        AI_R16, 1, 0, 176, V_SLOW },
    {  92000, E_TURRET,      AI_SPI, 1, 0,  48, V_SLOW },
    {  92000, E_TURRET,      AI_SPI, 1, 0, 208, V_SLOW },
    {  96000, E_MED_C,       AI_LSR, 1, 0, 128, V_SLOW },
    { 100000, E_FIGHTER_R,   AI_SIN, 6, 0, 128, V_SLOW },
    { 104000, E_BURSTER,     AI_F5,  4, 0, 128, V_SLOW },
    { 108000, E_POPCORN_B,   AI_F7,  4, 0, 128, V_SLOW },
    { 112000, E_BLUE,        AI_R16, 1, 0, 128, V_SLOW },

    /* === Act 4 (115-130s): cooldown before final boss === */
    { 116000, E_FIGHTER_AF,  AI_AIM, 3, 0, 128, V_NORM },
    { 122000, E_BURSTER,     AI_AIM, 3, 0, 128, V_NORM },
    EVT_END
};
```

- [ ] **Step 4: stage_init에 case 4 추가**

`STAGE.CPP`의 `stage_init`:

```c
    switch (stage_num) {
    case 2:  g_active_script = stage2_script; break;
    case 3:  g_active_script = stage3_script; break;
    default: g_active_script = stage1_script; break;
    }
```
→
```c
    switch (stage_num) {
    case 2:  g_active_script = stage2_script; break;
    case 3:  g_active_script = stage3_script; break;
    case 4:  g_active_script = stage4_script; break;
    default: g_active_script = stage1_script; break;
    }
```

- [ ] **Step 5: HUD 스테이지 인트로/클리어 배열 4개로 확장**

`HUD.CPP`의 `hud_render_stage_intro`:

```c
    static const char *titles[3] = { "1단계", "2단계", "3단계" };
    static const char *names [3] = { "사막",  "강",    "우주"  };
    int s = (int)g_state.stage - 1;
    if (s < 0) s = 0;
    if (s > 2) s = 2;
```
→
```c
    static const char *titles[4] = { "1단계", "2단계", "3단계", "4단계" };
    static const char *names [4] = { "사막",  "강",    "우주",  "모선"  };
    int s = (int)g_state.stage - 1;
    if (s < 0) s = 0;
    if (s > 3) s = 3;
```

`HUD.CPP`의 `hud_render_stage_clear`:

```c
    static const char *titles[3] = { "1단계 완료", "2단계 완료", "3단계 완료" };
    int s = (int)g_state.stage - 1;
    if (s < 0) s = 0;
    if (s > 2) s = 2;
```
→
```c
    static const char *titles[4] = { "1단계 완료", "2단계 완료", "3단계 완료", "4단계 완료" };
    int s = (int)g_state.stage - 1;
    if (s < 0) s = 0;
    if (s > 3) s = 3;
```

- [ ] **Step 6: 빌드 & 검증**

Run: `./build.sh` 후 `./run.sh`
Expected: Stage 3 클리어 후 **Stage 4("4단계 / 모선")로 진입**하고 **ST03**이 재생된다. 잡몹·CI 미드보스가 정상 등장. (130초 후 보스 페이즈는 Task 5 전까지 Tank2 폴백이 뜰 수 있음 — Task 5에서 교체.)

---

## Task 5: 최종보스 — 신규 스프라이트 + BOSS 모듈 배선 + Stage 4 보스 트리거

Stage 4 메인보스로 신규 최종보스 1개를 추가한다. 동작은 Kl 보스 로직을 재사용하되 HP·공격 밀도를 상향.
새 스프라이트 1장만 제작.

> **구현 시 정정:** 이 프로젝트의 보스 스프라이트는 AI 이미지가 아니라 `tools/draw_assets.ts`가
> 절차적으로 그린다. 따라서 Step 1~2를 nano-img-maker 대신 **`draw_assets.ts`에 `drawFinalBoss()`
> 추가 + `sheet("SPR_BFIN", 96, 96, 1, drawFinalBoss)`**로 대체했다. 나머지 Step은 동일.

**Files:**
- Modify: `tools/draw_assets.ts` — `drawFinalBoss()` + `SPR_BFIN` sheet 추가 → `ASSETS/generated/SPR_BFIN.png`
- Modify: `convert.sh` — `SPR_BFIN` 변환 라인 추가
- Modify: `SRC/BOSS.H` — `BOSS_FIN` enum + 치수/HP/프레임 매크로
- Modify: `SRC/BOSS.CPP` — 스프라이트 로드, `boss_spawn_fin`, half_w/h·fire·anim·render 분기
- Modify: `SRC/GAME.CPP` — `GS_BOSS_INTRO`에서 `stage==4 → boss_spawn_fin()`

**Interfaces:**
- Consumes: `Boss g_boss`, `boss_fire_pattern`, `boss_update`, `boss_render` (BOSS.CPP 기존 구조)
- Produces: `void boss_spawn_fin(void)` (BOSS.H 공개), Stage 4 보스전 성립.

- [ ] **Step 1: 최종보스 PNG 생성**

전역 CLAUDE.md 규칙에 따라 `nano-img-maker` CLI 사용(정사각 1:1). 프레임 애니메이션 없이 단일 프레임(96x96)으로 시작해 스프라이트 코드 크기를 억제한다.

```bash
mkdir -p ASSETS/raw/gen
~/.bun/bin/nano-img-maker -w 96 -H 96 -o ASSETS/raw/gen/SPR_BFIN.png \
  "top-down view of a menacing alien mothership core boss for a 16-bit DOS shmup, dark metallic hull with glowing red central reactor eye, symmetric, facing downward, pixel-art friendly, black background, high contrast"
```

Run: `sips -g pixelWidth -g pixelHeight ASSETS/raw/gen/SPR_BFIN.png`
Expected: 96 x 96.

- [ ] **Step 2: convert.sh에 스프라이트 변환 추가**

`convert.sh`의 "Converting boss sprites..." 섹션(현재 `SPR_BKL` 라인 뒤)에 추가. 단일 프레임이므로 `--grid 1x1`:

```bash
bun tools/mksprite.ts --bin --grid 1x1 "$GEN/SPR_BFIN.png" SPR_BFIN
```

(`$GEN`은 convert.sh가 쓰는 생성 PNG 디렉터리 변수. Step 1의 출력 경로를 `$GEN`에 맞춰 저장할 것. 다르면 경로를 `$GEN`이 가리키는 위치로 조정.)

- [ ] **Step 3: 에셋 변환 실행 + SPR 생성 확인**

Run: `./convert.sh`
Expected: 에러 없음. `ls SRC/SPR_BFIN.SPR` → 파일 존재.

- [ ] **Step 4: BOSS.H에 최종보스 정의 추가**

`BossType` enum:

```c
typedef enum {
    BOSS_TANK2 = 0,
    BOSS_MG    = 1,
    BOSS_KL    = 2
} BossType;
```
→
```c
typedef enum {
    BOSS_TANK2 = 0,
    BOSS_MG    = 1,
    BOSS_KL    = 2,
    BOSS_FIN   = 3
} BossType;
```

`BOSS_KL_*` 매크로 블록 아래에 추가:

```c
/* Final boss (stage 4). 96x96, single frame, highest HP. */
#define BOSS_FIN_W       96
#define BOSS_FIN_H       96
#define BOSS_FIN_HALF_W  48
#define BOSS_FIN_HALF_H  48
#define BOSS_FIN_HP_MAX 520
#define BOSS_FIN_FRAMES   1
```

`boss_spawn_kl` 선언 아래에 추가:

```c
/* Spawn the Stage 4 final boss. */
void boss_spawn_fin(void);
```

- [ ] **Step 5: BOSS.CPP — 스프라이트 로드 + 소유 정리**

파일 스코프 스프라이트 선언(`static Sprite g_spr_kl;` 부근)에 추가:

```c
static Sprite g_spr_fin;
```

기존 `g_have_mg`/`g_have_kl` 플래그와 동일한 패턴으로 `g_have_fin` 플래그 추가(선언부에 `static int g_have_fin = 0;` — 기존 have 플래그가 선언된 곳과 동일 위치).

`boss_init`에서 Kl 로드 뒤에 추가:

```c
    if (spr_load("SPR_BFIN.SPR", &g_spr_fin) == SPR_OK) g_have_fin = 1;
```

`boss_close`에서 Kl 해제 앞/뒤에 대칭으로 추가:

```c
    if (g_have_fin) spr_free(&g_spr_fin);
```

- [ ] **Step 6: BOSS.CPP — half_w/half_h 분기**

`boss_half_w`/`boss_half_h`의 `switch (g_boss.type)`에 `BOSS_FIN` 케이스 추가:

```c
    case BOSS_FIN: return BOSS_FIN_HALF_W;   /* in boss_half_w */
    case BOSS_FIN: return BOSS_FIN_HALF_H;   /* in boss_half_h */
```

(각 함수의 기존 case들과 같은 형식으로 삽입.)

- [ ] **Step 7: BOSS.CPP — boss_spawn_fin 구현**

`boss_spawn_kl` 함수 바로 뒤에 추가:

```c
void boss_spawn_fin(void)
{
    g_boss.active = 1;
    g_boss.type = (u8)BOSS_FIN;
    g_boss.phase = 0;
    g_boss.hp = BOSS_FIN_HP_MAX;
    g_boss.hp_max = BOSS_FIN_HP_MAX;
    g_boss.x = (i16)(PLAY_W / 2);
    g_boss.y = (i16)(-BOSS_FIN_HALF_H);
    g_boss.vx_q4 = (i16)(BOSS_SLIDE_SPEED_Q4 + 12);
    g_boss.t_phase_ms = 0;
    g_boss.fire_cd_ms = 900;
    g_boss.body_frame = 0;
    g_boss.anim_t_ms = 0;
    g_boss.dying = 0;
    g_boss.dying_t_ms = 0;
}
```

- [ ] **Step 8: BOSS.CPP — 공격 밀도 + 애니 프레임 + 발사 쿨다운 분기**

`boss_fire_pattern`의 aggression:

```c
    int extra = 0;
    if (g_boss.type == BOSS_MG) extra = 2;
    else if (g_boss.type == BOSS_KL) extra = 4;
```
→
```c
    int extra = 0;
    if (g_boss.type == BOSS_MG) extra = 2;
    else if (g_boss.type == BOSS_KL) extra = 4;
    else if (g_boss.type == BOSS_FIN) extra = 6;
```

`boss_update`의 프레임 수 switch:

```c
    switch (g_boss.type) {
    case BOSS_MG: frames = BOSS_MG_FRAMES; break;
    case BOSS_KL: frames = BOSS_KL_FRAMES; break;
    default:      frames = BOSS_TANK2_BODY_FRAMES; break;
    }
```
→
```c
    switch (g_boss.type) {
    case BOSS_MG:  frames = BOSS_MG_FRAMES; break;
    case BOSS_KL:  frames = BOSS_KL_FRAMES; break;
    case BOSS_FIN: frames = BOSS_FIN_FRAMES; break;
    default:       frames = BOSS_TANK2_BODY_FRAMES; break;
    }
```

`boss_update`의 발사 쿨다운 base(현재 Kl 분기 뒤)에 FIN 추가:

```c
            u16 base[3] = { 1500, 1500, 900 };
            if (g_boss.type == BOSS_MG) {
                base[0] = 1100; base[1] = 1000; base[2] = 600;
            } else if (g_boss.type == BOSS_KL) {
                base[0] = 900; base[1] = 800; base[2] = 500;
            }
```
→ Kl `else if` 뒤에 추가:
```c
            } else if (g_boss.type == BOSS_FIN) {
                base[0] = 800; base[1] = 700; base[2] = 420;
            }
```

- [ ] **Step 9: BOSS.CPP — 렌더 분기**

`boss_render`의 타입 분기:

```c
    if (g_boss.type == BOSS_MG && g_have_mg) {
        spr_draw_clipped(&g_spr_mg, g_boss.body_frame, sx, sy);
    } else if (g_boss.type == BOSS_KL && g_have_kl) {
        spr_draw_clipped(&g_spr_kl, g_boss.body_frame, sx, sy);
    } else {
```
→ Kl 분기 뒤에 FIN 분기 삽입:
```c
    if (g_boss.type == BOSS_MG && g_have_mg) {
        spr_draw_clipped(&g_spr_mg, g_boss.body_frame, sx, sy);
    } else if (g_boss.type == BOSS_KL && g_have_kl) {
        spr_draw_clipped(&g_spr_kl, g_boss.body_frame, sx, sy);
    } else if (g_boss.type == BOSS_FIN && g_have_fin) {
        spr_draw_clipped(&g_spr_fin, 0, sx, sy);
    } else {
```

- [ ] **Step 10: GAME.CPP — Stage 4 보스 트리거**

`GS_BOSS_INTRO`의 보스 스폰(현재 `GAME.CPP:251-253`):

```c
                if      (g_state.stage == 2) boss_spawn_mg();
                else if (g_state.stage == 3) boss_spawn_kl();
                else                          boss_spawn_tank2();
```
→
```c
                if      (g_state.stage == 2) boss_spawn_mg();
                else if (g_state.stage == 3) boss_spawn_kl();
                else if (g_state.stage == 4) boss_spawn_fin();
                else                          boss_spawn_tank2();
```

- [ ] **Step 11: 빌드 & 검증**

Run: `./build.sh` 후 `./run.sh`
Expected: Stage 4 130초 후 **신규 최종보스**가 등장한다. 스프라이트가 정상 표시되고, 3페이즈로 공격 밀도가 상승하며, 격파 시 죽음 시퀀스(폭발) + 보상 아이템이 나온다. 격파 후 GS_STAGE_CLEAR로 전이(다음은 엔딩 — Task 6).

---

## Task 6: 엔딩 — ST07 재생 + 스태프롤 크롤 + 자동 복귀

최종보스 격파 후 GS_ENDING에서 ST07을 재생하고, 아래→위로 흐르는 스태프롤을 표시한 뒤
자동으로 타이틀(ST06)로 복귀한다. Z로 즉시 스킵 가능.

**Files:**
- Modify: `SRC/GAME.CPP` — GS_STAGE_CLEAR→GS_ENDING 전이 시 ST07 로드, GS_ENDING 케이스(자동 종료 + Z 스킵 → `bgm_menu()`)
- Modify: `SRC/HUD.CPP` — `hud_render_ending()`을 크롤로 재작성

**Interfaces:**
- Consumes: `bgm_menu()` (Task 3), `snd_stop/load/play`, `g_state.gs_t_ms`, `font_puts`, `font_text_width` (FONT.H), `gfx_clear`
- Produces: 엔딩 크롤 총 길이 상수 `ENDING_MS` 공유(HUD와 GAME 양쪽 타이밍 일치용)

- [ ] **Step 1: 엔딩 총 길이 상수 정의**

`HUD.H`(공개 헤더)에 추가하여 GAME.CPP와 HUD.CPP가 공유:

```c
/* Total staff-roll crawl duration (ms). Auto-return to title after this. */
#define ENDING_MS 26000UL
```

- [ ] **Step 2: GS_ENDING 진입 시 ST07 로드**

`GAME.CPP`의 `GS_STAGE_CLEAR`에서 마지막 스테이지 클리어 후 엔딩으로 보내는 분기(현재 `GAME.CPP:344-349` 부근):

```c
                } else {
                    bullet_clear_all_enemy();
                    g_state.gs = GS_ENDING;
                }
```
→
```c
                } else {
                    bullet_clear_all_enemy();
                    snd_stop();
                    snd_load("ST07.VGM");
                    snd_play();
                    g_state.gs = GS_ENDING;
                }
```

- [ ] **Step 3: GS_ENDING 케이스 재작성 (자동 종료 + Z 스킵)**

현재 GS_ENDING(대략):

```c
        case GS_ENDING:
            bg_update(dt);
            if (z_pressed) {
                g_state.gs = GS_TITLE;
                g_state.gs_t_ms = 0;
                snd_stop();
                snd_load("ST00.VGM");
                snd_play();
            }
            gfx_clear(0);
            bg_render();
            hud_render_ending();
            break;
```
→
```c
        case GS_ENDING:
            bg_update(dt);
            /* Z skips, or auto-return once the crawl finishes. */
            if (z_pressed || g_state.gs_t_ms >= ENDING_MS) {
                bgm_menu();
                g_state.gs = GS_TITLE;
                g_state.gs_t_ms = 0;
            }
            gfx_clear(0);
            bg_render();
            hud_render_ending();
            break;
```

- [ ] **Step 4: hud_render_ending()을 스태프롤 크롤로 재작성**

`HUD.CPP`의 `hud_render_ending()` 전체를 교체. `g_state.gs_t_ms`로 y 오프셋을 계산해
각 줄을 아래(y=PLAY_H)에서 위로 스크롤한다. `draw_text_centered(y, str, color)`(기존 static 헬퍼)와
`g_state`를 그대로 사용한다.

```c
void hud_render_ending(void)
{
    /* Staff-roll lines, top-to-bottom in reading order. NULL = blank gap. */
    static const char *lines[] = {
        "라이덴 2 데모",
        0,
        "전 단계 완료",
        "축하합니다",
        0,
        0,
        "제작",
        "게임 디자인",
        "프로그래밍",
        0,
        "음악",
        "OPL2 VGM",
        0,
        "엔진",
        "왓컴 C++ DOS",
        "VGA 모드 13h",
        0,
        0,
        "최종 점수",
        0,
        0,
        0,
        "다시 만나요",
        "THE END",
        0
    };
    const int nlines = (int)(sizeof(lines) / sizeof(lines[0]));
    const int line_h = 16;      /* per-line vertical step */
    const int play_h = 200;

    /* Scroll speed tuned so the whole roll clears within ENDING_MS.
     * total travel = play_h + nlines*line_h; y0 starts at play_h and
     * decreases linearly with time. */
    long total_travel = (long)play_h + (long)nlines * line_h;
    long y0 = (long)play_h - (total_travel * (long)g_state.gs_t_ms) / (long)ENDING_MS;

    int i;
    for (i = 0; i < nlines; i++) {
        int y = (int)(y0 + (long)i * line_h);
        if (y < -line_h || y >= play_h) continue;   /* offscreen cull */
        if (lines[i] == 0) continue;                /* blank gap */
        draw_text_centered(y, lines[i], HUD_TEXT_COLOR);
    }

    /* Render the numeric score just below the "최종 점수" label line.
     * That label is at index L; compute its y and draw the number one
     * line under it. */
    {
        int score_label_idx = 18;   /* index of "최종 점수" above */
        int sy = (int)(y0 + (long)(score_label_idx + 1) * line_h);
        if (sy >= 0 && sy < play_h) {
            int sx = 32 + (256 + 7 * 4) / 2;
            draw_number(sx - 1, sy, g_state.score, 7, 25);
        }
    }
}
```

> 주의: `score_label_idx = 18`은 위 `lines[]`에서 `"최종 점수"`의 실제 인덱스와 반드시 일치해야 한다. `lines[]`를 수정하면 이 인덱스도 함께 갱신한다.

- [ ] **Step 5: 빌드 & 검증**

Run: `./build.sh` 후 `./run.sh`
Expected: Stage 4 보스 격파 → 엔딩에서 **ST07** 재생 + 텍스트가 아래에서 위로 흐른다. 중간에 최종 점수가 함께 스크롤된다. 약 26초 후(또는 Z 즉시) 타이틀로 복귀하며 **ST06**이 다시 재생된다.

- [ ] **Step 6: 크롤 타이밍 미세 조정**

크롤이 음악보다 너무 빨리/느리게 끝나면 `HUD.H`의 `ENDING_MS`를 조정(예: 22000~30000)한다. 조정 후 `./build.sh`→`./run.sh`로 재확인.

---

## 최종 통합 검증 (전체 플레이스루)

- [ ] `./build.sh` 무에러 → `./run.sh`
- [ ] 타이틀 ST06 → Z → Stage1 ST00 → S2 ST01 → S3 ST02 → S4 ST03 → 최종보스 격파 → 엔딩 ST07 크롤 → 타이틀 ST06 복귀
- [ ] 배경은 전 구간 별 스크롤(그리드 없음)
- [ ] 오디오 이상 시 `./sfxtest.sh`로 회귀 확인

## 자기 검토 결과 (spec 대비)

- 메뉴곡 ST06 → Task 3. 스테이지 ST00~03 → Task 3+4. 엔딩 ST07 → Task 6. Stage 4 → Task 4. 최종보스 1신규 스프라이트 → Task 5. 별 스크롤 배경 → Task 2. VGM 반입 → Task 1. **모든 spec 항목이 태스크로 커버됨.**
- 타입/이름 일관성: `bgm_menu`/`bgm_stage`(Task3)는 Task6에서 재사용. `BOSS_FIN`/`boss_spawn_fin`/`g_spr_fin`/`g_have_fin`/`BOSS_FIN_*` 매크로 이름이 Task5 전 스텝에서 일치. `ENDING_MS`는 HUD.H에 정의되어 GAME/HUD 공유.
- 플레이스홀더 없음(모든 코드 스텝에 실제 코드 수록). 단, `lines[]`의 `score_label_idx`는 배열 편집 시 수동 동기화 필요 — 주석으로 명시.
