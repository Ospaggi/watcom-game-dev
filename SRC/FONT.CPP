#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FONT.H"
#include "GFX.H"

/* ---- compiled font binary data ---- */

static unsigned char *_bin_data = NULL;
static unsigned long  _bin_size = 0;
static int            _num_glyphs = 0;
static unsigned char *_bin_index = NULL;
static unsigned char *_bin_code  = NULL;

/* ---- call compiled glyph: EDI=dst, ESI=code, EAX=color4 ---- */

void _draw_cfont(void *dst, const void *code, unsigned long color4);
#pragma aux _draw_cfont = \
    "call esi" \
    parm [edi] [esi] [eax] \
    modify [edi esi eax];

/* ---- file helpers ---- */

static unsigned char *_load_alloc(const char *name, unsigned long *out_size) {
    FILE *f;
    long sz;
    unsigned char *buf;
    f = fopen(name, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc(sz);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, sz, f);
    fclose(f);
    *out_size = (unsigned long)sz;
    return buf;
}

/* ---- init / close ---- */

void font_init(void) {
    unsigned long magic;

    _bin_data = _load_alloc("FONT.BIN", &_bin_size);
    if (_bin_data && _bin_size >= 8) {
        memcpy(&magic, _bin_data, 4);
        if (magic == 0x544E4643UL) {  /* "CFNT" */
            _num_glyphs = _bin_data[4] | (_bin_data[5] << 8);
            _bin_index = _bin_data + 8;
            _bin_code  = _bin_index + _num_glyphs * 10;
        }
    }
}

void font_close(void) {
    if (_bin_data) { free(_bin_data); _bin_data = NULL; }
    _num_glyphs = 0;
}

/* ---- binary search ---- */

static unsigned char *_find_glyph(unsigned long cp) {
    int lo = 0, hi = _num_glyphs - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        unsigned char *entry = _bin_index + mid * 10;
        unsigned long ecp;
        memcpy(&ecp, entry, 4);
        if (ecp == cp) {
            unsigned long off;
            memcpy(&off, entry + 4, 4);
            return _bin_code + off;
        }
        if (ecp < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

/* ---- UTF-8 decoder ---- */

static unsigned long _utf8_decode(const char **pp) {
    unsigned char c = (unsigned char)*(*pp)++;
    unsigned long cp;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) {
        cp = (unsigned long)(c & 0x1F) << 6;
        cp |= (unsigned long)(*(*pp)++ & 0x3F);
        return cp;
    }
    if ((c & 0xF0) == 0xE0) {
        cp = (unsigned long)(c & 0x0F) << 12;
        cp |= (unsigned long)(*(*pp)++ & 0x3F) << 6;
        cp |= (unsigned long)(*(*pp)++ & 0x3F);
        return cp;
    }
    if ((c & 0xF8) == 0xF0) (*pp) += 3;
    return '?';
}

/* ---- public API ---- */

void font_puts(int x, int y, const char *str, unsigned char color) {
    unsigned long cp, color4;
    unsigned char *code;
    int gw;

    /* clip: glyphs are 16px tall; rendering past screen corrupts adjacent memory */
    if (y < 0 || y + FONT_ENG_H > GFX_H) return;

    color4 = (unsigned long)color;
    color4 |= color4 << 8;
    color4 |= color4 << 16;

    while (*str) {
        cp = _utf8_decode(&str);
        if (cp == 0) break;
        gw = (cp >= 0xAC00UL && cp <= 0xD7A3UL) ? FONT_HAN_W : FONT_ENG_W;

        if (x >= GFX_W) break;
        if (x >= 0 && x + gw <= GFX_W) {
            /* runtime uses only the compiled FONT.BIN glyphs; a codepoint
             * that was never compiled (not used in any source literal) simply
             * draws nothing. */
            code = _find_glyph(cp);
            if (code) _draw_cfont(gfx_buffer() + y * GFX_W + x, code, color4);
        }

        x += gw;
    }
}

int font_text_width(const char *str) {
    int w = 0;
    unsigned long cp;
    while (*str) {
        cp = _utf8_decode(&str);
        if (cp == 0) break;
        w += (cp >= 0xAC00UL && cp <= 0xD7A3UL) ? FONT_HAN_W : FONT_ENG_W;
    }
    return w;
}
