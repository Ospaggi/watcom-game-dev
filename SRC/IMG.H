#ifndef IMG_H_INCLUDED
#define IMG_H_INCLUDED

#define IMG_OK   0
#define IMG_ERR -1

typedef struct {
    int w;
    int h;
    unsigned char *pix;  /* heap-allocated raw indexed bytes */
} Img;

/* Full-screen 320x200 raw indexed file (no header, exactly 64000 bytes). */
int  img_load_fs(const char *path, Img *out);

/* Variable-size raw indexed: 4-byte header { uint16 w; uint16 h; } + pixels. */
int  img_load(const char *path, Img *out);

void img_free(Img *out);

/* Blit full image at (x,y), opaque, clipped to screen. */
void img_blit(const Img *im, int x, int y);

/* Vertical-scroll blit: positions the source image with its column 0 at
 * screen column dx0, copying rows starting at src_y (wrapping vertically
 * through im->h), writing into back-buffer rows [dy0..dy1). Output is
 * clipped at dx0 (left), dx1 (right), dy0 (top), dy1 (bottom), and never
 * runs past the image width. Used by the gameplay scene for the scrolling
 * background strip — pass dx0 = play-area left edge, dx1 = right edge. */
void img_blit_vscroll(const Img *im, int dx0, int dx1,
                      int dy0, int dy1, int src_y);

/* Same as img_blit_vscroll but treats palette index 0 as transparent —
 * pixels with index 0 are not written. Slower than the opaque path
 * (per-pixel branch) but preserves what's already in the back-buffer.
 * Used for parallax overlays like the cloud layer. */
void img_blit_vscroll_trans(const Img *im, int dx0, int dx1,
                            int dy0, int dy1, int src_y);

/* Copy a sub-rectangle (sx, sy, w, h) from `im` to the back-buffer at
 * (dx, dy). Index-0 pixels are skipped (transparent). The destination
 * is clipped to the screen; the source rect is read directly without
 * wrapping (caller ensures it fits inside im->w x im->h). */
void img_blit_rect_trans(const Img *im, int dx, int dy,
                         int sx, int sy, int w, int h);

#endif
