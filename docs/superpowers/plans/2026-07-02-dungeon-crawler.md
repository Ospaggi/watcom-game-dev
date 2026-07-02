# 던전 크롤러 (DUNGEON) 구현 계획

> **For agentic workers:** 이 계획은 **인라인 실행** (superpowers:executing-plans) 전용이다.
> 서브에이전트 금지. 각 태스크의 시각 검증 체크포인트에서 SCRNCAP 캡처를 확인한다.
> **커밋은 자동으로 하지 않는다** — 사용자가 확인 후 직접 지시한다.

**Goal:** 4층 던전을 내려가 보스를 처치하는 턴제 로그라이크 (메인메뉴/4층/엔딩).

**Architecture:** 기존 엔진 모듈은 무수정. 신규 모듈 DUNGEON(맵생성+FOV) / MOB(플레이어·몬스터·전투) / HUD(패널+메시지) + GAME.CPP 상태 머신. 턴제 — 입력이 턴을 소비했을 때만 전체 화면 재렌더.

**Tech Stack:** Watcom C++ (WCL386, DOS4GW), VGA Mode 13h, Bun 에셋 도구, DOSBox 빌드/실행.

## Global Constraints

- STL/예외/RTTI 금지, C 표준 라이브러리만 (`stdio.h`, `stdlib.h`, `string.h`)
- 소스/런타임 파일명은 8.3 대문자
- init 순서: `timer_init()` → `input_init()` → `snd_init()` → `sfx_init()` → `gfx_init()` → `font_init()`, shutdown은 정확히 역순
- 4칸 들여쓰기, K&R 중괄호, 소문자 함수명 (`dun_generate()` 스타일)
- 팔레트: 0=투명, 1~15=그레이, 16~=Red램프, 48~=Yellow, 80~=Green, 208~=Brown (램프당 16단계, +0 어두움 → +15 밝음)
- 빌드: `./build.sh` (BUILD.LOG 확인), 실행: `./run.sh`
- 웹 에셋은 CC0 우선, 출처/라이선스를 `ASSETS/raw/dungeon/CREDITS.md`에 기록

## 화면/데이터 상수 (모든 태스크 공통)

- 타일 16×16. 뷰포트 15×11타일 = (0,0)~(239,175). 우측 패널 x=240~319. 메시지줄 y=176~199
- 맵 40×30. 카메라 `camx=clamp(px-7, 0, 25)`, `camy=clamp(py-5, 0, 19)`
- `TILES.SPR` 프레임: 0=바닥, 1=벽, 2=계단, 3=물약, 4=검, 5=금화
- `MOBS.SPR` 프레임: 0=쥐 1=박쥐 2=고블린 3=스켈레톤 4=오크 5=슬라임 6=다크나이트 7=마법사 8=보스 9=주인공

---

### Task 1: 웹 리소스 다운로드 + 라이선스 기록

**Files:**
- Create: `ASSETS/raw/dungeon/` (타일셋 PNG), `ASSETS/raw/dungeon/sfx/` (WAV/OGG)
- Create: `ASSETS/raw/dungeon/CREDITS.md`

**Interfaces:**
- Produces: 16×16 던전 타일셋 PNG 1개 이상 (바닥/벽/계단/몬스터/아이템 포함), 효과음 6종 (타격/피격/아이템/계단/승리/패배)

- [ ] **Step 1: 타일셋 후보 검색·다운로드** — WebSearch/WebFetch로 아래 우선순위로 확보:
  1. DawnLike (OpenGameArt, CC0+크레딧 요청: DragonDePlatino, DawnBringer 팔레트) — `https://opengameart.org/content/dawnlike-16x16-universal-rogue-like-tileset-v181`
  2. Kenney Tiny Dungeon (CC0) — `https://kenney.nl/assets/tiny-dungeon`
  3. 0x72 Dungeon Tileset II (CC0) — `https://0x72.itch.io/dungeontileset-ii`
  zip은 스크래치패드에 받고 PNG만 `ASSETS/raw/dungeon/`로 복사.
- [ ] **Step 2: 효과음 다운로드** — Juhani Junkala "512 Sound Effects (8-bit style)" (OpenGameArt, CC0) 등에서 타격/피격/아이템/계단/승리/패배 6종 선별 → `ASSETS/raw/dungeon/sfx/`
- [ ] **Step 3: 시각 확인** — 타일셋 PNG를 Read로 열어 필요한 셀(바닥/벽/계단/몬스터 9종/아이템 3종)이 실제로 있는지 확인, 각 셀의 (sx, sy) 픽셀 좌표를 기록
- [ ] **Step 4: CREDITS.md 작성** — 각 파일의 출처 URL·작가·라이선스 기록

### Task 2: 시트 합성 도구 + convert.sh 확장 + 변환 검증

**Files:**
- Create: `tools/mksheet.ts`
- Modify: `convert.sh` (스프라이트/효과음 변환 추가)
- Produces: `SRC/TILES.SPR` (6프레임), `SRC/MOBS.SPR` (10프레임), `SRC/ATK.SFX` `HIT.SFX` `ITEM.SFX` `STAIR.SFX` `WIN.SFX` `LOSE.SFX`

**Interfaces:**
- Consumes: Task 1의 PNG + 셀 좌표
- Produces: `spr_load("TILES.SPR")` / `spr_load("MOBS.SPR")`로 로드 가능한 SPR (프레임 순서는 위 공통 상수 표 그대로)

- [ ] **Step 1: mksheet.ts 작성** — 소스 PNG들에서 16×16 셀들을 잘라 한 줄짜리 시트 PNG로 합성. PNG 디코더는 `tools/mksprite.ts`의 함수(`readPng` 계열)를 복사해 재사용, 인코더는 미니멀 구현(IHDR/IDAT(무필터)/IEND + `Bun.deflateSync`). CLI:
  ```
  bun tools/mksheet.ts <cells.json> <OUT.PNG>
  # cells.json: [{ "src": "ASSETS/raw/dungeon/xxx.png", "sx": 0, "sy": 16 }, ...] 순서=프레임 순서
  ```
  셀 좌표 JSON은 `ASSETS/raw/dungeon/TILES.json`, `MOBS.json`으로 저장 (Task 1 Step 3의 좌표 사용).
- [ ] **Step 2: convert.sh에 추가**
  ```bash
  echo "Building sprite sheets..."
  bun tools/mksheet.ts ASSETS/raw/dungeon/TILES.json ASSETS/raw/dungeon/TILES.PNG
  bun tools/mksheet.ts ASSETS/raw/dungeon/MOBS.json ASSETS/raw/dungeon/MOBS.PNG
  bun tools/mksprite.ts --bin --grid 6x1  ASSETS/raw/dungeon/TILES.PNG SRC/TILES.SPR
  bun tools/mksprite.ts --bin --grid 10x1 ASSETS/raw/dungeon/MOBS.PNG  SRC/MOBS.SPR
  echo "Converting dungeon SFX..."
  bun tools/mksfx.ts ASSETS/raw/dungeon/sfx/atk.wav   SRC/ATK.SFX
  bun tools/mksfx.ts ASSETS/raw/dungeon/sfx/hit.wav   SRC/HIT.SFX
  bun tools/mksfx.ts ASSETS/raw/dungeon/sfx/item.wav  SRC/ITEM.SFX
  bun tools/mksfx.ts ASSETS/raw/dungeon/sfx/stair.wav SRC/STAIR.SFX
  bun tools/mksfx.ts ASSETS/raw/dungeon/sfx/win.wav   SRC/WIN.SFX
  bun tools/mksfx.ts ASSETS/raw/dungeon/sfx/lose.wav  SRC/LOSE.SFX
  ```
  (mksprite의 실제 CLI 인자 형식은 실행 전에 소스에서 확인해 맞춘다)
- [ ] **Step 3: 실행** — `./convert.sh` 성공, `SRC/TILES.SPR` 등 파일 생성 확인
- [ ] **Step 4: 시각 검증** — 합성된 `TILES.PNG`/`MOBS.PNG`를 Read로 열어 프레임 순서·투명 배경 확인 (팔레트 매핑 결과가 이상하면 소스 셀 교체)

### Task 3: DUNGEON 모듈 (맵 생성 + FOV) + 시각 검증

**Files:**
- Create: `SRC/DUNGEON.H`, `SRC/DUNGEON.CPP`
- Modify: `SRC/GAME.CPP` (임시 디버그 렌더로 교체)

**Interfaces:**
- Produces:
  ```c
  #define MAP_W 40
  #define MAP_H 30
  #define T_WALL 0
  #define T_FLOOR 1
  #define T_STAIRS 2
  #define VIS_UNSEEN 0
  #define VIS_SEEN 1
  #define VIS_LIT 2
  #define MAX_ROOMS 10
  typedef struct { int x, y, w, h; } Room;
  extern unsigned char dun_map[MAP_H][MAP_W];
  extern unsigned char dun_vis[MAP_H][MAP_W];
  extern Room dun_rooms[MAX_ROOMS];
  extern int  dun_nrooms;
  void dun_generate(int floor, int *px, int *py); /* floor 4는 계단 없음 */
  int  dun_walkable(int x, int y);                /* 벽 아님 && 맵 안 */
  void dun_fov(int px, int py);                   /* 반경 5, LIT/SEEN 갱신 */
  /* 아이템 */
  #define IT_POTION 0
  #define IT_SWORD  1
  #define IT_GOLD   2
  #define MAX_ITEMS 12
  typedef struct { int x, y, kind, taken; } Item;
  extern Item dun_items[MAX_ITEMS];
  extern int  dun_nitems;
  void dun_spawn_items(int floor);
  ```

- [ ] **Step 1: DUNGEON.CPP 구현** — 핵심 알고리즘:
  ```c
  /* 방 배치: 최대 200회 시도, 방 크기 w=4+rand%5, h=3+rand%4,
     기존 방과 1칸 여유 겹침 검사. 새 방은 직전 방 중심과 L자 복도 연결. */
  void dun_generate(int floor, int *px, int *py)
  {
      int i, tries, x, y;
      memset(dun_map, T_WALL, sizeof(dun_map));
      memset(dun_vis, VIS_UNSEEN, sizeof(dun_vis));
      dun_nrooms = 0;
      for (tries = 0; tries < 200 && dun_nrooms < MAX_ROOMS; tries++) {
          Room r;
          int ok = 1;
          r.w = 4 + rand() % 5;
          r.h = 3 + rand() % 4;
          r.x = 1 + rand() % (MAP_W - r.w - 2);
          r.y = 1 + rand() % (MAP_H - r.h - 2);
          for (i = 0; i < dun_nrooms; i++) {
              Room *o = &dun_rooms[i];
              if (r.x < o->x + o->w + 1 && o->x < r.x + r.w + 1 &&
                  r.y < o->y + o->h + 1 && o->y < r.y + r.h + 1) { ok = 0; break; }
          }
          if (!ok) continue;
          for (y = r.y; y < r.y + r.h; y++)
              for (x = r.x; x < r.x + r.w; x++)
                  dun_map[y][x] = T_FLOOR;
          if (dun_nrooms > 0) {
              Room *p = &dun_rooms[dun_nrooms - 1];
              int cx = p->x + p->w / 2, cy = p->y + p->h / 2;
              int tx = r.x + r.w / 2,  ty = r.y + r.h / 2;
              while (cx != tx) { dun_map[cy][cx] = T_FLOOR; cx += (tx > cx) ? 1 : -1; }
              while (cy != ty) { dun_map[cy][cx] = T_FLOOR; cy += (ty > cy) ? 1 : -1; }
          }
          dun_rooms[dun_nrooms++] = r;
      }
      *px = dun_rooms[0].x + dun_rooms[0].w / 2;
      *py = dun_rooms[0].y + dun_rooms[0].h / 2;
      if (floor < 4) {
          Room *lr = &dun_rooms[dun_nrooms - 1];
          dun_map[lr->y + lr->h / 2][lr->x + lr->w / 2] = T_STAIRS;
      }
  }
  ```
  ```c
  /* LOS: Bresenham. 목표 칸 자체가 벽이어도 보인다(벽면 조명). */
  static int los(int x0, int y0, int x1, int y1)
  {
      int dx = abs(x1 - x0), dy = abs(y1 - y0);
      int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
      int err = dx - dy;
      while (x0 != x1 || y0 != y1) {
          int e2 = err * 2;
          if (e2 > -dy) { err -= dy; x0 += sx; }
          if (e2 <  dx) { err += dx; y0 += sy; }
          if (x0 == x1 && y0 == y1) break;
          if (dun_map[y0][x0] == T_WALL) return 0;
      }
      return 1;
  }

  void dun_fov(int px, int py)
  {
      int x, y;
      for (y = 0; y < MAP_H; y++)
          for (x = 0; x < MAP_W; x++)
              if (dun_vis[y][x] == VIS_LIT) dun_vis[y][x] = VIS_SEEN;
      for (y = py - 5; y <= py + 5; y++) {
          for (x = px - 5; x <= px + 5; x++) {
              if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
              if ((x - px) * (x - px) + (y - py) * (y - py) > 25) continue;
              if (los(px, py, x, y)) dun_vis[y][x] = VIS_LIT;
          }
      }
  }
  ```
  ```c
  /* 아이템: 방 1..nrooms-1의 랜덤 바닥칸. 물약2 검1 금화3, 겹침 회피. */
  void dun_spawn_items(int floor)
  {
      static const int kinds[6] = { IT_POTION, IT_POTION, IT_SWORD,
                                    IT_GOLD, IT_GOLD, IT_GOLD };
      int i, t;
      (void)floor;
      dun_nitems = 0;
      for (i = 0; i < 6 && dun_nitems < MAX_ITEMS; i++) {
          for (t = 0; t < 50; t++) {
              Room *r = &dun_rooms[1 + rand() % (dun_nrooms - 1)];
              int x = r->x + rand() % r->w, y = r->y + rand() % r->h;
              int j, clash = 0;
              if (dun_map[y][x] != T_FLOOR) continue;
              for (j = 0; j < dun_nitems; j++)
                  if (dun_items[j].x == x && dun_items[j].y == y) { clash = 1; break; }
              if (clash) continue;
              dun_items[dun_nitems].x = x;
              dun_items[dun_nitems].y = y;
              dun_items[dun_nitems].kind = kinds[i];
              dun_items[dun_nitems].taken = 0;
              dun_nitems++;
              break;
          }
      }
  }
  ```
- [ ] **Step 2: GAME.CPP를 디버그 렌더로 임시 교체** — 8×6픽셀 미니 타일로 40×30 전체 맵을 그리는 main: 벽=idx3, 바닥=idx6, 계단=Yellow(58), LIT칸은 밝게(+4), 방향키로 플레이어 `@`(빨강 24 사각형) 이동 + FOV 갱신, R키로 재생성, F10키로 `scrncap_save_bmp("CAP.BMP")`, ESC 종료. (임시 코드 — Task 6에서 전면 교체)
- [ ] **Step 3: 빌드** — `./build.sh`, BUILD.LOG에 에러 없음
- [ ] **Step 4: 시각 검증** — `./run.sh` → F10 캡처 → `bun tools/bmp2png.ts` 변환 후 이미지 Read. 확인: 방들이 모두 복도로 연결, 계단 존재, FOV가 벽에 막힘, R 몇 번 눌러 다양한 맵 확인

### Task 4: MOB 모듈 (플레이어/몬스터/범프 전투/AI)

**Files:**
- Create: `SRC/MOB.H`, `SRC/MOB.CPP`

**Interfaces:**
- Consumes: `dun_map/dun_vis/dun_rooms/dun_walkable` (Task 3), `hud_msg(const char*)` (Task 5 — 이 태스크 시점엔 extern 선언만으로 컴파일)
- Produces:
  ```c
  #define MAX_MOBS 16
  #define KIND_BOSS 8
  typedef struct { int x, y, hp, maxhp, atk, kind, alive; } Mob;
  typedef struct { const char *name; int hp, atk, frame; } MobDef;
  typedef struct { int x, y, hp, maxhp, atk, potions, gold; } Player;
  extern Mob mobs[MAX_MOBS];
  extern int nmobs;
  extern const MobDef mob_defs[9];
  extern Player pl;
  void player_reset(void);            /* HP20/20, ATK3, 물약1, 금화0 */
  void mob_spawn_floor(int floor);    /* 방 1..N-1에 배치, 4층은 보스 추가 */
  Mob *mob_at(int x, int y);
  void mob_act_all(void);             /* 전 몬스터 1턴: LIT이면 추적, 아니면 배회 */
  int  player_attack(Mob *m);         /* 1=처치. 메시지+SFX는 내부 처리 */
  ```

- [ ] **Step 1: MOB.CPP 구현**
  ```c
  const MobDef mob_defs[9] = {
      { "쥐",       4,  1, 0 },   /* 1층 */
      { "박쥐",     3,  2, 1 },
      { "고블린",   6,  2, 2 },   /* 2층 */
      { "스켈레톤", 8,  3, 3 },
      { "오크",    12,  4, 4 },   /* 3층 */
      { "슬라임",  10,  3, 5 },
      { "다크나이트", 16, 5, 6 }, /* 4층 */
      { "마법사",  12,  6, 7 },
      { "드래곤",  40,  7, 8 },   /* 보스 */
  };
  /* 스폰 수: 층별 { 5, 6, 7, 6 }. 종류 = (floor-1)*2 + rand()%2.
     위치: 방 1..nrooms-1 랜덤 바닥칸, 겹침 회피 (아이템과는 겹쳐도 무방).
     4층: 마지막 방 중심에 보스(KIND_BOSS) 1마리 추가. */
  ```
  ```c
  /* 데미지 공용: atk + rand()%3 - 1 (최소 1) */
  static int roll_dmg(int atk)
  {
      int d = atk + rand() % 3 - 1;
      return (d < 1) ? 1 : d;
  }

  int player_attack(Mob *m)
  {
      char buf[64];
      int d = roll_dmg(pl.atk);
      m->hp -= d;
      if (m->hp <= 0) {
          m->alive = 0;
          sprintf(buf, "%s을(를) 물리쳤다!", mob_defs[m->kind].name);
          hud_msg(buf);
          return 1;
      }
      sprintf(buf, "%s에게 %d 데미지!", mob_defs[m->kind].name, d);
      hud_msg(buf);
      return 0;
  }

  static void mob_attack_player(Mob *m)
  {
      char buf[64];
      int d = roll_dmg(m->atk);
      pl.hp -= d;
      sprintf(buf, "%s의 공격! %d 데미지", mob_defs[m->kind].name, d);
      hud_msg(buf);
  }

  void mob_act_all(void)
  {
      static const int d4[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
      int i, nx, ny;
      for (i = 0; i < nmobs; i++) {
          Mob *m = &mobs[i];
          if (!m->alive) continue;
          if (dun_vis[m->y][m->x] == VIS_LIT) {   /* 상호 가시 = 추적 */
              int dx = (pl.x > m->x) - (pl.x < m->x);
              int dy = (pl.y > m->y) - (pl.y < m->y);
              if (abs(pl.x - m->x) >= abs(pl.y - m->y)) { nx = m->x + dx; ny = m->y; }
              else                                       { nx = m->x; ny = m->y + dy; }
              if (!(nx == pl.x && ny == pl.y) &&
                  (!dun_walkable(nx, ny) || mob_at(nx, ny))) {
                  if (nx != m->x) { nx = m->x; ny = m->y + dy; }
                  else            { nx = m->x + dx; ny = m->y; }
              }
              if (nx == pl.x && ny == pl.y) { mob_attack_player(m); continue; }
          } else {
              int r = rand() % 4;
              nx = m->x + d4[r][0];
              ny = m->y + d4[r][1];
              if (nx == pl.x && ny == pl.y) continue;
          }
          if (dun_walkable(nx, ny) && !mob_at(nx, ny)) { m->x = nx; m->y = ny; }
      }
  }
  ```
- [ ] **Step 2: 컴파일 확인** — `./build.sh` (아직 GAME.CPP는 디버그 렌더 — MOB은 링크만 확인)

### Task 5: HUD 모듈 (우측 패널 + 메시지)

**Files:**
- Create: `SRC/HUD.H`, `SRC/HUD.CPP`

**Interfaces:**
- Consumes: `Player pl` (Task 4), `gfx_*`, `font_puts`
- Produces:
  ```c
  void hud_msg(const char *s);      /* 최대 63자 저장 */
  void hud_draw(int floor);         /* 패널(240~319) + 하단 메시지줄 렌더 */
  ```

- [ ] **Step 1: HUD.CPP 구현**
  ```c
  static char msgbuf[64] = "";

  void hud_msg(const char *s)
  {
      strncpy(msgbuf, s, 63);
      msgbuf[63] = '\0';
  }

  void hud_draw(int floor)
  {
      char buf[32];
      int w;
      gfx_fill_rect(240, 0, 80, 176, 2);            /* 패널 배경 */
      gfx_vline(240, 0, 175, 8);
      sprintf(buf, "지하 %d층", floor);
      font_puts(246, 6, buf, 15);
      sprintf(buf, "HP %d/%d", pl.hp, pl.maxhp);
      font_puts(246, 32, buf, 15);
      gfx_rect(245, 50, 71, 8, 15);                  /* HP 바 (Red 램프) */
      w = (pl.hp > 0) ? 69 * pl.hp / pl.maxhp : 0;
      gfx_fill_rect(246, 51, w, 6, 24);
      sprintf(buf, "공격력 %d", pl.atk);
      font_puts(246, 68, buf, 15);
      sprintf(buf, "물약 x%d", pl.potions);
      font_puts(246, 94, buf, 11);
      sprintf(buf, "금화 %d", pl.gold);
      font_puts(246, 120, buf, 58);                  /* Yellow */
      font_puts(246, 152, "P:물약", 8);
      gfx_fill_rect(0, 176, 320, 24, 0);             /* 메시지줄 */
      font_puts(4, 180, msgbuf, 14);
  }
  ```
- [ ] **Step 2: 컴파일 확인** — `./build.sh`

### Task 6: GAME.CPP 통합 (턴 루프 + 아이템 + 층 이동)

**Files:**
- Modify: `SRC/GAME.CPP` (디버그 렌더 전면 교체)

**Interfaces:**
- Consumes: Task 3~5의 모든 API + `spr_load/spr_draw_clipped` + `TILES.SPR`/`MOBS.SPR`

- [ ] **Step 1: GAME.CPP 작성** — 구조:
  ```c
  /* 상태: ST_MENU, ST_PLAY, ST_DEAD, ST_ENDING. KEY_P = 0x19 정의.
     전역: Sprite spr_tiles, spr_mobs; int floor; int dirty; */

  /* new_floor(f): srand는 main에서 1회(timer_ms()).
     dun_generate → pl.x/y 설정 → dun_spawn_items → mob_spawn_floor
     → dun_fov → 층 BGM 로드/재생 → hud_msg("지하 N층에 내려왔다") → dirty=1 */

  static const char *floor_bgm[4] =
      { "ST01.VGM", "ST02.VGM", "ST03.VGM", "ST06.VGM" };

  /* render_play(): 카메라 클램프 후
     - 타일: VIS_UNSEEN → 스킵(검정), VIS_LIT → spr_tiles 프레임(dun_map값
       0벽→프레임1, 1바닥→프레임0, 2계단→프레임2),
       VIS_SEEN → fill_rect(벽=idx3, 바닥/계단=idx1) 딤 처리
     - 아이템: !taken && VIS_LIT일 때 프레임 3+kind
     - 몬스터: alive && VIS_LIT일 때 spr_mobs 프레임 mob_defs[kind].frame
     - 주인공: spr_mobs 프레임 9
     - hud_draw(floor) → gfx_vsync() → gfx_flip() */

  /* 플레이 턴 처리 (input_read_key 논블로킹):
     - 방향키/WASD: 목표칸에 몬스터 → player_attack + sfx(ATK),
       처치 시 금화 +5, 보스(KIND_BOSS) 처치 시 → sfx(WIN), ST_ENDING.
       몬스터 없고 walkable → 이동. 이동 후:
         아이템 칸 → 획득 (물약: potions++ / 검: atk+=2 / 금화: gold+=20,
         메시지 + sfx(ITEM), taken=1)
         계단 칸 → sfx(STAIR), floor++, new_floor(floor)
     - P: potions>0 && hp<maxhp → hp+=10 클램프, potions--, 메시지, sfx(ITEM)
     - ESC: ST_MENU로
     - 턴 소비 시: mob_act_all() → hp<=0이면 sfx(LOSE), ST_DEAD
       → dun_fov(pl.x, pl.y) → dirty=1
     - dirty일 때만 render_play() (BGM 루프 체크는 매 루프) */

  /* 메뉴: gfx_clear(0), 타이틀 "던전 크롤러" (font_puts, 큰 배치는 텍스트 2줄),
     "ENTER: 게임 시작   ESC: 종료", ST00.VGM 재생.
     ENTER → player_reset, floor=1, new_floor(1), ST_PLAY.
     게임오버: "당신은 쓰러졌다..." + 도달 층/금화, ENTER → ST_MENU.
     엔딩: ST07.VGM, "드래곤을 물리쳤다!" + 금화 점수, ENTER → ST_MENU. */

  /* main(): 표준 init 순서 → 스프라이트/SFX 로드
     (sfx_load 슬롯: 0=ATK 1=HIT 2=ITEM 3=STAIR 4=WIN 5=LOSE)
     → 상태 루프 → 표준 역순 shutdown */
  ```
  F10 디버그 캡처(`scrncap_save_bmp`)는 유지.
- [ ] **Step 2: 빌드** — `./build.sh`, 에러 없음
- [ ] **Step 3: 시각 검증 (핵심 체크포인트)** — `./run.sh` → 캡처 확인:
  - 메뉴 화면 정상, ENTER로 시작
  - 타일 렌더/FOV 딤 처리/HUD/한글 메시지 정상
  - 범프 전투, 아이템 획득, 계단으로 2층 이동
- [ ] **Step 4: 상태 전이 검증** — 사망 → 게임오버 → 메뉴 재시작 동작 확인

### Task 7: 사운드 마감 + 4층 보스 → 엔딩 확인

**Files:**
- Modify: `SRC/GAME.CPP` (필요 시 밸런스 상수만)

- [ ] **Step 1: 치트 빌드로 엔딩 검증** — 임시로 `pl.atk = 99` 시작 + `new_floor(4)` 직행 코드로 빌드 → 보스 처치 → 엔딩 화면/음악 확인 → 치트 제거 후 재빌드
- [ ] **Step 2: 음악/효과음 확인** — 층 전환마다 BGM 교체, 6종 SFX 발음 확인. 이상 시 `./sfxtest.sh`로 회귀 확인
- [ ] **Step 3: 시각 검증** — 엔딩/게임오버 화면 캡처 확인

### Task 8: 밸런싱 + 최종 검증

- [ ] **Step 1: 완주 플레이** — 1층부터 정상 플레이로 난이도 확인 (1~2층은 무난, 3~4층은 물약 관리 필요한 수준). 과하게 어렵/쉬우면 mob_defs HP/ATK, 물약 회복량(10), 검 보너스(+2)만 조정
- [ ] **Step 2: 최종 빌드/실행** — `./build.sh` 클린 빌드 → `./run.sh` 최종 확인, BUILD.LOG 무경고 지향
- [ ] **Step 3: 문서 갱신** — `CREDITS.md` 최종화. 사용자에게 결과 보고 (커밋 여부는 사용자 결정)

## Self-Review 결과

- 스펙 커버리지: 메뉴/4층/엔딩(T6~7), 절차 생성+FOV(T3), 범프 전투+AI(T4), 라이트 아이템(T3/T6), HUD+한글 메시지(T5), 웹 리소스+라이선스(T1~2), 음악/SFX(T2/T7) — 전 항목 태스크 존재
- 타입/이름 일관성: `dun_*` / `mob_*` / `hud_*` / `pl` 전 태스크 동일
- 데이터 의존: Task 1의 셀 좌표와 mksprite CLI 인자 형식은 실행 시점 확인 항목으로 명시
