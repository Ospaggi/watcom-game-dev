/*
 * DUNGEON.CPP - Procedural floor generation + field of view.
 *
 * Rooms-and-corridors: up to MAX_ROOMS random non-overlapping rooms,
 * each connected to the previous room's center by an L-shaped corridor.
 * FOV is recursive shadowcasting (8 octants) with integer fraction
 * slopes - no floating point. Radius-5 disc is fully lit; the 2-tile
 * ring beyond it is VIS_DIM (rendered dithered); the rest stays black.
 */
#include <stdlib.h>
#include <string.h>
#include "DUNGEON.H"

unsigned char dun_map[MAP_H][MAP_W];
unsigned char dun_vis[MAP_H][MAP_W];
Room dun_rooms[MAX_ROOMS];
int  dun_nrooms = 0;

Item dun_items[MAX_ITEMS];
int  dun_nitems = 0;

void dun_generate(int floor, int *px, int *py)
{
    int i, x, y, tries;

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
                r.y < o->y + o->h + 1 && o->y < r.y + r.h + 1) {
                ok = 0;
                break;
            }
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
            dun_map[cy][cx] = T_FLOOR;
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

int dun_walkable(int x, int y)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return 0;
    return dun_map[y][x] != T_WALL;
}

/*
 * Recursive shadowcasting (Bjorn Bergstrom / RogueBasin), 8 octants.
 * Slopes are kept as integer fractions with positive denominators so
 * no floating point is needed.
 */
#define FOV_RADIUS  7   /* casting range: bright disc + 2-tile dim ring */
#define FOV_BRIGHT2 25  /* dist^2 <= 25 -> fully lit */

/* octant transform rows: xx, xy, yx, yy */
static const int oct_xx[8] = { 1,  0,  0, -1, -1,  0,  0,  1 };
static const int oct_xy[8] = { 0,  1, -1,  0,  0, -1,  1,  0 };
static const int oct_yx[8] = { 0,  1,  1,  0,  0, -1, -1,  0 };
static const int oct_yy[8] = { 1,  0,  0,  1, -1,  0,  0, -1 };

static int opaque(int x, int y)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return 1;
    return dun_map[y][x] == T_WALL;
}

static void lite(int x, int y, int d2)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return;
    if (d2 <= FOV_BRIGHT2) dun_vis[y][x] = VIS_LIT;
    else if (dun_vis[y][x] != VIS_LIT) dun_vis[y][x] = VIS_DIM;
}

/* slope a/b < c/d with b, d > 0 */
#define SLOPE_LT(an, ad, cn, cd) ((an) * (cd) < (cn) * (ad))

static void cast_octant(int px, int py, int row,
                        int sn, int sd, int en, int ed, int oct)
{
    int j, dx, dy;

    if (SLOPE_LT(sn, sd, en, ed)) return;    /* start < end */

    for (j = row; j <= FOV_RADIUS; j++) {
        int blocked = 0;
        int nsn = sn, nsd = sd;              /* new start while blocked */
        dy = -j;
        for (dx = -j; dx <= 0; dx++) {
            /* cell corner slopes as fractions with positive denominators:
               l = (dx-0.5)/(dy+0.5) -> (1-2dx)/(-2dy-1)
               r = (dx+0.5)/(dy-0.5) -> (-2dx-1)/(1-2dy) */
            int ln = 1 - 2 * dx,  ld = -2 * dy - 1;
            int rn = -2 * dx - 1, rd = 1 - 2 * dy;
            int mx = px + dx * oct_xx[oct] + dy * oct_xy[oct];
            int my = py + dx * oct_yx[oct] + dy * oct_yy[oct];

            if (SLOPE_LT(sn, sd, rn, rd)) continue;   /* start < r */
            if (SLOPE_LT(ln, ld, en, ed)) break;      /* end > l  */

            if (dx * dx + dy * dy <= FOV_RADIUS * FOV_RADIUS)
                lite(mx, my, dx * dx + dy * dy);

            if (blocked) {
                if (opaque(mx, my)) {
                    nsn = rn; nsd = rd;
                } else {
                    blocked = 0;
                    sn = nsn; sd = nsd;
                }
            } else if (opaque(mx, my) && j < FOV_RADIUS) {
                blocked = 1;
                cast_octant(px, py, j + 1, sn, sd, ln, ld, oct);
                nsn = rn; nsd = rd;
            }
        }
        if (blocked) break;
    }
}

void dun_fov(int px, int py)
{
    int i;

    memset(dun_vis, VIS_UNSEEN, sizeof(dun_vis));
    lite(px, py, 0);
    for (i = 0; i < 8; i++)
        cast_octant(px, py, 1, 1, 1, 0, 1, i);
}

void dun_spawn_items(int floor)
{
    static const int kinds[6] = {
        IT_POTION, IT_POTION, IT_SWORD, IT_GOLD, IT_GOLD, IT_GOLD
    };
    int i, t;

    (void)floor;
    dun_nitems = 0;
    for (i = 0; i < 6 && dun_nitems < MAX_ITEMS; i++) {
        for (t = 0; t < 50; t++) {
            Room *r = &dun_rooms[1 + rand() % (dun_nrooms - 1)];
            int x = r->x + rand() % r->w;
            int y = r->y + rand() % r->h;
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
