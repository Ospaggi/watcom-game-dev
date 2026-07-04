/*
 * MOB.CPP - Player/monster state, bump combat, per-turn monster AI.
 *
 * Monsters chase greedily (major axis first) while inside the player's
 * lit FOV — visibility is mutual — and wander randomly otherwise.
 * Damage rolls are atk + rand()%3 - 1, floored at 1.
 */
#include <stdio.h>
#include <stdlib.h>
#include "DUNGEON.H"
#include "MOB.H"
#include "HUD.H"

Mob mobs[MAX_MOBS];
int nmobs = 0;
Player pl;

const MobDef mob_defs[9] = {
    { "Rat",         4, 1, 0 },   /* 1층 */
    { "Spider",       3, 2, 1 },
    { "Goblin",     6, 2, 2 },   /* 2층 */
    { "Skeleton",   8, 3, 3 },
    { "Orc",      12, 4, 4 },   /* 3층 */
    { "Slime",    10, 3, 5 },
    { "Dark Knight", 16, 5, 6 },  /* 4층 */
    { "Wizard",    12, 5, 7 },
    { "Dragon",    35, 5, 8 },   /* 보스 */
};

void player_reset(void)
{
    pl.hp = pl.maxhp = 24;
    pl.atk = 3;
    pl.potions = 1;
    pl.gold = 0;
}

static int mob_add(int kind, int x, int y)
{
    Mob *m;
    if (nmobs >= MAX_MOBS) return 0;
    m = &mobs[nmobs++];
    m->x = x;
    m->y = y;
    m->kind = kind;
    m->hp = m->maxhp = mob_defs[kind].hp;
    m->atk = mob_defs[kind].atk;
    m->alive = 1;
    return 1;
}

void mob_spawn_floor(int floor)
{
    static const int counts[4] = { 5, 6, 7, 6 };
    int want = counts[floor - 1];
    int i, t;

    nmobs = 0;
    for (i = 0; i < want; i++) {
        for (t = 0; t < 50; t++) {
            Room *r = &dun_rooms[1 + rand() % (dun_nrooms - 1)];
            int x = r->x + rand() % r->w;
            int y = r->y + rand() % r->h;
            if (dun_map[y][x] != T_FLOOR) continue;
            if (mob_at(x, y)) continue;
            mob_add((floor - 1) * 2 + rand() % 2, x, y);
            break;
        }
    }
    if (floor == 4) {
        Room *lr = &dun_rooms[dun_nrooms - 1];
        int bx = lr->x + lr->w / 2, by = lr->y + lr->h / 2;
        Mob *m = mob_at(bx, by);
        if (m) m->alive = 0;    /* boss takes the spot */
        mob_add(KIND_BOSS, bx, by);
    }
}

Mob *mob_at(int x, int y)
{
    int i;
    for (i = 0; i < nmobs; i++)
        if (mobs[i].alive && mobs[i].x == x && mobs[i].y == y)
            return &mobs[i];
    return 0;
}

static int roll_dmg(int atk)
{
    int d = atk + rand() % 3 - 1;
    return (d < 1) ? 1 : d;
}

/* 을/를 selection by the final jongseong of a UTF-8 Korean name. */
static const char *eul_reul(const char *s)
{
    unsigned int cp = 0;
    const unsigned char *p = (const unsigned char *)s;

    while (*p) {
        if ((*p & 0xF0) == 0xE0)      { cp = ((p[0] & 0x0F) << 12) |
                                             ((p[1] & 0x3F) << 6) |
                                              (p[2] & 0x3F); p += 3; }
        else if ((*p & 0xE0) == 0xC0) { cp = ((p[0] & 0x1F) << 6) |
                                              (p[1] & 0x3F); p += 2; }
        else cp = *p++;
    }
    if (cp >= 0xAC00 && cp <= 0xD7A3 && (cp - 0xAC00) % 28 != 0)
        return "을";
    return "를";
}

int player_attack(Mob *m)
{
    char buf[64];
    int d = roll_dmg(pl.atk);
    const char *nm = mob_defs[m->kind].name;

    m->hp -= d;
    if (m->hp <= 0) {
        m->alive = 0;
        sprintf(buf, "You beat a %s!", nm);
        hud_msg(buf);
        return 1;
    }
    sprintf(buf, "%d Damage to a %s!", mob_defs[m->kind].name, d);
    hud_msg(buf);
    return 0;
}

static void mob_attack_player(Mob *m)
{
    char buf[64];
    int d = roll_dmg(m->atk);

    pl.hp -= d;
    sprintf(buf, "%s's attack! %d Damage", mob_defs[m->kind].name, d);
    hud_msg(buf);
}

void mob_act_all(void)
{
    static const int d4[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    int i, nx, ny;

    for (i = 0; i < nmobs; i++) {
        Mob *m = &mobs[i];
        if (!m->alive) continue;

        if (dun_vis[m->y][m->x] == VIS_LIT) {
            int dx = (pl.x > m->x) - (pl.x < m->x);
            int dy = (pl.y > m->y) - (pl.y < m->y);
            if (abs(pl.x - m->x) >= abs(pl.y - m->y)) { nx = m->x + dx; ny = m->y; }
            else                                       { nx = m->x; ny = m->y + dy; }
            if (!(nx == pl.x && ny == pl.y) &&
                (!dun_walkable(nx, ny) || mob_at(nx, ny))) {
                if (nx != m->x) { nx = m->x; ny = m->y + dy; }
                else            { nx = m->x + dx; ny = m->y; }
            }
            if (nx == pl.x && ny == pl.y) {
                mob_attack_player(m);
                continue;
            }
        } else {
            int r = rand() % 4;
            nx = m->x + d4[r][0];
            ny = m->y + d4[r][1];
            if (nx == pl.x && ny == pl.y) continue;
        }

        if (dun_walkable(nx, ny) && !mob_at(nx, ny)) {
            m->x = nx;
            m->y = ny;
        }
    }
}
