/* A 16-bit texture put in the PVR's twiddled order in main RAM, to be copied
 * to VRAM in one go (render_pvr.c): pure arithmetic, kept apart so the host
 * can test it against KallistiOS's own formula (tests/pvr_tile_test.c).
 *
 * KallistiOS's pvr_txr_load_ex works the twiddled address out bit by bit
 * for every pixel and writes VRAM 16 bits at a time across the bus: some
 * 150 ms for a 512 x 512 texture on the console. Here the address is two
 * table lookups (the x part and the y part) and a store into RAM; the
 * result goes to VRAM by store queues (pvr_txr_load).
 *
 * The order: in squares of min(w, h), one after another along the longer
 * side; inside a square the bits of y and x interleaved, y's the even ones. */
#ifndef DC2_PVR_TWIDDLE_H
#define DC2_PVR_TWIDDLE_H

/* the bits of i spread to the even places (i < 1024) */
static inline unsigned pvr_twid_spread(unsigned i)
{
    unsigned r = 0;
    for (int b = 0; b < 10; b++) r |= ((i >> b) & 1u) << (2 * b);
    return r;
}

/* src (w x h pixels, a row every pitch pixels) into dst (w * h pixels,
   twiddled); w, h powers of two from 8 to 1024 */
static inline void pvr_twiddle16(const unsigned short *src, unsigned pitch, unsigned w, unsigned h, unsigned short *dst)
{
    static unsigned tab[1024];
    static int made;
    if (!made) { for (unsigned i = 0; i < 1024; i++) tab[i] = pvr_twid_spread(i); made = 1; }
    unsigned min = w < h ? w : h, mask = min - 1, sq = min * min;
    unsigned col[1024];
    for (unsigned x = 0; x < w; x++) col[x] = (tab[x & mask] << 1) + (x / min) * sq;
    for (unsigned y = 0; y < h; y++) {
        const unsigned short *row = src + y * pitch;
        unsigned short *out = dst + tab[y & mask] + (y / min) * sq;
        for (unsigned x = 0; x < w; x++) out[col[x]] = row[x];
    }
}

/* The same order made a tile at a time, so that no buffer the size of the
 * texture is needed (a 512 x 512 one would take 512 KB of RAM): an aligned
 * T x T square of the texture (T a power of two, up to 32) is T * T pixels
 * in a row in the twiddled order, at T * T times the tile's own twiddled
 * index. fetch(ctx, x, y, n, out) gives n pixels of row y from column x;
 * emit(ctx, at, tile, n) takes n twiddled pixels for pixel offset at
 * (a multiple of n: 32-byte aligned for the larger tiles). */
typedef void (*pvr_fetch_fn)(void *ctx, unsigned x, unsigned y, unsigned n, unsigned short *out);
typedef void (*pvr_emit_fn)(void *ctx, unsigned at, const unsigned short *tile, unsigned n);

static inline void pvr_twiddle_tiles(unsigned w, unsigned h, pvr_fetch_fn fetch, pvr_emit_fn emit, void *ctx)
{
    static unsigned short rows[32 * 32], tile[32 * 32] __attribute__((aligned(32)));
    unsigned min = w < h ? w : h, t = min < 32 ? min : 32, k = 0, sq = min * min;
    while ((1u << k) < t) k++;
    unsigned loc[32], loc2[32];
    for (unsigned i = 0; i < t; i++) { loc[i] = pvr_twid_spread(i); loc2[i] = loc[i] << 1; }
    for (unsigned s0 = 0; s0 < (w > h ? w : h) / min; s0++)          /* the squares along the longer side */
        for (unsigned ty = 0; ty < min / t; ty++)
            for (unsigned tx = 0; tx < min / t; tx++) {
                unsigned x0 = tx * t + (w > h ? s0 * min : 0), y0 = ty * t + (w > h ? 0 : s0 * min);
                for (unsigned y = 0; y < t; y++) fetch(ctx, x0, y0 + y, t, rows + y * t);
                for (unsigned y = 0; y < t; y++) {
                    unsigned short *o = tile + loc[y];
                    const unsigned short *r = rows + y * t;
                    for (unsigned x = 0; x < t; x += 2) { o[loc2[x]] = r[x]; o[loc2[x + 1]] = r[x + 1]; }
                }
                unsigned at = s0 * sq + ((pvr_twid_spread(ty) | pvr_twid_spread(tx) << 1) << (2 * k));
                emit(ctx, at, tile, t * t);
            }
}
#endif
