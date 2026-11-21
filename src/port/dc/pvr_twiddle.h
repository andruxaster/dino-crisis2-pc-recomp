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
#endif
