#ifndef DUNGEON_H_INCLUDED
#define DUNGEON_H_INCLUDED

#define MAP_W 40
#define MAP_H 30

#define T_WALL   0
#define T_FLOOR  1
#define T_STAIRS 2

#define VIS_UNSEEN 0
#define VIS_DIM    1    /* dim ring beyond full sight - dithered render */
#define VIS_LIT    2

#define MAX_ROOMS 10

typedef struct {
    int x, y, w, h;
} Room;

extern unsigned char dun_map[MAP_H][MAP_W];
extern unsigned char dun_vis[MAP_H][MAP_W];
extern Room dun_rooms[MAX_ROOMS];
extern int  dun_nrooms;

/* Generate floor layout, return player start. Floor 4 has no stairs. */
void dun_generate(int floor, int *px, int *py);
int  dun_walkable(int x, int y);
void dun_fov(int px, int py);   /* shadowcasting FOV, updates dun_vis */

/* ---- items ---- */
#define IT_POTION 0
#define IT_SWORD  1
#define IT_GOLD   2
#define MAX_ITEMS 12

typedef struct {
    int x, y, kind, taken;
} Item;

extern Item dun_items[MAX_ITEMS];
extern int  dun_nitems;

void dun_spawn_items(int floor);

#endif
