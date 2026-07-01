/*
 * GAME.CPP - Engine smoke test.
 *
 * Minimal main() that brings up every retained subsystem and proves the
 * base is healthy before a new game is built on top:
 *   - timer / input / sound(OPL2) / sfx(SB) / gfx(Mode 13h) / font
 *   - uploads the 256-color palette and draws all 15 ramps as shaded bars
 *   - renders Hangul + English text via the bitmap font
 *   - plays a VGM track (loops), and fires a PCM sample on SPACE
 *
 * Init order matters: timer -> input -> snd -> sfx -> gfx (sound chains
 * through INT 8 to the timer ISR). Shutdown is the exact reverse.
 */
#include "GFX.H"
#include "INPUT.H"
#include "TIMER.H"
#include "SOUND.H"
#include "SFX.H"
#include "FONT.H"
#include "PALETTE.H"

#define BGM_TRACK "TYRIAN.VGM"
#define SFX_BEEP  0

/* Draw the 15 color ramps (palette indices 16..255) as vertical bars,
 * each column a ramp, each of 16 rows a shade. Visual palette sanity check. */
static void draw_palette_bars(void)
{
    int r, s;
    int bar_w = 18;
    int x0 = 10;
    int y0 = 44;
    for (r = 0; r < 15; r++) {
        for (s = 0; s < 16; s++) {
            unsigned char idx = (unsigned char)(16 + r * 16 + s);
            gfx_fill_rect(x0 + r * bar_w, y0 + s * 6, bar_w - 1, 6, idx);
        }
    }
}

int main(void)
{
    unsigned long last, now;
    int px, dir;

    /* --- init (order is load-bearing) --- */
    timer_init();
    input_init();
    snd_init();
    sfx_init();
    gfx_init();
    gfx_set_palette_all(game_palette);
    font_init();

    /* --- load assets --- */
    snd_load(BGM_TRACK);
    snd_play();
    sfx_load(SFX_BEEP, "FIRE.SFX");

    px = 40;
    dir = 1;
    last = timer_ms();

    while (1) {
        now = timer_ms();
        if (now - last >= 16) {          /* ~60 Hz logic tick */
            last = now;
            px += dir * 2;
            if (px < 4 || px > GFX_W - 24) dir = -dir;
        }

        /* BGM keeps looping. */
        if (!snd_playing()) snd_play();

        /* SPACE fires the PCM sample; ESC quits. */
        if (input_read_key() == KEY_SPACE) sfx_play(SFX_BEEP);
        if (input_key(KEY_ESC)) break;

        /* --- render --- */
        gfx_clear(0);
        font_puts(10, 8, "WATCOM ENGINE BASE READY", 15);
        font_puts(10, 24, "왓콤 엔진 베이스 준비 완료", 15);
        draw_palette_bars();

        /* animated bar proves gfx + timing are alive */
        gfx_fill_rect(px, 150, 20, 10, 15);
        gfx_rect(px, 150, 20, 10, 1);

        font_puts(10, 175, "스페이스: 효과음    ESC: 종료", 8);

        gfx_vsync();
        gfx_flip();
    }

    /* --- shutdown (reverse of init) --- */
    snd_stop();
    font_close();
    gfx_close();
    sfx_close();
    snd_close();
    input_close();
    timer_close();
    return 0;
}
