#ifndef MOB_H_INCLUDED
#define MOB_H_INCLUDED

#define MAX_MOBS  16
#define KIND_BOSS 8

typedef struct {
    int x, y;
    int hp, maxhp, atk;
    int kind;      /* index into mob_defs */
    int alive;
} Mob;

typedef struct {
    const char *name;
    int hp, atk;
    int frame;     /* frame in MOBS.SPR */
} MobDef;

typedef struct {
    int x, y;
    int hp, maxhp, atk;
    int potions, gold;
} Player;

extern Mob mobs[MAX_MOBS];
extern int nmobs;
extern const MobDef mob_defs[9];
extern Player pl;

void player_reset(void);          /* HP 20/20, ATK 3, potion 1, gold 0 */
void mob_spawn_floor(int floor);  /* rooms 1..N-1; floor 4 adds the boss */
Mob *mob_at(int x, int y);
void mob_act_all(void);           /* one turn: chase if lit, else wander */
int  player_attack(Mob *m);       /* returns 1 if the mob died */

#endif
