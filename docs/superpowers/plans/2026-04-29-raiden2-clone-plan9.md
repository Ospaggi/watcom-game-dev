# Raiden 2 Clone — Plan 9: 무기 레벨 L1~L4

> **For agentic workers:** Use superpowers:subagent-driven-development. Steps use `- [ ]` checkbox.

**Goal:** Vulcan/Laser/Plasma 각각 레벨 1~4 구현. P 픽업 시 weapon_level +1 (cap 4). HUD에 현재 레벨 숫자 표시. 더 많이 쏠수록 화려.

**Architecture:** WEAPON.CPP의 fire 함수를 레벨별 분기로 확장. ITEM 픽업 코드는 weapon_level 증가 1줄만 추가. HUD는 무기 색 사각형 옆 숫자 1자리 추가. 신규 모듈 0.

**Spec 참조:** `docs/superpowers/specs/2026-04-28-raiden2-clone-design.md` §4.3 (메인 무기 레벨)

---

## File Map

### MODIFIED FILES

| 경로 | 변경 |
|---|---|
| `SRC/WEAPON.CPP` | fire_vulcan/laser/plasma를 L1~L4 분기 처리. 추가 탄 spawn |
| `SRC/COLLIDE.CPP` | `collide_player_vs_items()`에서 P 픽업 시 weapon_level +1 (cap 4) |
| `SRC/HUD.CPP` | 무기 색 사각형 (x=12, y=28, 4x4) 옆에 레벨 숫자 (x=18, y=28) |

### NOT TOUCHED

- 휘는 Laser (sin/curve 추적): 후속 plan
- 호밍 미사일 자동 발사: 후속 plan

---

## Task 1: WEAPON.CPP — V/L/P 레벨별 발사

**Files:**
- Modify: `SRC/WEAPON.CPP`

`fire_vulcan_l1` 같은 단일 함수를 레벨 인자 받는 형태로 변경하거나, 또는 그대로 두고 안에서 weapon_level 분기. 후자가 변경 적음.

**Step 1: WEAPON.CPP — 레벨별 분기 (간단 모드)**

```c
static void fire_vulcan(u8 lvl)
{
    i16 cx = g_player.x;
    i16 py = g_player.y - PLAYER_HALF_H - 6;

    /* L1: center.  L2: +/-2 parallel.  L3: L2 + outer 1deg.  L4: L3 + outer 2deg. */
    /* lateral velocity per outer pair (q4) */
    i16 lat1 = 4;   /* small spread for L3 outer */
    i16 lat2 = 8;   /* wider for L4 outer */

    bullet_spawn_player(BUL_PLAYER_VULCAN, cx - 1, py, 0, VULCAN_VY_Q4);
    if (lvl >= 2) {
        bullet_spawn_player(BUL_PLAYER_VULCAN, cx - 4, py, 0, VULCAN_VY_Q4);
        bullet_spawn_player(BUL_PLAYER_VULCAN, cx + 2, py, 0, VULCAN_VY_Q4);
    }
    if (lvl >= 3) {
        bullet_spawn_player(BUL_PLAYER_VULCAN, cx - 6, py, -lat1, VULCAN_VY_Q4);
        bullet_spawn_player(BUL_PLAYER_VULCAN, cx + 4, py,  lat1, VULCAN_VY_Q4);
    }
    if (lvl >= 4) {
        bullet_spawn_player(BUL_PLAYER_VULCAN, cx - 8, py, -lat2, VULCAN_VY_Q4);
        bullet_spawn_player(BUL_PLAYER_VULCAN, cx + 6, py,  lat2, VULCAN_VY_Q4);
    }
    sfx_play(SFX_ATK);
}

static void fire_laser(u8 lvl)
{
    i16 cx = g_player.x;
    i16 py = g_player.y - PLAYER_HALF_H - 12;

    /* L1: 1 thin beam (2x12).  L2: 1 thicker (4x12).  L3: + 2 side beams (parallel ±6).  L4: + 2 wider (±12). */
    if (lvl == 1) {
        bullet_spawn_player(BUL_PLAYER_LASER, cx - 1, py, 0, LASER_VY_Q4);
    } else {
        /* For lvl >= 2, fire two narrow beams 1 px apart (visually thicker) */
        bullet_spawn_player(BUL_PLAYER_LASER, cx - 1, py, 0, LASER_VY_Q4);
        bullet_spawn_player(BUL_PLAYER_LASER, cx + 1, py, 0, LASER_VY_Q4);
    }
    if (lvl >= 3) {
        bullet_spawn_player(BUL_PLAYER_LASER, cx - 6, py, 0, LASER_VY_Q4);
        bullet_spawn_player(BUL_PLAYER_LASER, cx + 6, py, 0, LASER_VY_Q4);
    }
    if (lvl >= 4) {
        bullet_spawn_player(BUL_PLAYER_LASER, cx - 12, py, 0, LASER_VY_Q4);
        bullet_spawn_player(BUL_PLAYER_LASER, cx + 12, py, 0, LASER_VY_Q4);
    }
    sfx_play(SFX_ATK);
}

static void fire_plasma(u8 lvl)
{
    i16 cx = g_player.x;
    i16 py = g_player.y - PLAYER_HALF_H - 4;

    bullet_spawn_player(BUL_PLAYER_PLASMA, cx - 2, py, 0, PLASMA_VY_Q4);
    if (lvl >= 2) {
        /* Visually larger by spawning 2 stacked balls */
        bullet_spawn_player(BUL_PLAYER_PLASMA, cx - 2, py - 4, 0, PLASMA_VY_Q4);
    }
    if (lvl >= 3) {
        /* Side balls — slight outward angle */
        bullet_spawn_player(BUL_PLAYER_PLASMA, cx - 6, py + 2, -8, PLASMA_VY_Q4);
        bullet_spawn_player(BUL_PLAYER_PLASMA, cx + 2, py + 2,  8, PLASMA_VY_Q4);
    }
    if (lvl >= 4) {
        /* Wider side balls */
        bullet_spawn_player(BUL_PLAYER_PLASMA, cx - 10, py + 4, -16, PLASMA_VY_Q4);
        bullet_spawn_player(BUL_PLAYER_PLASMA, cx + 6,  py + 4,  16, PLASMA_VY_Q4);
    }
    sfx_play(SFX_ATK);
}
```

기존 `fire_vulcan_l1`/`fire_laser_l1`/`fire_plasma_l1` 함수는 위 레벨 받는 함수로 대체. `weapon_fire_tick`에서 `g_state.weapon_level`을 인자로 전달:

```c
void weapon_fire_tick(u32 dt_ms, int z_held)
{
    if (g_vulcan_cd_ms > dt_ms) g_vulcan_cd_ms -= dt_ms;
    else                         g_vulcan_cd_ms = 0;

    if (!z_held) return;
    if (g_vulcan_cd_ms > 0) return;

    u8 lvl = g_state.weapon_level;
    if (lvl < 1) lvl = 1;
    if (lvl > 4) lvl = 4;

    switch (g_state.weapon) {
    case PW_VULCAN:
        fire_vulcan(lvl);
        g_vulcan_cd_ms = VULCAN_PERIOD_MS;
        break;
    case PW_LASER:
        fire_laser(lvl);
        g_vulcan_cd_ms = LASER_PERIOD_MS;
        break;
    case PW_PLASMA:
        fire_plasma(lvl);
        g_vulcan_cd_ms = PLASMA_PERIOD_MS;
        break;
    default:
        fire_vulcan(lvl);
        g_vulcan_cd_ms = VULCAN_PERIOD_MS;
        break;
    }
}
```

**Step 2: 빌드 검증**

```bash
./build.sh
cat BUILD.LOG | tail -5
```

**Step 3: 커밋**

```bash
git add SRC/WEAPON.CPP
git commit -m "WEAPON — implement L1-L4 spreads for Vulcan/Laser/Plasma

Replace single-bullet fire_*_l1 functions with fire_*(lvl) that
dispatches by g_state.weapon_level (1..4 cap). Each weapon has a
distinctive escalation:
  Vulcan: 1 -> 3 parallel -> 5 fan -> 7 fan
  Laser:  1 thin -> 1 thick -> 3 beams -> 5 beams
  Plasma: 1 ball -> 2 stacked -> 4 (2 stacked + 2 side) -> 6

Side bullets in Vulcan L3+ and Plasma L3+ get small lateral velocity
for a fan effect. Lasers stay parallel for now (no curve)."
```

---

## Task 2: COLLIDE — P 픽업 시 weapon_level +1

**Files:**
- Modify: `SRC/COLLIDE.CPP`

기존 `collide_player_vs_items()`는 무기 전환만 함. weapon_level 증가 추가.

**Step 1: COLLIDE.CPP 수정**

```c
        /* pickup: switch weapon + score + level up */
        u8 new_weapon = it->pw_color;
        if (g_state.weapon == new_weapon) {
            /* same weapon: just level up */
            if (g_state.weapon_level < 4) g_state.weapon_level++;
        } else {
            /* different weapon: switch and gain a level (cap 4) */
            g_state.weapon = new_weapon;
            if (g_state.weapon_level < 4) g_state.weapon_level++;
        }
        g_state.score += 10000;
        it->active = 0;
        sfx_play(SFX_PWR);
```

기존 `g_state.weapon = it->pw_color;` 한 줄을 위 블록으로 교체.

> **참고**: spec section 9.1 "다른 색 먹으면 무기 전환 (파워 레벨은 그대로 +1)". 즉 같은 무기든 다른 무기든 픽업 시 +1. 위 코드는 그렇게 함.

**Step 2: 빌드 + 커밋**

```bash
./build.sh
cat BUILD.LOG | tail -5

git add SRC/COLLIDE.CPP
git commit -m "P pickup levels up weapon (+1, cap 4)

Per spec section 9.1: picking up a P icon advances weapon_level +1
regardless of whether the player switched colors. Same color =
just level up. Different color = switch + level up. Cap at 4."
```

---

## Task 3: HUD — 무기 레벨 숫자 표시

**Files:**
- Modify: `SRC/HUD.CPP`

기존 무기 색 사각형 (x=12, y=28, 4x4) 옆에 weapon_level 숫자 1자리.

**Step 1: HUD.CPP 수정 — `hud_render()`에서 무기 사각형 다음 줄에**

```c
    gfx_fill_rect(12, 28, 4, 4, wcol);
    draw_digit(18, 28, (int)g_state.weapon_level, HUD_TEXT_COLOR);
```

**Step 2: 빌드 + 커밋**

```bash
./build.sh
cat BUILD.LOG | tail -5

git add SRC/HUD.CPP
git commit -m "HUD — show weapon level next to weapon color square

Single digit at x=18 y=28, right of the 4x4 weapon color indicator.
Reads g_state.weapon_level (1..4)."
```

---

## Task 4: 시각 검증

```bash
./run.sh
```

확인 사항:
- 시작 시 weapon level = 1 (HUD에 1 표시)
- P 픽업할 때마다 레벨 +1 (1 → 2 → 3 → 4 → cap)
- 무기별 시각 변화:
  - **Vulcan**: 1탄 → 3탄(평행) → 5탄(부채꼴) → 7탄
  - **Laser**: 얇은빔 → 굵은빔(2개) → 3빔 → 5빔
  - **Plasma**: 1볼 → 2스택 → 4볼 (스택+사이드) → 6볼
- 다른 색 P 먹으면 무기 전환 + 레벨 +1
- 잔기 0 콘티뉴 시 weapon_level이 1로 리셋 (state_reset_for_continue 동작)

---

## Self-Review Checklist

- [ ] **Spec coverage**: §4.3 메인 무기 L1~L4
- [ ] 레벨업 cap = 4
- [ ] HUD 표시
- [ ] CONTINUE 시 reset

---

## Open Items (Plan 10+)

- 휘는 Laser (curving beams)
- 호밍 미사일 자동 발사 (Z 홀드 시)
- 메달 시스템 + B/H/1UP 픽업
- 패럴랙스 클라우드
- 2면, 3면 + 보스
