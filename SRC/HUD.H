#ifndef HUD_H_INCLUDED
#define HUD_H_INCLUDED

void hud_msg(const char *s);   /* set the one-line message (max 63 chars) */
void hud_draw(int floor);      /* right panel (x=240..319) + bottom line */

#endif
