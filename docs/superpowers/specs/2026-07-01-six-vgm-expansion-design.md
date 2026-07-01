# 6곡 완전 활용 확장 — 설계 스펙

**Date:** 2026-07-01
**Author:** brainstorming session with Claude
**Status:** approved, ready for plan

## 목표 (Goal)

현재 3스테이지 종스크롤 슈팅 프로토타입에 **메뉴 전용 BGM · Stage 4(최종 스테이지) · 엔딩 스태프롤**을
추가하여, 보유한 6개 VGM 파일(ST00~03, ST06, ST07)을 전부 실제로 재생되는 상태로 만든다.
"이미지 리소스 최소화"를 제약으로 두어, 신규 아트는 **최종보스 스프라이트 1개**로 한정한다.

## VGM 최종 매핑

| 용도 | VGM | 신규 여부 | 비고 |
|------|-----|----------|------|
| 메인메뉴 (GS_TITLE) | `ST06.VGM` | 🆕 | 현재 메뉴는 `ST00`을 재생 중 |
| Stage 1 사막 (Desert) | `ST00.VGM` | 기존 | 유지 |
| Stage 2 강 (River) | `ST01.VGM` | 기존 | 유지 |
| Stage 3 우주 (Starfield) | `ST02.VGM` | 기존 | 유지 |
| Stage 4 적 모선 내부 | `ST03.VGM` | 🆕 | 파일은 `SRC/`에 존재하나 미사용 |
| 엔딩 (GS_ENDING) | `ST07.VGM` | 🆕 | 현재 엔딩은 `ST00`을 재생 중 |

**에셋 준비:** `ST03.VGM`은 이미 `SRC/`에 있다. `ST06.VGM`, `ST07.VGM`은
`ASSETS/raw/audio_vgm/`에만 존재하므로 `convert.sh`(또는 동일한 복사 단계)를 통해
`SRC/`로 반입해야 한다. DOS 8.3 대문자 파일명(`ST06.VGM`, `ST07.VGM`)을 유지한다.

## 현재 상태 (baseline)

- `SRC/GAME.CPP`: `#define MAX_STAGES 3`. 상태머신은 `GS_TITLE → GS_STAGE_INTRO →
  GS_PLAYING → GS_BOSS_INTRO → GS_BOSS_FIGHT → GS_STAGE_CLEAR → (다음 스테이지 |
  GS_ENDING)` 흐름. `GS_ENDING`, `GS_GAME_OVER`, `GS_CONTINUE_PROMPT`는 이미 존재.
- BGM 로딩: init에서 `ST00`, 스테이지 진입 시 stage2→`ST01`, stage3→`ST02`, 그 외 `ST00`.
  타이틀 복귀(엔딩/게임오버/컨티뉴 타임아웃) 시 `ST00`.
- `GS_ENDING`: `hud_render_ending()` 단일 텍스트 화면 + Z로 타이틀 복귀.
- `SRC/STAGE.CPP`: `stage1_script`(사막), `stage2_script`(강), `stage3_script`(우주).
  각 ~130초, 4막(Act) 구조, 미드보스 + 메인보스. `stage_init(int)`가 스크립트를 선택.
- 배경: Stage 1/2는 IMG(`IMG_DSRT`, `IMG_RIVR`), Stage 3은 절차적 스타필드(IMG 불필요).

## 변경 설계 (레이어별)

세 개의 깨끗한 모듈 경계로 격리한다.

### 1. 상태/엔진 레이어 — `SRC/GAME.CPP`, `SRC/STATE.H`

- `MAX_STAGES` `3` → `4`.
- **메뉴 BGM 전환:** 초기화 및 모든 "타이틀 복귀" 경로에서 `ST00` → `ST06`.
  대상: init BGM 로드(현재 `GAME.CPP:85`), 엔딩→타이틀 복귀(현재 `GAME.CPP:395`),
  게임오버/컨티뉴 타임아웃으로 `GS_TITLE` 진입하는 경로. 타이틀에 진입하는
  모든 경로가 `ST06`을 재생하도록 일관되게 처리한다.
- **Stage 4 BGM:** 스테이지 진입 BGM 분기에 `stage == 4 → ST03.VGM` 추가
  (현재 `GAME.CPP:340~342`의 if/else 체인 확장).
- **엔딩 BGM:** `GS_ENDING` 진입 시 `snd_stop() → snd_load("ST07.VGM") → snd_play()`.
  진입 1회만 로드하도록 상태 진입 감지(`gs_t_ms` 리셋 시점) 사용.
- **엔딩→타이틀:** 크롤 종료 또는 Z 스킵 시 `GS_TITLE`로 전환하며 `ST06` 재생.
- `STATE.H`의 `u8 stage;` 주석을 `1~3` → `1~4`로 갱신.
- 사운드 조작은 기존 `snd_stop() → snd_load() → snd_play()` 순서 패턴을 그대로 유지한다.

### 2. 콘텐츠 레이어 — `SRC/STAGE.CPP`, `SRC/STAGE.H`

- **`stage4_script` 추가** — 적 모선/요새 내부, 기존 스테이지와 동일한 ~130초 4막 구조.
  - Act 1: 잡몹 웨이브(기존 적 스프라이트 재사용).
  - Act 2: 미드보스 = **기존 보스 스프라이트 재사용**(예: `MG` 또는 `Kl`).
  - Act 3: 밀도 상승 잡몹 + 정예.
  - Act 4: 쿨다운 후 메인보스 = **신규 최종보스**.
- **`stage_init`에 `case 4` 추가** — `g_active_script = stage4_script`.
- **배경:** 전(全) 스테이지 공통 배경을 사용한다. 현재 배경은 스크롤 그리드
  (`SPR_GRID.SPR`)이며 스테이지별로 다르지 않다(사막/강 IMG는 미사용). 본 확장에서
  배경을 **절차적 우주 별 스크롤**로 교체한다(아래 별도 항목). Stage 4는 신규 배경
  작업이 없다.

### 3.5 배경 교체 — 절차적 우주 별 스크롤 (BG.CPP)

- 현재의 스크롤 그리드(`SPR_GRID.SPR`)를 제거하고, **반짝이는 별이 아래로 흐르는
  스타필드**로 교체한다(과거 화면보호기/눈내리기 스타일). 전 스테이지 공통.
- 절차적 생성: BG 모듈 내부 LCG RNG로 별 좌표를 초기화. 신규 이미지 에셋 없음.
- 시차(parallax) 3계층(느림·중간·빠름)에 밝기 차등(회색조 팔레트 1~15 활용),
  프레임마다 소폭 반짝임(twinkle). 화면 밖으로 나간 별은 상단에서 x를 새로 뽑아 재생성.
- `SPR_GRID` 로드/해제 제거. `convert.sh`의 `SPR_GRID` 변환은 남겨도 무해(선택 제거).

### 3. 엔딩 스태프롤 레이어 — `SRC/HUD.CPP`, `SRC/GAME.CPP`

- `hud_render_ending()`을 **아래→위 스크롤 크롤**로 교체.
  - 콘텐츠 순서: 게임 타이틀 → 크레딧(개발 / 음악 / 엔진) → 축하 메시지 → `THE END`.
  - `gs_t_ms` 기반으로 y 오프셋을 계산해 프레임마다 위로 이동.
  - `ST07` 재생 길이에 맞춰 자연스럽게 진행되도록 스크롤 속도를 조정.
  - 크롤이 끝(THE END가 중앙 정착)나면 자동으로 `GS_TITLE` 복귀. **Z로 즉시 스킵** 가능.
- 한글+영문 폰트(`FONT`)를 사용하며 신규 이미지는 사용하지 않는다.

### 4. 에셋 레이어 — 신규 최종보스 1개

- **정정:** 이 프로젝트의 모든 스프라이트는 `tools/draw_assets.ts`가 절차적으로 그리는
  와이어프레임 아트다(보스 포함, `drawLargeBoss()` 등). 따라서 최종보스도 외부 이미지
  생성기가 아니라 **`draw_assets.ts`에 절차적 드로잉 함수를 추가**하여 만든다(스타일 일치·
  파이프라인 통합·재현성).
- `draw_assets.ts`에 `drawFinalBoss()` 추가 → `sheet("SPR_BFIN", 96, 96, 1, drawFinalBoss)`.
  붉은 원자로 코어 + 측면 포드 + 트윈 배럴의 요새형 모선(단일 프레임).
- `convert.sh`에 `mksprite --bin --grid 1x1 "$GEN/SPR_BFIN.png" SPR_BFIN` 추가 → `SRC/SPR_BFIN.SPR`.
- 그 외 신규 아트는 없다. 잡몹·미드보스·배경·파워업·폭발은 모두 기존 에셋 재사용.

## 리스크 / 주의사항

- **컴파일 스프라이트 크기:** 최종보스가 크면 프레임당 x86 기계어 코드가 커진다.
  프레임 수를 최소화하고 투명 픽셀 비율을 높여 코드 크기를 억제한다.
- **ISR/사운드 순서:** `timer_init() → input_init() → snd_init()` 및 역순 종료를
  건드리지 않는다. BGM 교체는 재생 중 `snd_stop→snd_load→snd_play`만 사용.
- **엔딩 크롤 타이밍:** ST07 길이와 크롤 총 높이가 어긋나면 음악이 먼저 끝나거나
  텍스트가 잘릴 수 있다. 스크롤 속도를 실측으로 맞춘다(빌드→실행 확인).
- **VGM 반입 누락:** `ST06.VGM`/`ST07.VGM`가 `SRC/`에 없으면 `snd_load` 실패로
  무음이 된다. 빌드 전 반입을 확인한다.

## 테스트 (검증 방법)

자동 테스트 스위트는 없다. `./build.sh` → `./run.sh`로 실측 검증한다.

1. 타이틀에서 `ST06`이 재생되는지.
2. Stage 1~4 진입 시 각각 `ST00/ST01/ST02/ST03`이 재생되는지.
3. Stage 4 클리어(최종보스 격파) → 엔딩 진입 시 `ST07`이 재생되고 크롤이 흐르는지.
4. 크롤 자동 종료 및 Z 스킵 후 타이틀 복귀 시 `ST06`으로 되돌아오는지.
5. 오디오/ISR 인접 변경은 없으나, 사운드 이상 시 `./sfxtest.sh`로 회귀 확인.

## 범위 밖 (YAGNI)

- 신규 배경 IMG, 신규 잡몹/미드보스 스프라이트.
- 최종보스 격파 컷신(폭발 연출 재활용 이상의 별도 연출).
- 스테이지 셀렉트, 난이도 선택, 세이브/랭킹.
