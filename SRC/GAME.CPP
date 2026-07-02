/*
 * GAME.CPP - Dungeon crawler main: state machine + turn loop.
 *
 * States: menu -> play (floors 1..4) -> ending / dead -> menu.
 * Turn-based: the screen is redrawn only when a turn was consumed or the
 * state changed, so a 486 never breaks a sweat. Music loops through the
 * OPL2 VGM player; PCM effects go through the SB mixer.
 *
 * AUTOCAP=1 builds a self-driving capture run (no input) used for visual
 * verification during development; set to 0 for the real game.
 */
#define AUTOCAP 0

#include <stdio.h>
#include <stdlib.h>
#include "GFX.H"
#include "INPUT.H"
#include "TIMER.H"
#include "SOUND.H"
#include "SFX.H"
#include "FONT.H"
#include "SPRITE.H"
#include "SCRNCAP.H"
#include "PALETTE.H"
#include "DUNGEON.H"
#include "MOB.H"
#include "HUD.H"

#define KEY_P 0x19
#define KEY_F10 0x44

/* sfx slots */
#define SFX_S_ATK   0
#define SFX_S_HIT   1
#define SFX_S_ITEM  2
#define SFX_S_STAIR 3
#define SFX_S_WIN   4
#define SFX_S_LOSE  5

#define ST_MENU    0
#define ST_PLAY    1
#define ST_DEAD    2
#define ST_ENDING  3
#define ST_CREDITS 4

#define VIEW_TW 15
#define VIEW_TH 11

static Sprite spr_tiles;
static Sprite spr_mobs;
static int game_state = ST_MENU;
static int floor_no = 1;
static int dirty = 1;

/* dun_map tile -> TILES.SPR frame (wall=1, floor=0, stairs=2) */
static const int tile_frame[3] = { 1, 0, 2 };

static void bgm(const char *file)
{
    snd_stop();
    if (snd_load(file) == SND_OK) snd_play();
}

static void new_floor(int f)
{
    char buf[32];

    dun_generate(f, &pl.x, &pl.y);
    dun_spawn_items(f);
    mob_spawn_floor(f);
    dun_fov(pl.x, pl.y);

    switch (f) {
    case 1: bgm("ST01.VGM"); break;
    case 2: bgm("ST02.VGM"); break;
    case 3: bgm("ST03.VGM"); break;
    default: bgm("ST06.VGM"); break;
    }
    sprintf(buf, "지하 %d층에 내려왔다.", f);
    hud_msg(buf);
    dirty = 1;
}

static void start_game(void)
{
    player_reset();
    floor_no = 1;
    new_floor(1);
    game_state = ST_PLAY;
}

/* ---- rendering ---- */

static void render_play(void)
{
    int camx = pl.x - VIEW_TW / 2;
    int camy = pl.y - VIEW_TH / 2;
    int x, y, i;

    if (camx < 0) camx = 0;
    if (camx > MAP_W - VIEW_TW) camx = MAP_W - VIEW_TW;
    if (camy < 0) camy = 0;
    if (camy > MAP_H - VIEW_TH) camy = MAP_H - VIEW_TH;

    gfx_clear(0);
    for (y = 0; y < VIEW_TH; y++) {
        for (x = 0; x < VIEW_TW; x++) {
            int mx = camx + x, my = camy + y;
            if (dun_vis[my][mx] == VIS_UNSEEN) continue;
            spr_draw_clipped(&spr_tiles, tile_frame[dun_map[my][mx]],
                             x * 16, y * 16);
        }
    }
    for (i = 0; i < dun_nitems; i++) {
        Item *it = &dun_items[i];
        if (it->taken || dun_vis[it->y][it->x] == VIS_UNSEEN) continue;
        if (it->x < camx || it->x >= camx + VIEW_TW ||
            it->y < camy || it->y >= camy + VIEW_TH) continue;
        spr_draw_clipped(&spr_tiles, 3 + it->kind,
                         (it->x - camx) * 16, (it->y - camy) * 16);
    }
    for (i = 0; i < nmobs; i++) {
        Mob *m = &mobs[i];
        if (!m->alive || dun_vis[m->y][m->x] == VIS_UNSEEN) continue;
        if (m->x < camx || m->x >= camx + VIEW_TW ||
            m->y < camy || m->y >= camy + VIEW_TH) continue;
        spr_draw_clipped(&spr_mobs, mob_defs[m->kind].frame,
                         (m->x - camx) * 16, (m->y - camy) * 16);
    }
    /* the dim ring reads half-bright: checkerboard dither over the tile
       and anything standing on it */
    for (y = 0; y < VIEW_TH; y++)
        for (x = 0; x < VIEW_TW; x++)
            if (dun_vis[camy + y][camx + x] == VIS_DIM)
                gfx_dither_rect(x * 16, y * 16, 16, 16);
    spr_draw_clipped(&spr_mobs, 9, (pl.x - camx) * 16, (pl.y - camy) * 16);

    hud_draw(floor_no);
    gfx_vsync();
    gfx_flip();
}

static void render_menu(void)
{
    gfx_clear(0);
    spr_draw_clipped(&spr_mobs, 8, 120, 40);
    spr_draw_clipped(&spr_mobs, 9, 184, 40);
    font_puts(112, 70, "던전 크롤러", 15);
    font_puts(64, 100, "지하 4층의 드래곤을 잡아라", 7);
    font_puts(60, 136, "방향키/WASD: 이동과 공격", 8);
    font_puts(60, 152, "P: 물약    ESC: 종료", 8);
    font_puts(92, 180, "ENTER: 게임 시작", 14);
    gfx_vsync();
    gfx_flip();
}

static void render_dead(void)
{
    char buf[48];

    gfx_clear(0);
    font_puts(96, 60, "당신은 쓰러졌다...", 24);
    sprintf(buf, "도달: 지하 %d층    금화 %d", floor_no, pl.gold);
    font_puts(72, 100, buf, 7);
    font_puts(96, 150, "ENTER: 메뉴로", 14);
    gfx_vsync();
    gfx_flip();
}

static void render_ending(void)
{
    char buf[48];

    gfx_clear(0);
    spr_draw_clipped(&spr_mobs, 9, 152, 36);
    font_puts(88, 64, "드래곤을 물리쳤다!", 58);
    font_puts(64, 92, "던전에 평화가 찾아왔다.", 15);
    sprintf(buf, "획득한 금화: %d닢", pl.gold);
    font_puts(88, 124, buf, 58);
    font_puts(96, 164, "ENTER: 크레딧", 14);
    gfx_vsync();
    gfx_flip();
}

/* ---- scrolling ending credits ---- */

typedef struct {
    const char *text;
    unsigned char color;
} CreditLine;

static const CreditLine credit_lines[] = {
    { "던전 크롤러",                58 },
    { "",                           0 },
    { "개발",                       11 },
    { "QuickBASIC",                 15 },
    { "",                           0 },
    { "그래픽",                     11 },
    { "DawnLike 타일셋",            15 },
    { "DragonDePlatino",            7 },
    { "팔레트 DawnBringer",         7 },
    { "",                           0 },
    { "효과음",                     11 },
    { "Juhani Junkala",             15 },
    { "",                           0 },
    { "음악",                       11 },
    { "Ospaggi (도스박물관)",       15 },
    { "",                           0 },
    { "(C) 2026 QuickBASIC",        8 },
    { "",                           0 },
    { "플레이해 주셔서 감사합니다!", 14 },
};
#define N_CREDITS ((int)(sizeof(credit_lines) / sizeof(credit_lines[0])))
#define CREDIT_LINE_H   24
#define CREDIT_SPEED_MS 30  /* ms per scrolled pixel */

/* Draw the roll at the given scroll offset (0 = all below the screen).
 * Returns 1 while any line is still visible or on its way, 0 when done. */
static int render_credits(int scroll)
{
    int i, y, alive = 0;

    gfx_clear(0);
    for (i = 0; i < N_CREDITS; i++) {
        y = GFX_H + i * CREDIT_LINE_H - scroll;
        if (y > -CREDIT_LINE_H) alive = 1;
        if (!credit_lines[i].text[0]) continue;
        font_puts((GFX_W - font_text_width(credit_lines[i].text)) / 2,
                  y, credit_lines[i].text, credit_lines[i].color);
    }
    gfx_vsync();
    gfx_flip();
    return alive;
}

/* ---- play logic ----
 * Returns 0 = no turn, 1 = turn consumed, 2 = turn consumed but skip the
 * monster phase (descended stairs / game state changed).
 */

static void pickup(void)
{
    int i;

    for (i = 0; i < dun_nitems; i++) {
        Item *it = &dun_items[i];
        if (it->taken || it->x != pl.x || it->y != pl.y) continue;
        it->taken = 1;
        sfx_play(SFX_S_ITEM);
        switch (it->kind) {
        case IT_POTION:
            pl.potions++;
            hud_msg("물약을 주웠다.");
            break;
        case IT_SWORD:
            pl.atk += 2;
            hud_msg("검이 날카로워졌다! 공격 +2");
            break;
        default:
            pl.gold += 20;
            hud_msg("금화 20닢을 주웠다!");
            break;
        }
    }
}

static int play_turn(int key)
{
    int dx = 0, dy = 0, nx, ny;
    Mob *m;

    switch (key) {
    case KEY_UP:    case KEY_W: dy = -1; break;
    case KEY_DOWN:  case KEY_S: dy = 1;  break;
    case KEY_LEFT:  case KEY_A: dx = -1; break;
    case KEY_RIGHT: case KEY_D: dx = 1;  break;
    case KEY_P:
        if (pl.potions > 0 && pl.hp < pl.maxhp) {
            pl.potions--;
            pl.hp += 10;
            if (pl.hp > pl.maxhp) pl.hp = pl.maxhp;
            hud_msg("물약을 마셨다. HP +10");
            sfx_play(SFX_S_ITEM);
            return 1;
        }
        hud_msg(pl.potions > 0 ? "체력이 가득 찼다." : "물약이 없다!");
        dirty = 1;
        return 0;
    default:
        return 0;
    }

    nx = pl.x + dx;
    ny = pl.y + dy;

    m = mob_at(nx, ny);
    if (m) {
        sfx_play(SFX_S_ATK);
        if (player_attack(m)) {
            pl.gold += 5;
            if (m->kind == KIND_BOSS) {
                sfx_play(SFX_S_WIN);
                bgm("ST07.VGM");
                game_state = ST_ENDING;
                return 2;
            }
        }
        return 1;
    }

    if (!dun_walkable(nx, ny)) return 0;

    pl.x = nx;
    pl.y = ny;
    pickup();

    if (dun_map[pl.y][pl.x] == T_STAIRS) {
        sfx_play(SFX_S_STAIR);
        floor_no++;
        new_floor(floor_no);
        return 2;
    }
    return 1;
}

/* Run one full game turn for the given key. Returns 1 if turn consumed. */
static int game_turn(int key)
{
    int r = play_turn(key);

    if (r == 0) return 0;
    if (r == 1) {
        int hp_before = pl.hp;
        mob_act_all();
        if (pl.hp < hp_before) sfx_play(SFX_S_HIT);
        if (pl.hp <= 0) {
            sfx_play(SFX_S_LOSE);
            snd_stop();
            game_state = ST_DEAD;
        }
    }
    dun_fov(pl.x, pl.y);
    dirty = 1;
    return 1;
}

/* ---- main ---- */

#if AUTOCAP
static int dir_key_toward(int tx, int ty)
{
    if (tx > pl.x) return KEY_RIGHT;
    if (tx < pl.x) return KEY_LEFT;
    if (ty > pl.y) return KEY_DOWN;
    return KEY_UP;
}

static void autocap_run(void)
{
    static const int d4[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    int i, x, y, sx = -1, sy = -1;

    srand(999);

    render_menu();
    scrncap_save_bmp("MENU.BMP");

    start_game();
    render_play();
    scrncap_save_bmp("PLAY0.BMP");

    /* teleport next to the first mob to verify mob rendering */
    for (i = 0; i < 4; i++) {
        int nx = mobs[0].x + d4[i][0], ny = mobs[0].y + d4[i][1];
        if (dun_walkable(nx, ny) && !mob_at(nx, ny)) {
            pl.x = nx;
            pl.y = ny;
            break;
        }
    }
    dun_fov(pl.x, pl.y);
    render_play();
    scrncap_save_bmp("PLAY1.BMP");

    /* bump-attack until it dies: combat message + damage + gold */
    for (i = 0; i < 8 && mobs[0].alive && game_state == ST_PLAY; i++)
        game_turn(dir_key_toward(mobs[0].x, mobs[0].y));
    render_play();
    scrncap_save_bmp("PLAY2.BMP");

    /* corridor FOV check: teleport onto a floor cell outside every room */
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) {
            int r, in_room = 0;
            if (dun_map[y][x] != T_FLOOR) continue;
            for (r = 0; r < dun_nrooms; r++) {
                Room *rm = &dun_rooms[r];
                if (x >= rm->x && x < rm->x + rm->w &&
                    y >= rm->y && y < rm->y + rm->h) { in_room = 1; break; }
            }
            if (in_room) continue;
            pl.x = x;
            pl.y = y;
            y = MAP_H;
            break;
        }
    }
    dun_fov(pl.x, pl.y);
    render_play();
    scrncap_save_bmp("CORR.BMP");

    /* step onto the stairs -> floor 2 (new map, HUD floor number) */
    for (y = 0; y < MAP_H && sy < 0; y++)
        for (x = 0; x < MAP_W; x++)
            if (dun_map[y][x] == T_STAIRS) { sx = x; sy = y; break; }
    if (sy >= 0 && game_state == ST_PLAY) {
        for (i = 0; i < 4; i++) {
            int nx = sx + d4[i][0], ny = sy + d4[i][1];
            if (dun_walkable(nx, ny) && !mob_at(nx, ny)) {
                pl.x = nx;
                pl.y = ny;
                break;
            }
        }
        dun_fov(pl.x, pl.y);
        game_turn(dir_key_toward(sx, sy));
        render_play();
    }
    scrncap_save_bmp("PLAY3.BMP");

    game_state = ST_DEAD;
    render_dead();
    scrncap_save_bmp("DEAD.BMP");

    /* cheat-jump to floor 4, kill the boss through the real code path */
    game_state = ST_PLAY;
    floor_no = 4;
    new_floor(4);
    pl.atk = 99;
    for (i = 0; i < nmobs; i++) {
        if (mobs[i].alive && mobs[i].kind == KIND_BOSS) {
            int b = i, d;
            for (d = 0; d < 4; d++) {
                int nx = mobs[b].x + d4[d][0], ny = mobs[b].y + d4[d][1];
                if (dun_walkable(nx, ny) && !mob_at(nx, ny)) {
                    pl.x = nx;
                    pl.y = ny;
                    break;
                }
            }
            dun_fov(pl.x, pl.y);
            render_play();
            scrncap_save_bmp("BOSS.BMP");
            game_turn(dir_key_toward(mobs[b].x, mobs[b].y));
            break;
        }
    }
    if (game_state == ST_ENDING) render_ending();
    scrncap_save_bmp("ENDING.BMP");

    /* two snapshots of the credits roll: top block and bottom block */
    render_credits(200);
    scrncap_save_bmp("CREDIT1.BMP");
    render_credits(460);
    scrncap_save_bmp("CREDIT2.BMP");
}
#endif

int main(void)
{
    timer_init();
    input_init();
    snd_init();
    sfx_init();
    gfx_init();
    gfx_set_palette_all(game_palette);
    font_init();

    if (spr_load("TILES.SPR", &spr_tiles) != SPR_OK ||
        spr_load("MOBS.SPR", &spr_mobs) != SPR_OK) {
        gfx_close();
        sfx_close();
        snd_close();
        input_close();
        timer_close();
        printf("SPR load failed\n");
        return 1;
    }

    sfx_load(SFX_S_ATK,   "ATK.SFX");
    sfx_load(SFX_S_HIT,   "HIT.SFX");
    sfx_load(SFX_S_ITEM,  "ITEM.SFX");
    sfx_load(SFX_S_STAIR, "STAIR.SFX");
    sfx_load(SFX_S_WIN,   "WIN.SFX");
    sfx_load(SFX_S_LOSE,  "LOSE.SFX");

    srand((unsigned)timer_ms() + 7919);

#if AUTOCAP
    autocap_run();
#else
    bgm("ST00.VGM");
    input_flush();

    {
    unsigned long credits_t0 = 0;
    int credits_scroll = -1;

    while (1) {
        int key;

        if (!snd_playing()) snd_play();
        key = input_read_key();

        if (game_state == ST_MENU) {
            if (dirty) { render_menu(); dirty = 0; }
            if (key == KEY_ENTER) start_game();
            else if (key == KEY_ESC) break;
        } else if (game_state == ST_PLAY) {
            if (dirty) { render_play(); dirty = 0; }
            if (key == KEY_ESC) {
                game_state = ST_MENU;
                bgm("ST00.VGM");
                dirty = 1;
            } else if (key == KEY_F10) {
                render_play();
                scrncap_save_bmp("CAP.BMP");
            } else if (key > 0) {
                game_turn(key);
            }
        } else if (game_state == ST_ENDING) {
            if (dirty) { render_ending(); dirty = 0; }
            if (key == KEY_ENTER) {
                game_state = ST_CREDITS;
                credits_t0 = timer_ms();
                credits_scroll = -1;
            } else if (key == KEY_ESC) {
                game_state = ST_MENU;
                bgm("ST00.VGM");
                dirty = 1;
            }
        } else if (game_state == ST_CREDITS) {
            int s = (int)((timer_ms() - credits_t0) / CREDIT_SPEED_MS);
            if (s != credits_scroll) {
                credits_scroll = s;
                if (!render_credits(s)) key = KEY_ENTER;  /* roll finished */
            }
            if (key == KEY_ENTER || key == KEY_ESC) {
                game_state = ST_MENU;
                bgm("ST00.VGM");
                dirty = 1;
            }
        } else {  /* ST_DEAD */
            if (dirty) { render_dead(); dirty = 0; }
            if (key == KEY_ENTER || key == KEY_ESC) {
                game_state = ST_MENU;
                bgm("ST00.VGM");
                dirty = 1;
            }
        }
    }
    }
#endif

    snd_stop();
    font_close();
    gfx_close();
    sfx_close();
    snd_close();
    input_close();
    timer_close();
    return 0;
}
