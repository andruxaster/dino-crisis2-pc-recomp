/* Large textures in pieces for the PVR (render_pvr.c): pure arithmetic, kept
 * apart so the host can test it (tests/pvr_tile_test.c).
 *
 * The PVR's twiddled textures are powers of two on each side, so a 640 x 480
 * surface padded whole takes a 1024 x 512 texture (1 MB). Split into 512 +
 * 128 columns (each 512 rows) it takes 640 KB. A triangle over a split
 * texture is cut along the pieces' edges in texture space; the cut is
 * perspective-correct: u * rhw and rhw are linear on the screen, so is
 * u * rhw - U * rhw, the cut's plane. */
#ifndef DC2_PVR_TILE_H
#define DC2_PVR_TILE_H

#define TILE_MAX 3                      /* pieces per side at most */

static inline unsigned tile_pow2(unsigned n) { unsigned p = 8; while (p < n) p <<= 1; return p; }

/* n texels in pieces: off[k] the first texel, size[k] the power-of-two size
   in VRAM (the last may be padded; off[count] = n). A side is split only
   where padding would waste more than an eighth of it (and not under 64). */
static inline int tile_split(unsigned n, unsigned short *off, unsigned short *size)
{
    int k = 0;
    unsigned o = 0;
    while (o < n) {
        unsigned r = n - o, p = tile_pow2(r);
        off[k] = (unsigned short)o;
        if (k == TILE_MAX - 1 || r <= 64 || p - r <= r / 8) { size[k++] = (unsigned short)p; break; }
        size[k++] = (unsigned short)(p >> 1);
        o += p >> 1;
    }
    off[k] = (unsigned short)n;
    return k;
}

/* a vertex being cut: position, rhw, u * rhw, v * rhw, the colour (a r g b)
   and the fog factor, all linear on the screen */
typedef struct { float x, y, w, uw, vw, c[4], fog; } TileV;

static inline void tilev_lerp(TileV *o, const TileV *a, const TileV *b, float t)
{
    o->x = a->x + (b->x - a->x) * t;  o->y = a->y + (b->y - a->y) * t;
    o->w = a->w + (b->w - a->w) * t;
    o->uw = a->uw + (b->uw - a->uw) * t;  o->vw = a->vw + (b->vw - a->vw) * t;
    for (int i = 0; i < 4; i++) o->c[i] = a->c[i] + (b->c[i] - a->c[i]) * t;
    o->fog = a->fog + (b->fog - a->fog) * t;
}

/* the polygon (n vertices) kept where s * (coord - lim * w) >= 0, coord the
   u (axis 0) or v (1) times rhw; the result's count (at most n + 1) */
static inline int tile_clip_plane(const TileV *in, int n, TileV *out, int axis, float lim, float s)
{
    int m = 0;
    for (int i = 0; i < n; i++) {
        const TileV *a = &in[i], *b = &in[(i + 1) % n];
        float fa = s * ((axis ? a->vw : a->uw) - lim * a->w);
        float fb = s * ((axis ? b->vw : b->uw) - lim * b->w);
        if (fa >= 0) out[m++] = *a;
        if ((fa >= 0) != (fb >= 0)) tilev_lerp(&out[m++], a, b, fa / (fa - fb));
    }
    return m;
}

/* a triangle cut to u in [u0, u1], v in [v0, v1] (texture 0..1): the
   polygon's count (0, or 3..7) in out; tmp: 8 vertices of room */
static inline int tile_clip(const TileV tri[3], float u0, float u1, float v0, float v1, TileV *out, TileV *tmp)
{
    int n = tile_clip_plane(tri, 3, tmp, 0, u0, 1.0f);
    n = tile_clip_plane(tmp, n, out, 0, u1, -1.0f);
    n = tile_clip_plane(out, n, tmp, 1, v0, 1.0f);
    n = tile_clip_plane(tmp, n, out, 1, v1, -1.0f);
    return n >= 3 ? n : 0;
}
#endif
