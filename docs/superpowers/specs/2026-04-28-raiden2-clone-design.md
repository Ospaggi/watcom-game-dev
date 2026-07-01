# Raiden 2 클론 — 종스크롤 탄막 슈팅 게임 설계 문서

- **작성일**: 2026-04-28
- **대상 엔진**: Watcom C/C++ + DOS Mode 13h (이 리포지토리)
- **단계**: 디자인 (구현 전)
- **상태**: 승인 대기

---

## 1. 한 줄 요약

DOS Mode 13h(320x200x256)에서 동작하는 라이덴2 클론 종스크롤 탄막 슈팅 게임. 3 스테이지 + 보스, V/L/P 무기 + 호밍 + 봄(오토봄), 색순환 P 파워업, 누적 메달, 공중/지상 분리, 작은 히트박스.

## 2. 확정된 기획 결정

| 항목 | 값 |
|---|---|
| 화면 | 320x200, 플레이 영역 256x200 (가운데 pillarboxed), 좌우 32px HUD |
| 플레이어 수 | 1P |
| 메인 무기 | Vulcan / Laser / Plasma — P 색순환으로 전환 |
| 서브웨폰 | Homing Missile (Z 홀드 시 자동 발사, 파워 레벨 없음) |
| 봄 | 오토봄 + 화면 클리어 + 2.5초 무적, 시작 2개, 최대 7개 |
| 잔기 | 시작 3, 1UP at 1,000,000점, 그 자리 부활 |
| 콘티뉴 | 무한 (점수 0 리셋, 진행 위치 유지) |
| 메달 | 누적 (100→200→500→1k→2k→5k→10k), 화면 밖으로 흘리면 한 단계 강하 |
| 적 평면 | 공중 + 지상 (Vulcan/Plasma=둘 다, Laser=공중만) |
| 히트박스 | 플레이어 4x4 중앙 (스프라이트 16x48 내) |
| 스테이지 | Desert → River → Space (3면 배경 procedural starfield) |
| 보스 | 스테이지당 1, 단일 스프라이트 (Tank2 / MG / Kl) |

## 3. 화면 레이아웃 (320x200)

```
+------+-------------------+------+
| HUD  |                   | HUD  |
| L 32 |  Play 256x200     | R 32 |
|      |                   |      |
| 1UP  |  배경 스크롤      | BOMB |
|0123456|                   |  x3  |
|      |  플레이어         |      |
| WPN  |  적/탄/메달       | MED  |
|VULC 3|                   |12500 |
|      |                   |      |
| LIVES|                   | STG  |
| ★★★ |                   |  1-1 |
+------+-------------------+------+
   32         256              32
```

**왼쪽 HUD (32x200)**: 점수(7자리), 무기명+레벨, 잔기 아이콘.
**오른쪽 HUD (32x200)**: 봄 보유 수, 현재 메달 가치, 스테이지 표시.
**플레이 영역 (256x200)**: 배경 PNG 그대로 + 패럴랙스 + 플레이어 이동 범위 x=16~240, y=16~190.

## 4. 플레이어 시스템

### 4.1 우주선

- 스프라이트: `ASSETS/raw/ansimuz_spaceship/.../ship.png` (80x48, 5프레임 추정 — 구현 시 검증)
- 이동: 2 px/frame, 8방향 (대각선 보정 X — 라이덴 정통)
- 화면 경계: x=16~240, y=16~190
- 히트박스: 중앙 4x4 (cockpit 위치)
- 무적: 부활 후 2초 (깜빡임), 봄 후 2.5초

### 4.2 입력

| 키 | 동작 |
|---|---|
| ← → ↑ ↓ | 8방향 이동 |
| Z (홀드) | 메인 무기 발사 (자동 연사) + 호밍 자동 발사 |
| X | 봄 |
| F1 | 일시정지 / 재개 (토글) |
| ESC | 게임 종료 (TITLE로 복귀, PLAYING 중에는 확인 프롬프트) |

### 4.3 메인 무기 (V / L / P, 레벨 1~4)

#### Vulcan (V, 빨강) — 공중 + 지상
- 발사 간격: 80ms (~12 shot/sec), 데미지 1/탄
- L1: 정면 1탄 / L2: 정면 2탄 평행 / L3: + ±15° 사선 2탄 / L4: + ±30° 2탄 (총 6탄 부채꼴)

#### Laser (L, 파랑) — 공중만 (지상 통과)
- 발사 간격: 50ms (사실상 연속), 데미지 2/탄
- L1: 1빔 (2px 폭) / L2: 1빔 (4px 폭) / L3: + 좌우 휘는 2빔 / L4: 더 많은 휘는 빔

#### Plasma (P, 보라) — 공중 + 지상, 약한 호밍
- 발사 간격: 100ms, 데미지 3/탄
- L1: 1볼 (4x4) / L2: 1볼 (6x6) / L3: 2볼 좌우 분기 / L4: 3볼 (전방 + 좌우)
- **참고**: 라이덴2 정통 Plasma는 "엿가락 레이저"인데, 단순화를 위해 약한 호밍 볼로 대체. 진짜 엿가락은 v2.

### 4.4 호밍 미사일

- 자동 발사 (Z 홀드 시 메인과 동시), 발사 간격 400ms
- 가장 가까운 공중 적 자동 추적, 포물선 궤적
- 한 번에 최대 4발, 데미지 2/발
- 파워 레벨 없음 (시작부터 풀파워)

### 4.5 봄

- **수동**: X → 봄 1 소비 → 화면 폭발 → 모든 적탄 소거 + 일반 적 즉사 + 보스 200 데미지 → 2.5초 무적
- **오토봄**: 피격 시 봄 ≥ 1이면 자동 발동
- 시작 2개, 최대 7개

### 4.6 사망 / 부활 / 게임오버

- 잔기 ≥ 1: 그 자리 부활, 무적 2초
- 잔기 0 → GAME OVER → CONTINUE? 10초 카운트다운 → Z 누르면 점수 0 리셋, 잔기 3 + 봄 2 재개

## 5. 적 시스템

### 5.1 평면 분리

```c
typedef enum {
    PLANE_AIR = 0,    // 플레이어와 같은 Z, 충돌 가능, 모든 무기 타격
    PLANE_GROUND = 1  // 배경 위, 충돌 X, Laser 통과
} EnemyPlane;
```

### 5.2 공중 적 (Air) — 21종

| 코드네임 | 자산 | 크기 | 프레임 | 역할 |
|---|---|---|---|---|
| `E_POPCORN_A` | `pop` | 23x31 | 4 | 1면 popcorn |
| `E_POPCORN_B` | `Pop0` | 22x35 | 4 | 1면 popcorn 변종 |
| `E_FIGHTER_R` | `rn` | 28x42 | 4 | 1면 V형 편대 |
| `E_FIGHTER_S` | `rs` | 22x30 | 3 | 1면 빠름 |
| `E_FIGHTER_V` | `rv` | 31x30 | 4 | 1면 일반 |
| `E_BLUE` | `Bl` | 31x38 | 4 | 2면 청색 사선 |
| `E_CYAN` | `CI` | 31x38 | 5 | 2면 청록 |
| `E_DROP` | `dp` | 33x33 | 4 | 2면 드롭형 |
| `E_HEAVY_B/G/Y` | `hb`/`hg`/`hy` | ~30x30 | 3 | 2면 중량형 3변종 |
| `E_AF` | `AF` | 34x51 | 4 | 3면 advanced fighter |
| `E_SS` | `ss` | 40x47 | 4 | 3면 작은 우주선 |
| `E_MED_C/S/V/T/SS/PRP/PP` | `MC`/`MS`/`MV`/`MT`/`MSS`/`prp`/`pp` | 40~70px | 4~8 | 중형 적 7변종 |
| `E_POWER_DELIVER` | `pwup_ship` | 34x51 | 4 | 파워업 운반선 (P 1개 드롭) |

**총 공중 적 종류**: 21종 (popcorn/fighter 변종 ~10, 중형 7, 특수 4). 모든 종이 1면에 등장하지 않고 스테이지별 분배.

### 5.3 지상 적 (Ground)

| 코드네임 | 자산 | 크기 |
|---|---|---|
| `G_TANK_S` | `Tank1` (32x32 본체 + head) | 32x32 |
| `G_TANK_RD` | `tnk_rd` + `tnk_rdb` | 21x23 |
| `G_TURRET_BASE` | `Turret_base` (5프레임) | 가변 |
| `G_TURRET_SILVER` | `Turre_head_silver` | 27x20 |

지상 적은 배경과 같이 스크롤 (자체 이동도 가능). 폭발 후 `tank1_body_rip1~6.png` 6프레임 잔해 잠시 표시.

### 5.4 적 데이터 구조

```c
typedef struct {
    u8  active;          // 0 = 슬롯 비어있음
    u8  type;            // E_*
    u8  plane;           // PLANE_AIR / PLANE_GROUND
    u8  ai_id;           // PAT_*
    i16 x, y;            // 위치 (플레이 영역 좌상단 기준, signed)
    i16 vx, vy;          // 속도 (1/16 px 단위 서브픽셀)
    u16 hp;              // 체력
    u16 t_spawn;         // 스폰 후 경과 ms
    u8  fire_cooldown;
    u8  frame;           // 애니 프레임
    u16 ai_state;        // AI FSM용 일반 상태
} Enemy;

#define MAX_ENEMIES 32
static Enemy g_enemies[MAX_ENEMIES];
```

### 5.5 AI 패턴 (`PAT_*`)

| ID | 동작 |
|---|---|
| `PAT_STRAIGHT_DOWN` | 그냥 아래로 직진, 일정 간격 직진탄 |
| `PAT_SINE` | 사인파 좌우 흔들면서 아래로 |
| `PAT_AIM_PLAYER` | 직진하다가 멈추고 플레이어 조준탄 N발 |
| `PAT_DIVE` | 위에서 등장 → 플레이어 향해 가속 → 화면 끝까지 |
| `PAT_FORMATION_V` | V자 편대 (스폰 시 5대 동시), 동시 발사 |
| `PAT_HOVER_LR` | 위쪽에 머물며 좌우 이동, 주기 발사 |
| `PAT_GROUND_STATIC` | 지상 정지, 플레이어 조준탄 |
| `PAT_GROUND_MOVE` | 지상 이동, 조준탄 |
| `PAT_DELIVER_POWERUP` | 화면 가로질러 빠르게 통과, P 1개 드롭 |
| `PAT_SCRIPTED` | 외부 스크립트 (보스용) |

각 패턴은 `enemy_ai_PAT_X(Enemy* e, u32 dt_ms)` 함수.

## 6. 탄 시스템

```c
typedef struct {
    u8  active;
    u8  type;            // BUL_*
    u8  color_idx;       // 팔레트 인덱스
    i16 x, y;            // 정수 px (서브픽셀 X, 단순)
    i16 vx, vy;          // 1/16 px/frame
    u8  size;            // 4 / 6 / 8 / 16
} Bullet;

#define MAX_PLAYER_BULLETS  64
#define MAX_ENEMY_BULLETS   256
static Bullet g_pbullets[MAX_PLAYER_BULLETS];
static Bullet g_ebullets[MAX_ENEMY_BULLETS];
```

**탄 종류 (`BUL_*`)**:
| 종류 | 크기 | 사용처 |
|---|---|---|
| `BUL_DOT` / `BUL_RICE` / `BUL_BIG` | 4x4 / 6x6 / 8x8 | 적탄 (3 단계) |
| `BUL_AIMED` | 6x6 | 플레이어 조준탄 |
| `BUL_LASER_SHORT` | 4x10 | 보스 발사 빔 |
| `BUL_PLAYER_VULCAN` | 2x6 | 플레이어 vulcan |
| `BUL_PLAYER_LASER` | 2~4x16 | 플레이어 laser |
| `BUL_PLAYER_PLASMA` | 6x6 | 플레이어 plasma |
| `BUL_HOMING` | 4x8 | 플레이어 호밍 |

탄 그리기: 작은 건 `gfx_fill_rect`, 큰 건 작은 스프라이트.

## 7. 충돌 검사 (매 프레임 60Hz)

| 충돌 종류 | 비용 | 비고 |
|---|---|---|
| player_bullet vs air_enemy | O(64×32) = 2048 | AABB |
| player_bullet vs ground_enemy | O(64×N_ground) | Laser는 스킵 |
| enemy_bullet vs player_hitbox(4x4) | O(256) | AABB |
| player_sprite vs air_enemy_sprite | O(32) | 정면 충돌 |

플레이어 히트박스만 4x4. 나머지는 스프라이트 외곽 박스.

## 8. 폭발 / 이펙트

```c
typedef struct {
    u8 active; u8 frame;
    i16 x, y; u16 t_ms;
} Effect;
#define MAX_EFFECTS 24
```

- 일반 적 사망: `explosion.png` (5프레임 16x16) 1회
- 큰 적: 폭발 2~3개 동시 (랜덤 오프셋)
- 보스 사망: 다중 폭발 1초간 + 화면 흔들림

## 9. 아이템 시스템

```c
typedef struct {
    u8  active;
    u8  type;        // ITEM_POWER / ITEM_BOMB / ITEM_HOMING / ITEM_MEDAL_S/L / ITEM_1UP
    u8  pw_color;    // ITEM_POWER일 때 PW_VULCAN/LASER/PLASMA
    u8  frame;       // 회전 애니 (0~14)
    i16 x, y;
    i16 vx, vy;
    u16 t_spawn;
    u16 medal_value; // 메달일 때 가치 (스폰 시점 누적값 캡처)
} Item;

#define MAX_ITEMS 32
```

### 9.1 P 아이콘 (색순환)

자산: `pwupitem_0~14.png` (15프레임, 11x27)

- 위에서 등장 → 아래로 흘러감 (vy = +0.5 px/frame), 좌우 sine 흔들림
- **플레이어 탄 맞음 → 색순환** (Vulcan→Laser→Plasma→Vulcan...), 0.3초 쿨다운
- **플레이어 픽업 → 그 색의 무기로 전환 + 파워 레벨 +1 (캡 4) + 10000점**
- 화면 밖으로 흘러가면 사라짐
- 색 표시: `gfx_blit_recolor()` 신규 함수로 회색 톤 → 색램프 매칭

### 9.2 봄 아이템 (B)

- 시각화: `pwupitem` 1프레임 + 노란색 recolor + ENG.FNT 'B' 글자 오버레이
- 위에서 등장, vy = +0.7
- 픽업 → 봄 +1 (캡 7) + 5000점, 풀이면 50000점만

### 9.3 호밍 아이콘 (H) — 단순화

- 호밍 미사일 자체는 처음부터 풀파워이므로 픽업은 **보너스 점수**로 활용
- 픽업 → 30000점

### 9.4 메달 시스템 (Raiden 2 누적)

```c
static const u32 medal_value_table[] = {
    100, 200, 500, 1000, 2000, 5000, 10000  // 7단계
};
static u8 g_medal_level = 0;
```

- 작은 메달: 8x8 노랑 (런타임 `gfx_fill_rect`로 그림, PNG 불필요)
- 큰 메달: 12x12 금색 (작은 적 = 작은 1, 중형 = 큰 1, 보스 = 큰 5, 지상 = 작은 2)
- 메달 픽업마다 `g_medal_level += 1` (캡 6)
- 화면 밖으로 흘리면 `g_medal_level -= 1` (최소 0)
- 죽으면 0으로 리셋
- 봄 발동 시 화면 위 모든 메달 자동 흡수

### 9.5 1UP

- 시각화: `ship.png` 1프레임 축소 + ENG.FNT '1UP' 글자
- 1면당 1개 정도 숨겨진 위치에서 등장 (스크립트 명시)
- 픽업 → 잔기 +1 (캡 9) + 30000점

### 9.6 점수 시스템

| 액션 | 점수 |
|---|---|
| 적 격파 (popcorn) | 100 |
| 적 격파 (중형) | 500 |
| 적 격파 (대형) | 2000 |
| 보스 격파 | 50000 |
| 메달 픽업 | 100~10000 (단계별) |
| P / B / H / 1UP 픽업 | 10000 / 5000 / 30000 / 30000 |
| 보스 클리어 봄 보너스 | 50000 × 잔여 봄 |
| 보스 클리어 잔기 보너스 | 100000 × 잔여 잔기 |

**1UP 임계값**: `g_extend_score = 1,000,000`, 점수 ≥ extend 시 잔기 +1, extend += 1,000,000.

## 10. 글로벌 게임 상태

```c
typedef struct {
    u32 score;
    u32 next_extend;
    u8  lives;
    u8  bombs;
    u8  weapon;             // PW_VULCAN/LASER/PLASMA
    u8  weapon_level;       // 1~4
    u8  medal_level;        // 0~6
    u8  invincible_frames;
    u8  stage;              // 1~3
    u8  continues_used;
    u32 stage_t_ms;
} GameState;

extern GameState g_state;
```

## 11. 스테이지 데이터 + 보스

### 11.1 스폰 스크립트 구조

```c
typedef enum {
    EVT_END = 0,
    EVT_SPAWN, EVT_BG_CHANGE, EVT_BOSS_INTRO,
    EVT_PAUSE_SCROLL, EVT_RESUME_SCROLL, EVT_DROP_ITEM
} EventType;

typedef struct {
    u32 t_ms;
    u8  type;
    u8  arg1, arg2;
    i16 x;
    u8  ai_pattern;
    u8  count;
} StageEvent;

extern const StageEvent stage1_script[];
extern const StageEvent stage2_script[];
extern const StageEvent stage3_script[];
```

### 11.2 스테이지별 콘텐츠 (개략 ~90초씩)

#### Stage 1 — Desert
| 구간 | 시간 | 내용 |
|---|---|---|
| 인트로 | 0~10s | 작은 적 1~2종 직진 |
| 메인 | 10~40s | popcorn + V자 편대 + 지상 탱크 등장 |
| 보너스 | 40~50s | 파워업 운반선 (P 1개) |
| 압박 | 50~75s | 중형 적 + 지상 터렛 |
| 보스 | 80s~ | **Tank2** (70x70 본체 + tank2_head 회전) |

#### Stage 2 — River
| 구간 | 시간 | 내용 |
|---|---|---|
| 인트로 | 0~10s | 빠른 적 사선 등장 |
| 메인 | 10~50s | popcorn 더 많이 + 중형 8프레임 회전 |
| 1UP 비밀 | 30s | 특정 popcorn 5대 모두 격파 시 1UP |
| 압박 | 50~75s | 큰 적 + 강 위 터렛 |
| 보스 | 80s~ | **MG** (84x74) — 좌우 슬라이드 + 화염 탄막 |

#### Stage 3 — Space (배경 procedural starfield)
| 구간 | 시간 | 내용 |
|---|---|---|
| 인트로 | 0~10s | 빠른 사선 |
| 메인 | 10~40s | 회전체 + 대형 우주선 |
| 메달 러쉬 | 40~50s | popcorn 30대 동시 |
| 압박 | 50~75s | 중형 다수 + sub-boss |
| 최종 보스 | 80s~ | **Kl** (99x83) — 다단계 패턴 (3 페이즈) |

### 11.3 보스 시스템

```c
typedef struct {
    u8  active;
    u8  type;            // BOSS_TANK2 / BOSS_MG / BOSS_KL
    u8  phase;           // 0~3
    u32 hp;              // Tank2=2000, MG=3500, Kl=6000
    u32 hp_max;
    i16 x, y;
    u16 t_phase_ms;
    u16 ai_state;
    u8  fire_cooldown;
    u8  frame;
} Boss;

static Boss g_boss;
```

**페이즈 전환**:
- HP > 66% → Phase 0 (간단한 패턴)
- HP > 33% → Phase 1 (탄막 추가)
- HP ≤ 33% → Phase 2 (광폭화)
- HP = 0 → 폭발 시퀀스 1초 → STAGE_CLEAR

**Tank2 (1면) 패턴 예시**:
| Phase | HP | 동작 |
|---|---|---|
| 0 | 100~66% | 좌우 슬라이드, 헤드가 플레이어 추적, 1.5초마다 조준탄 3발 |
| 1 | 66~33% | + 좌우 끝에서 부채꼴 탄 5발 추가 |
| 2 | 33~0% | + 화면 가로지르는 가속 돌진 (3초마다), 탄 빈도 2배 |

## 12. 게임 상태머신

```c
typedef enum {
    GS_TITLE = 0,
    GS_STAGE_INTRO,
    GS_PLAYING,
    GS_BOSS_INTRO,
    GS_BOSS_FIGHT,
    GS_STAGE_CLEAR,
    GS_GAME_OVER,
    GS_CONTINUE_PROMPT,
    GS_ENDING,
} GameStateId;
```

**전이**:
```
TITLE  --[Z]-->  STAGE_INTRO  --[1.5s]-->  PLAYING
PLAYING  --[스크립트 끝]-->  BOSS_INTRO  --[3s]-->  BOSS_FIGHT
BOSS_FIGHT  --[보스 사망]-->  STAGE_CLEAR  --[3s]-->  STAGE_INTRO (다음 스테이지)
                                                 \--[3면이면]-->  ENDING
PLAYING/BOSS  --[잔기 0]-->  GAME_OVER  --[2s]-->  CONTINUE_PROMPT
CONTINUE_PROMPT  --[Z]-->  PLAYING (점수 0, 잔기 3)
                  \--[10s 타임아웃]-->  TITLE
ENDING  --[페이드 아웃]-->  TITLE
```

## 13. 사운드

기존 SFX(.SFX) + SOUND(VGM) 활용.

### 13.1 BGM (VGM × 4) — 모두 보유

| 슬롯 | 파일 | 용도 |
|---|---|---|
| BGM_TITLE_S1 | `ASSETS/raw/audio_vgm/ST00.vgm` (63 KB) | 타이틀 + 1면 (Desert) |
| BGM_S2 | `ASSETS/raw/audio_vgm/ST01.vgm` (92 KB) | 2면 (River) |
| BGM_S3 | `ASSETS/raw/audio_vgm/ST02.vgm` (66 KB) | 3면 (Space) |
| BGM_BOSS | `ASSETS/raw/audio_vgm/ST03.vgm` (74 KB) | 모든 보스전 공통 |

→ 곡 분배는 청취 후 재배정 가능. 백업으로 `SRC/TYRIAN.VGM` 유지.
→ `convert.sh`에 ST00~ST03 → `SRC/`로 복사 또는 직접 참조 추가 필요.

### 13.2 SFX (8~10개 필요, 현재 1개)

| 사운드 | 종류 |
|---|---|
| 메인 무기 발사 V/L/P | SFX 짧음, 무기별 다른 음 |
| 호밍 발사 | SFX 짧음 |
| 적 폭발 (작은/큰) | SFX |
| 봄 발동 | SFX 가장 긴 음 |
| 메달/P/B/H/1UP 픽업 | SFX 각각 |

**현재 보유**: `SRC/FIRE.SFX` 1개. **부족**: 7~9개 SFX 추가 필요. 프로토타입 단계는 기존 1개를 발사음으로 임시 사용 → 후속 이터레이션에서 SFX 자산 보강.

## 14. 아키텍처

### 14.1 모듈러 C-스타일 + 데이터 테이블

라이덴2 같은 데이터-주도 슈팅게임에 자연스럽고, Watcom 제약(no STL/exceptions/RTTI) 안에서 깨끗.

### 14.2 파일 맵

#### 엔진 (기존, 수정 최소)

| 파일 | 변경 |
|---|---|
| `GFX.CPP/H` | **신규**: `gfx_blit_recolor()` (P 색순환) |
| `INPUT/TIMER/SOUND/SFX/SPRITE/IMG/FONT.CPP/H` | 변경 없음 |

#### 게임 (신규, 약 4400 줄)

| 파일 | 줄수 | 책임 |
|---|---|---|
| `GAME.CPP` | ~250 | 메인 루프, FSM 디스패치, init/shutdown |
| `STATE.H` | ~50 | 전역 상태 정의 |
| `PLAYER.CPP/H` | ~400 | 플레이어 구조체, 이동, 무기, 봄, 무적 |
| `WEAPON.CPP/H` | ~250 | 무기별 발사 함수 |
| `ENEMY.CPP/H` | ~500 | Enemy 구조체, AI 디스패치, 종류별 데이터 |
| `ENEMY_AI.CPP/H` | ~400 | 패턴 함수 약 10개 |
| `BULLET.CPP/H` | ~300 | 탄 풀, 충돌 검사 |
| `ITEM.CPP/H` | ~250 | 아이템 풀 + 처리 |
| `STAGE.CPP/H` | ~150 | 스테이지 디스패처, 배경 스크롤 |
| `STAGE1_DATA.CPP` | ~200 | `stage1_script[]` |
| `STAGE2_DATA.CPP` | ~200 | `stage2_script[]` |
| `STAGE3_DATA.CPP` | ~200 | `stage3_script[]` |
| `BOSS.CPP/H` | ~400 | 보스 FSM, 페이즈 전환 |
| `HUD.CPP/H` | ~200 | 좌우 HUD 렌더링 |
| `SCORE.CPP/H` | ~100 | 점수, 1UP, 메달 가치 |
| `EFFECT.CPP/H` | ~150 | 폭발 풀 |
| `BG.CPP/H` | ~150 | 배경 스크롤 + procedural 스타필드 |
| `RECOLOR.CPP/H` | ~50 | 팔레트 매핑 헬퍼 |

`WCL386 *.CPP -fe=GAME.EXE`이 모두 잡음.

### 14.3 데이터 흐름 (한 프레임)

```
main loop:
  input_poll()
  dt_ms = timer_update()
  game_update(dt_ms):
    case GS_PLAYING:
      stage_advance(dt_ms)         // 스폰 스크립트 처리
      bg_update(dt_ms)
      player_update(input, dt_ms)
      weapon_fire_if_needed()
      enemy_update_all(dt_ms)
      bullet_update_all(dt_ms)
      item_update_all(dt_ms)
      effect_update_all(dt_ms)
      collision_check_all()
      score_check_extend()
  game_render():
    bg_render()
    item_render_below()            // 메달 등 (적 아래)
    enemy_render_ground()
    bullet_render_player()
    enemy_render_air()
    player_render()
    bullet_render_enemy()
    item_render_above()            // P/B/H 등
    boss_render()
    effect_render()
    hud_render()                   // 좌우 HUD (마지막)
  gfx_present()                    // vsync + flip
```

## 15. 자산 파이프라인

### 15.1 스프라이트 (`tools/mksprite.ts`)

기존 `mksprite.ts`는 시트 입력 가정. jinvorionstg 적은 **개별 파일** 분리 → **전처리 필요**:
- `tools/montage_frames.sh` (신규) — ImageMagick `montage`로 프레임들을 가로 시트로 합침 → `mksprite.ts` 입력
- 그 후 `--bin --grid NxR`로 .SPR 생성

| 원본 | 출력 |
|---|---|
| `ship.png` 80x48 (5 frames) | `SPR_PSHP.SPR` |
| `pop_0~3.png` (4 PNG) | montage → `SPR_EPOP.SPR` |
| `Tank2_1~3 + tank2_head` (4 PNG) | montage → `SPR_BTNK.SPR` |
| ... | (적 종류마다) |

**팔레트 매칭**: `mksprite.ts`가 `palette.json` 기반 256색 인덱스 매칭. ansimuz vs jinvorionstg 색감 차이 발생 가능 → 변환 후 시각 검수.

### 15.2 배경 (`tools/mkimg.ts`)

| 원본 | 출력 |
|---|---|
| `desert-backgorund.png` 256x272 | `IMG_DSRT.IMG` |
| `clouds.png` 256x103 | `IMG_DCLD.IMG` |
| `clouds-transparent.png` 256x103 | `IMG_DCLD2.IMG` |
| `River/PNG/background.png` 256x320 | `IMG_RIVR.IMG` |
| `River/PNG/props.png` 111x53 | `IMG_RPRP.IMG` |

### 15.3 신규 자산 (자체 제작 — PNG 0개 추가)

| 자산 | 방법 |
|---|---|
| 메달 (8x8, 12x12) | 런타임 `gfx_fill_rect` |
| 봄 'B' (16x16) | `pwupitem` recolor + ENG.FNT 'B' 오버레이 |
| 호밍 'H' (16x16) | 동일 |
| 1UP (16x8) | `ship.png` 1프레임 + ENG.FNT '1UP' |
| 별 (3면) | 코드 (현 GAME.CPP 별 코드 재활용) |
| 보스 큰 폭발 | 기존 `explosion.png` 다중 + 흰 원 페이드 |

### 15.4 사운드

- VGM **4곡 보유** (`ASSETS/raw/audio_vgm/ST00~ST03.vgm`) — 13.1 참고
- SFX **1개 보유** (`SRC/FIRE.SFX`) — 7~9개 추가 필요 (다음 이터레이션)
- `convert.sh`에 `ASSETS/raw/audio_vgm/ST*.vgm` → `SRC/` 복사 단계 추가 필요

## 16. 성능 예산

**프레임당 최대 부하 추정** (worst case):

| 항목 | 개수 | 픽셀 |
|---|---|---|
| 배경 풀스크린 | 1 | 64,000 |
| Clouds 패럴랙스 ×2 | 2 | 52,736 |
| 적 스프라이트 (compiled) | 32 | ~30,000 |
| 플레이어 탄 | 64 | ~768 |
| 적 탄 | 256 | ~6,144 |
| 아이템 | 32 | ~6,000 |
| 폭발 | 24 | ~6,144 |
| HUD | 1 | 12,800 |
| **합계** | | **~178,000 px** |

256색 1B/px → 178KB/frame. 30fps = 5.3 MB/sec, 60fps = 10.7 MB/sec.

**1차 목표 30 fps 안정**, 여유 나오면 60 fps.

**최적화 카드**:
1. Dirty rect (HUD는 변경 시만 재그림)
2. Compiled sprite (이미 사용중, 투명 픽셀 스킵)
3. 화면 밖 탄 pre-clip (이미 hoist됨)

## 17. 테스트 전략

자동 테스트 어려움 → 단계적 수동 검증.

| 단계 | 검증 내용 | 방법 |
|---|---|---|
| 컴파일 | 모든 모듈 link OK | `./build.sh` BUILD.LOG |
| 시각 | 스프라이트 컬러/프레임 | DOSBox 실행 → `screencap.sh` |
| 플레이테스트 | 게임 진행 가능 | 직접 플레이 |
| 회귀 | 기존 데모 무손상 | 별도 GAME.CPP로 보존 |

**디버그 빌드 키 (`#ifdef DEBUG`)**:
- F1 = 일시정지, F2 = 무적 토글, F3 = 풀파워+봄 7
- F4 = 다음 스테이지, F5 = HP 표시 ON/OFF

## 18. 마일스톤 (구현 순서)

> **스코프 주의**: 본 스펙은 작업량이 큼 (~4400줄, 27 파일). 단일 implementation plan으로 모두 커버하기보다 **마일스톤별로 plan을 나누는 것이 자연스러움**. 첫 plan은 마일스톤 1~7 (1면 풀 플레이 가능 상태)까지를 대상으로 권장. 마일스톤 8~14는 후속 plan.

상세는 plan 단계에서 결정. 개략:
1. 자산 변환 파이프라인 (PNG → SPR/IMG)
2. PLAYER + 기본 화면 (배경 1장 + 이동만)
3. WEAPON + BULLET 풀 (Vulcan만)
4. ENEMY + ENEMY_AI (popcorn 1종 + 1 패턴)
5. 충돌 + 점수 + 폭발
6. ITEM + 메달 + P 색순환
7. 봄 + 무적 + 사망/부활
8. 무기 L/P 추가 + 호밍
9. STAGE 시스템 + 1면 풀 콘텐츠
10. 보스 시스템 + 1면 보스
11. HUD 풀
12. FSM (TITLE / STAGE_INTRO / STAGE_CLEAR / GAME_OVER / CONTINUE)
13. 2면 + 3면 콘텐츠 + 보스
14. 사운드 통합 + 밸런싱

---

## 19. Open Questions / 후속 작업

- ship.png 80x48의 정확한 프레임 분할 (구현 첫 작업으로 PNG 직접 검수)
- SFX 7~9개 추가 자산 확보 (현재 발사음 1개만 보유) — 임시로 1 SFX를 발사 전체에 공용
- jinvorionstg 적 자산 색감과 ansimuz 배경/플레이어 색감의 팔레트 통합 시각 검수
- 1면 보스 Tank2의 헤드 회전을 어떻게 구현할지 (head 별도 스프라이트로 매 프레임 그릴지, 본체+head 합성된 하나의 스프라이트로 미리 만들지)
- ST00~ST03 VGM 곡을 실제 들어보고 스테이지/보스 분배 재조정 여부 결정

## 20. 참고

- ASSETS 카탈로그: `ASSETS/README.md`
- 엔진 문서: `CLAUDE.md`
- 자산 출처: ansimuz/spaceship-shooter-environment (CC0), jinvorionstg/medium-tiny-enemies-pack (상업 사용 OK)
