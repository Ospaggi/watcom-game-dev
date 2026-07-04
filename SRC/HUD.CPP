/*
 * HUD.CPP - Right status panel and bottom message line.
 *
 * Panel occupies x=240..319 of the 320x200 screen; the message line sits
 * under the 15x11-tile viewport at y=176..199. Korean text renders via
 * the 16x16 bitmap font (FONT.CPP).
 */
#include <stdio.h>
#include <string.h>
#include "GFX.H"
#include "FONT.H"
#include "MOB.H"
#include "HUD.H"

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

    gfx_fill_rect(240, 0, 80, 176, 2);
    gfx_vline(240, 0, 175, 8);

    sprintf(buf, "B%d Floor", floor);
    font_puts(246, 6, buf, 15);

    sprintf(buf, "HP %d/%d", pl.hp > 0 ? pl.hp : 0, pl.maxhp);
    font_puts(246, 32, buf, 15);
    gfx_rect(245, 50, 71, 8, 15);
    w = (pl.hp > 0) ? 69 * pl.hp / pl.maxhp : 0;
    if (w > 0) gfx_fill_rect(246, 51, w, 6, 24);

    sprintf(buf, "ATK %d", pl.atk);
    font_puts(246, 66, buf, 15);

    sprintf(buf, "Potion x%d", pl.potions);
    font_puts(246, 92, buf, 11);

    sprintf(buf, "Gold %d", pl.gold);
    font_puts(246, 118, buf, 58);

    font_puts(246, 152, "P:Potion", 8);

    gfx_fill_rect(0, 176, 320, 24, 0);
    font_puts(4, 180, msgbuf, 14);
}
