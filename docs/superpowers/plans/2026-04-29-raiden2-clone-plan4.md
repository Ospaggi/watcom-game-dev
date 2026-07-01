# Raiden 2 Clone — Plan 4: 사운드 통합 (BGM + SFX)

> **For agentic workers:** Use superpowers:subagent-driven-development to execute. Steps use checkbox (`- [ ]`) syntax.

**Goal:** ST00.vgm BGM 재생 (자동 루프), 발사/폭발/봄 SFX 트리거. 기존 SOUND/SFX 엔진 모듈 그대로 사용 — 신규 모듈 0.

**Architecture:** Plan 1-3의 기반 위에 자산 파이프라인 (ST00.vgm 복사) + GAME.CPP 사운드 init/loop + WEAPON/COLLIDE/PLAYER에 sfx_play() 호출 추가. SFX는 현재 FIRE.SFX 1개만 보유하므로 모든 슬롯에 같은 파일 로드 (음원 다양화는 후속 작업, 기능적으로 동작 확인이 목표).

**Tech Stack:** Watcom C++, Sound Blaster + AdLib (DOSBox 에뮬레이션).

**Spec 참조:** `docs/superpowers/specs/2026-04-28-raiden2-clone-design.md` §13 (사운드)

---

## File Map

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `convert.sh` | ST00.vgm → SRC/ST00.VGM 복사 + FIRE.SFX 로드 슬롯 표 (실제 작업은 코드에서) |
| `.gitignore` | SRC/*.VGM, SRC/*.SFX 추가 (생성 산출물) |
| `SRC/GAME.CPP` | snd_load + snd_play + 메인 루프 BGM 재시작 + sfx_load 슬롯들 |
| `SRC/WEAPON.CPP` | Vulcan 발사 시 sfx_play(SFX_ATK) |
| `SRC/COLLIDE.CPP` | 적 사망 시 sfx_play(SFX_HIT) |
| `SRC/PLAYER.CPP` | 봄 발동 시 sfx_play(SFX_BOMB) (수동/오토 둘 다) |

### NOT TOUCHED

- 신규 모듈 0
- 신규 sprite/img 0
- 신규 SFX 음원 0 (FIRE.SFX 재사용 — 음원 다양화는 후속)
- HUD, 무기 다양화, 메달 등은 Plan 5+

---

## Task 1: convert.sh — ST00.vgm 복사 + .gitignore 갱신

**Files:**
- Modify: `convert.sh`
- Modify: `.gitignore`

기존 convert.sh는 TYRIAN.VGM을 외부 경로에서 복사. 추가로 ASSETS/raw/audio_vgm/ST00.vgm을 SRC/ST00.VGM로 복사.

- [ ] **Step 1: convert.sh 갱신**

기존 "Copying VGM..." 단계 다음에 BGM 복사 추가 (또는 그 전에 두 줄로):

```bash
echo "Copying VGM..."
cp "/Users/gcjjyy/lab/oscc/imsplay/public/18 Tyrian, The Level.vgm" SRC/TYRIAN.VGM
cp ASSETS/raw/audio_vgm/ST00.vgm SRC/ST00.VGM
```

- [ ] **Step 2: 실행 + ST00.VGM 생성 확인**

```bash
./convert.sh
ls -lh SRC/ST00.VGM SRC/TYRIAN.VGM SRC/FIRE.SFX
```

Expected: ST00.VGM ~63KB 존재.

- [ ] **Step 3: .gitignore 갱신**

`.gitignore`에 추가 (기존 `SRC/*.SPR`, `SRC/*.IMG` 옆):

```
SRC/*.VGM
SRC/*.SFX
```

- [ ] **Step 4: 커밋**

```bash
git add convert.sh .gitignore
git commit -m "Copy ST00.vgm to SRC/ + gitignore generated VGM/SFX

ST00.vgm is the title + Stage 1 BGM track (per ASSETS/README.md
section 4-A). Copied to SRC/ for the in-DOS snd_load(). Both
*.VGM and *.SFX outputs are now gitignored — they are derived
artifacts of convert.sh."
```

---

## Task 2: GAME.CPP — BGM init + 루프 + SFX 슬롯 로드

**Files:**
- Modify: `SRC/GAME.CPP`

엔진 init 직후 VGM/SFX 로드. 메인 루프에서 BGM이 멈추면 자동 재시작 (간단한 루프).

- [ ] **Step 1: GAME.CPP main()에 사운드 로드 추가**

`gfx_set_palette_all(game_palette);` 다음 (또는 state_reset 이전 어디든) 추가:

```c
    /* Load BGM (snd_load returns SND_OK = 0 on success). */
    snd_load("ST00.VGM");
    snd_play();

    /* Load SFX slots. We only have FIRE.SFX in Plan 4 — load it into
     * the slots used by each event. Distinct sounds are deferred to
     * a later asset pass. */
    sfx_load(SFX_ATK,  "FIRE.SFX");
    sfx_load(SFX_HIT,  "FIRE.SFX");
    sfx_load(SFX_BOMB, "FIRE.SFX");
```

- [ ] **Step 2: 메인 루프에 BGM 자동 재시작**

`bg_update(dt);` 직전이나 그 위 어디든 (한 번 실행되면 됨):

```c
        /* BGM loop: if the OPL2 player has finished the track, restart it. */
        if (!snd_playing()) snd_play();
```

> **Note**: snd_playing()이 매 프레임 호출돼도 성능 문제 거의 없음 (단순 플래그 검사 추정). 안 그러면 한 번 끝나면 영영 무음.

- [ ] **Step 3: shutdown에 snd_stop 명시**

기존 shutdown에서 sfx_close → snd_close 순서가 LIFO 맞으므로 추가 변경 불필요. 단 안전을 위해 close 직전에 snd_stop() 추가 (대부분 snd_close가 알아서 처리하지만 안전망):

```c
    snd_stop();        /* explicit stop before close */
    effect_close();
    enemy_close();
    ...
```

(snd_close가 이미 stop을 호출한다면 중복 OK; 안 한다면 필수.)

- [ ] **Step 4: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -10
ls -l SRC/GAME.EXE
```

Expected: 0 errors. GAME.EXE 갱신.

- [ ] **Step 5: 커밋**

```bash
git add SRC/GAME.CPP
git commit -m "Wire BGM (ST00.VGM) + SFX slot loads in GAME.CPP

Loads ST00.VGM after gfx_set_palette_all and starts playback. Main
loop calls snd_play() if snd_playing() is 0 — naive loop, fine for
prototype until the SOUND module gains explicit looping.

SFX slots ATK/HIT/BOMB all load FIRE.SFX for now (only audio asset
we have). Distinct fire/explosion/bomb sounds are a later asset
pass — the wiring is in place so swapping files in convert.sh is
the only thing needed.

snd_stop() called before snd_close() as a safety belt."
```

---

## Task 3: WEAPON.CPP — 발사음 트리거

**Files:**
- Modify: `SRC/WEAPON.CPP`

Vulcan 발사 시점에 sfx_play(SFX_ATK).

- [ ] **Step 1: 새 include 추가**

`SRC/WEAPON.CPP` 상단 includes에 추가:

```c
#include "SFX.H"
```

- [ ] **Step 2: fire_vulcan_l1 함수에 sfx_play 추가**

```c
static void fire_vulcan_l1(void)
{
    /* Single bullet from the player ship's nose (top-center).
     * Bullet position (px, py) is its top-left in play-area coords.
     * Player position (g_player.x, .y) is the sprite CENTER. */
    i16 px = g_player.x - 1;
    i16 py = g_player.y - PLAYER_HALF_H - 6;
    bullet_spawn_player(BUL_PLAYER_VULCAN, px, py, 0, VULCAN_VY_Q4);
    sfx_play(SFX_ATK);
}
```

> **참고**: Vulcan은 12.5발/sec로 자주 발사돼서 SFX 풀(8 voices)에 빠르게 쌓일 수 있음. SFX 모듈은 voice 부족 시 슬롯 회수 처리 — 현재로선 OK. 사운드 너무 시끄럽거나 자르고 들리면 발사 간격당 한 번씩만 sfx_play하도록 weapon_fire_tick 레벨로 옮길 수 있음 (현재는 fire_vulcan_l1 내부라 동일 효과).

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -8
```

Expected: 0 errors.

- [ ] **Step 4: 커밋**

```bash
git add SRC/WEAPON.CPP
git commit -m "Play ATK SFX on Vulcan fire

sfx_play(SFX_ATK) inside fire_vulcan_l1, right after the bullet
spawns. Uses the SFX_ATK slot loaded with FIRE.SFX in GAME.CPP."
```

---

## Task 4: COLLIDE.CPP — 적 사망음 트리거

**Files:**
- Modify: `SRC/COLLIDE.CPP`

적이 죽는 시점에 sfx_play(SFX_HIT).

- [ ] **Step 1: 새 include 추가**

상단 includes에 추가:

```c
#include "SFX.H"
```

- [ ] **Step 2: 적 사망 처리에 sfx_play 추가**

기존 `collide_player_bullets_vs_enemies`에서 hp == 0인 분기:

```c
            if (e->hp > 1) {
                e->hp--;
            } else {
                e->active = 0;
                effect_spawn_explosion(e->x, e->y);
                g_state.score += 100;
                sfx_play(SFX_HIT);
            }
```

- [ ] **Step 3: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -8
```

Expected: 0 errors.

- [ ] **Step 4: 커밋**

```bash
git add SRC/COLLIDE.CPP
git commit -m "Play HIT SFX on enemy death

sfx_play(SFX_HIT) at the same point we spawn the death explosion
and grant the +100 score. Uses the SFX_HIT slot loaded with
FIRE.SFX in GAME.CPP."
```

---

## Task 5: PLAYER.CPP — 봄/사망 음 트리거

**Files:**
- Modify: `SRC/PLAYER.CPP`

봄 발동(수동 + 오토봄) 및 플레이어 사망 폭발 시 sfx_play(SFX_BOMB).

- [ ] **Step 1: 새 include 추가**

상단 includes에 추가:

```c
#include "SFX.H"
```

- [ ] **Step 2: blast_all_enemies()에 sfx_play 추가**

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
        /* future: g_state.score += per-type bonus */
    }
    sfx_play(SFX_BOMB);
}
```

> 모든 적 폭발 폭격은 한 번의 봄 발동이므로 sfx_play도 한 번. 루프 안에 넣지 말 것.

- [ ] **Step 3: player_take_damage 사망 폭발 시에도 SFX (선택)**

플레이어가 봄 없이 죽을 때도 폭발음 — 이미 blast_all_enemies는 안 호출되므로 별도. 사망 분기에 sfx_play(SFX_HIT) 추가 (또는 SFX_BOMB):

```c
    /* die */
    effect_spawn_explosion(g_player.x, g_player.y);
    sfx_play(SFX_HIT);
    if (g_state.lives > 0) g_state.lives--;
```

> SFX_HIT 재사용 — 실제로 다른 음이 있으면 SFX_LOSE 슬롯 등으로 분리.

- [ ] **Step 4: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -8
ls -l SRC/GAME.EXE
```

Expected: 0 errors.

- [ ] **Step 5: 커밋**

```bash
git add SRC/PLAYER.CPP
git commit -m "Play SFX on bomb activation and player death

blast_all_enemies (used by both manual bomb and autobomb) calls
sfx_play(SFX_BOMB) once after the screen-clear loop. Player death
explosion also triggers sfx_play(SFX_HIT) — same sound as enemy
death for now (FIRE.SFX), distinguishable later when the audio
asset pack expands."
```

---

## Task 6: 시각/청각 검증

- [ ] **Step 1: 사용자 검증**

```bash
./run.sh
```

확인 사항:
- 게임 시작 시 BGM (ST00) 재생, 끝나면 자동 재시작
- Z를 누를 때마다 발사음
- 적이 죽으면 폭발음
- X로 봄 발동 시 봄 효과음 (한 번)
- 오토봄 발동 시에도 봄 효과음
- 플레이어가 죽으면 폭발음
- ESC 정상 종료 (BGM 멈춤)

> **Note**: Plan 4에서 모든 SFX는 FIRE.SFX 재사용이라 음이 같음 — 실제 식별은 타이밍/빈도로. 향후 다른 음원 파일 추가하면 자연스럽게 분리됨.

- [ ] **Step 2: git log 점검**

```bash
git log --oneline -8
```

Expected: Plan 4의 5개 커밋이 main 위에.

---

## Self-Review Checklist

- [ ] **Spec coverage**:
  - BGM 재생 (ST00.VGM): ✓
  - 발사 SFX: ✓
  - 폭발 SFX: ✓
  - 봄 SFX: ✓
  - 다양한 음원: ✗ (FIRE.SFX 재사용 — 명시적 후속 작업)
- [ ] **함수 시그니처 매칭**:
  - sfx_play(int slot) → void
  - snd_load/play/stop/playing API 그대로 사용
- [ ] **자산 경로**: SRC/ST00.VGM, SRC/FIRE.SFX (snd_load/sfx_load는 CWD 기준)

---

## Open Items (Plan 5 인계)

- Distinct SFX 음원: 발사, 폭발(작은/큰), 봄, 픽업 등 별도 파일 (사용자 fx_sounds/ 디렉토리에서 추가 변환)
- 스테이지별 BGM 전환 (ST01/ST02/ST03 — 현재는 ST00만)
- 보스전 BGM (ST03)
- HUD (점수/잔기/봄 표시)
