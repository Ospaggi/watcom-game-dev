#ifndef SPRITE_H_INCLUDED
#define SPRITE_H_INCLUDED

#define SPR_OK   0
#define SPR_ERR -1

typedef struct {
    int w;
    int h;
    int num_frames;
    unsigned char **frames;       /* compiled-code pointers (always present) */
    unsigned char **raw_frames;   /* raw indexed pixels (NULL if .RAW absent) */
    void *_data;                  /* compiled-code block */
    void *_raw_data;              /* raw-pixel block (NULL if absent) */
} Sprite;

int  spr_load(const char *filename, Sprite *spr);
void spr_free(Sprite *spr);

/* Draw frame `fi` of `spr` at top-left (sx, sy). Picks the fast compiled
 * path when fully on-screen, falls back to clipped raw blit when partially
 * off-screen, and returns silently when fully off-screen or when the
 * sprite has no raw companion and is partially off-screen. */
void spr_draw_clipped(const Sprite *spr, int fi, int sx, int sy);

/* Build a duplicate of `src` with every opaque pixel overwritten by
 * the given palette index. Used for the white hit-flash silhouette so
 * the non-transparent shape of the sprite is preserved (unlike a plain
 * bounding-box fill). Caller must spr_free(dst) when done. Returns
 * SPR_OK or SPR_ERR. */
int  spr_dup_tinted(const Sprite *src, unsigned char color, Sprite *dst);

#endif
