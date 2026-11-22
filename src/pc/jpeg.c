/* The rooms' JPEG backgrounds (0x4D2990, IJG libjpeg 6b in the port with an
 * extra output colour space 6: 15-bit RGB, 2 bytes a pixel). A small baseline
 * decoder that gives the same pixels as libjpeg 6b as the port set it up: the
 * floating-point IDCT, fancy (triangle) upsampling with the rows past the top
 * and bottom repeated, YCbCr -> RGB by its tables, then 15 bits, blue high.
 * Only what the game's files are (all 1169 pictures alike): baseline, 8-bit,
 * Y 2x2 + Cb 1x1 + Cr 1x1 in one interleaved scan, no restart markers;
 * anything else is left undecoded. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dc2/types.h"
#include "dc2/gpu.h"
#include "dc2/stall.h"

typedef struct {
    int id, h, v, tq;                 /* sampling factors, quantization table */
    int td, ta, pred;                 /* Huffman tables, DC predictor */
    int bw, bh;                       /* blocks across and down (whole MCUs) */
    int dw, dh;                       /* downsampled width and height (pixels) */
    unsigned char *plane;             /* bw*8 x bh*8 samples */
} Comp;

typedef struct {                      /* one Huffman table (jdhuff.c's derived form) */
    int maxcode[18], valoffset[17];
    unsigned char huffval[256];
    unsigned short look[256];         /* the next 8 bits: length << 8 | value, 0 for a longer code */
    int present;
} Huff;

typedef struct {
    const unsigned char *p, *end;
    unsigned buf; int bits;           /* bit buffer: up to 4 bytes ahead of what is read */
    int rbits;                        /* of its bits, those of the data (the rest: zeros past it) */
    unsigned nbytes;                  /* bytes put in it so far */
    const unsigned char *after[8];    /* p after each of the last of them */
    int marker;                       /* a marker hit: zeros from now on */
    int short_data;                   /* bits were wanted past it: MCUs left empty (jdhuff.c) */
    int eoi;                          /* the data's own EOI was reached */
    unsigned short q[4][64];
    float fq[4][64];                  /* the float IDCT's tables */
    Huff dc[4], ac[4];
    Comp c[4]; int nc;
    int width, height;
} Jpeg;

static const int zigzag[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21,
    28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61,
    54, 47, 55, 62, 63};

static int u16be(const unsigned char *p) { return p[0] << 8 | p[1]; }

/* ---- entropy decoding ----------------------------------------------------------------- */

static void huff_build(Huff *t, const unsigned char *bits, const unsigned char *vals, int nvals)
{
    int code = 0, k = 0;
    memset(t, 0, sizeof *t);
    memcpy(t->huffval, vals, (size_t)nvals);
    for (int l = 1; l <= 16; l++) {
        t->valoffset[l] = k - code;
        k += bits[l - 1]; code += bits[l - 1];
        t->maxcode[l] = bits[l - 1] ? code - 1 : -1;
        code <<= 1;
    }
    t->maxcode[17] = 0x7FFFFFFF;
    t->present = 1;
    for (int b = 0; b < 256; b++) {                /* huff_decode's walk for codes of up to 8 bits */
        int c = b >> 7, l = 1;
        while (l <= 8 && c > t->maxcode[l]) { l++; if (l <= 8) c = c << 1 | (b >> (8 - l) & 1); }
        t->look[b] = (unsigned short)(l <= 8 ? l << 8 | t->huffval[(c + t->valoffset[l]) & 0xFF] : 0);
    }
}

/* The bits of the scan, read a byte at a time as libjpeg reads them; past
 * the scan's end (a marker, or the data's end, where the port's source gives
 * an EOI) zeros, and the data is short from the first of them on. The buffer
 * runs ahead (for the 8-bit Huffman lookahead): rbits says which bits are
 * the data's, after[] where the scan was after each byte, so that the end
 * is found from the byte the last bit read came from (scan_end). */
static void fill(Jpeg *j)
{
    while (j->bits <= 24) {
        int b = -1;
        if (!j->marker && j->p < j->end) {
            const unsigned char *q = j->p;
            b = *q++;
            if (b == 0xFF) {
                while (q < j->end && *q == 0xFF) q++;      /* fill bytes */
                if (q < j->end && *q == 0) j->p = q + 1;    /* a stuffed FF */
                else { j->marker = 1; b = -1; }
            } else j->p = q;
        }
        if (b < 0) b = 0; else if (j->rbits == j->bits) j->rbits += 8;
        j->buf = j->buf << 8 | (unsigned)b; j->bits += 8;
        j->after[j->nbytes++ & 7] = j->p;
    }
}

static inline void skip_bits(Jpeg *j, int n)
{
    j->bits -= n;
    if (n > j->rbits) { j->rbits = 0; j->short_data = 1; } else j->rbits -= n;
}

static inline int get_bit(Jpeg *j)
{
    if (!j->bits) fill(j);
    int v = (int)(j->buf >> (j->bits - 1)) & 1;
    skip_bits(j, 1);
    return v;
}

static int get_bits(Jpeg *j, int n)
{
    if (n > 16) { int v = 0; while (n--) v = v << 1 | get_bit(j); return v; }
    if (j->bits < n) fill(j);
    int v = (int)(j->buf >> (j->bits - n)) & ((1 << n) - 1);
    skip_bits(j, n);
    return v;
}

static int huff_decode(Jpeg *j, const Huff *t)
{
    if (j->bits < 8) fill(j);
    unsigned k = t->look[(j->buf >> (j->bits - 8)) & 0xFF];
    if (k) { skip_bits(j, (int)(k >> 8)); return (int)(k & 0xFF); }
    int code = get_bit(j), l = 1;
    while (code > t->maxcode[l]) { code = code << 1 | get_bit(j); if (++l > 16) return 0; }
    return t->huffval[(code + t->valoffset[l]) & 0xFF];
}

/* where reading the scan a byte at a time would have left it */
static void scan_end(Jpeg *j, const unsigned char *start)
{
    unsigned used = j->nbytes - (unsigned)j->bits / 8;
    j->p = used ? j->after[(used - 1) & 7] : start;
}

static int extend(int v, int s) { return v < (1 << (s - 1)) ? v - (1 << s) + 1 : v; }

/* ---- the floating-point IDCT (jidctflt.c: the port's libjpeg was set to it) ------------
 * The game runs with the x87 at single precision (Direct3D sets it), so every
 * step rounds to a float as here; the tables are float products of doubles. */
/* The table: quantval * aanscalefactor[col] * aanscalefactor[row], each
 * product of the x87 rounded once to 24 bits. Done in integers here, so it
 * comes out the same whatever the host's double is (the Dreamcast's is a
 * float): a * 2^ea times the factor's 53-bit mantissa, rounded to 24 bits. */
typedef unsigned long long u64x;
static const struct { u64x m; int e; } aan_sf[8] = {      /* the factors as m / 2^e */
    {0x10000000000000ull, 52}, {0x163150b14861efull, 52}, {0x14e7ae914d6fcaull, 52}, {0x12d062ef6c11aaull, 52},
    {0x10000000000000ull, 52}, {0x192469c0a7bf3bull, 53}, {0x11517a7bc720bbull, 53}, {0x11a855de72ab5dull, 54}};

static void mul_round24(unsigned *a, int *ea, u64x m, int em)
{
    u64x lo = (u64x)*a * (m & 0xFFFFFFFFu), hi = (u64x)*a * (m >> 32);
    u64x L = lo + (hi << 32), H = (hi >> 32) + (L < lo);       /* the 128-bit product */
    int n = 0;
    for (u64x t = H; t; t >>= 1) n++;
    if (n) n += 64; else for (u64x t = L; t; t >>= 1) n++;
    int sh = n - 24;
    if (sh <= 0) { *a = (unsigned)L; *ea -= em; return; }
    /* shift right by sh (< 64: at most 24 + 53 bits) with round half to even */
    u64x q = (L >> sh) | (H << (64 - sh));
    int half = (int)(L >> (sh - 1)) & 1, sticky = sh > 1 && (L & ((1ull << (sh - 1)) - 1)) != 0;
    if (half && (sticky || (q & 1))) q++;
    if (q >> 24) { q >>= 1; sh++; }                            /* (rounded up to 2^24) */
    *a = (unsigned)q; *ea += sh - em;
}

static void fq_build(float *f, const unsigned short *q)
{
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++) {
            unsigned a = q[r * 8 + c]; int e = 0;
            mul_round24(&a, &e, aan_sf[c].m, aan_sf[c].e);          /* (column first) */
            mul_round24(&a, &e, aan_sf[r].m, aan_sf[r].e);
            f[r * 8 + c] = ldexpf((float)a, e);
        }
}

static unsigned char idct_limit(long v)               /* IDCT_range_limit[v & RANGE_MASK] */
{
    int m = (int)(v & 1023), s = m >= 512 ? m - 1024 : m;
    s += 128;
    return (unsigned char)(s < 0 ? 0 : s > 255 ? 255 : s);
}

/* (objects, not literals: with x87 excess precision a literal keeps all its digits) */
static const float K1_414 = 1.414213562f, K1_847 = 1.847759065f, K1_082 = 1.082392200f, K2_613 = -2.613125930f;
#define OUT(x) idct_limit((((long)(x)) + 4) >> 3)      /* (INT32) truncates, DESCALE(.., 3) */

static void idct_float(const short *in, const float *q, unsigned char *out, int stride)
{
    float ws[64];
    for (int c = 0; c < 8; c++) {
        const short *ip = in + c; const float *qp = q + c; float *w = ws + c;
        if (!ip[8] && !ip[16] && !ip[24] && !ip[32] && !ip[40] && !ip[48] && !ip[56]) {
            float dc = ip[0] * qp[0];
            for (int r = 0; r < 8; r++) w[8 * r] = dc;
            continue;
        }
        float t0 = ip[0] * qp[0], t1 = ip[16] * qp[16], t2 = ip[32] * qp[32], t3 = ip[48] * qp[48];
        float t10 = t0 + t2, t11 = t0 - t2, t13 = t1 + t3, t12 = (t1 - t3) * K1_414 - t13;
        t0 = t10 + t13; t3 = t10 - t13; t1 = t11 + t12; t2 = t11 - t12;
        float t4 = ip[8] * qp[8], t5 = ip[24] * qp[24], t6 = ip[40] * qp[40], t7 = ip[56] * qp[56];
        float z13 = t6 + t5, z10 = t6 - t5, z11 = t4 + t7, z12 = t4 - t7;
        t7 = z11 + z13; t11 = (z11 - z13) * K1_414;
        float z5 = (z10 + z12) * K1_847;
        t10 = K1_082 * z12 - z5; t12 = K2_613 * z10 + z5;
        t6 = t12 - t7; t5 = t11 - t6; t4 = t10 + t5;
        w[0] = t0 + t7; w[56] = t0 - t7; w[8] = t1 + t6; w[48] = t1 - t6;
        w[16] = t2 + t5; w[40] = t2 - t5; w[32] = t3 + t4; w[24] = t3 - t4;
    }
    for (int r = 0; r < 8; r++) {
        const float *w = ws + 8 * r; unsigned char *o = out + r * stride;
        if (w[1] == 0 && w[2] == 0 && w[3] == 0 && w[4] == 0 && w[5] == 0 && w[6] == 0 && w[7] == 0) {
            unsigned char v = OUT(w[0]);                /* (every sum below is w[0] then, exactly) */
            for (int i = 0; i < 8; i++) o[i] = v;
            continue;
        }
        float t10 = w[0] + w[4], t11 = w[0] - w[4], t13 = w[2] + w[6], t12 = (w[2] - w[6]) * K1_414 - t13;
        float t0 = t10 + t13, t3 = t10 - t13, t1 = t11 + t12, t2 = t11 - t12;
        float z13 = w[5] + w[3], z10 = w[5] - w[3], z11 = w[1] + w[7], z12 = w[1] - w[7];
        float t7 = z11 + z13; t11 = (z11 - z13) * K1_414;
        float z5 = (z10 + z12) * K1_847;
        t10 = K1_082 * z12 - z5; t12 = K2_613 * z10 + z5;
        float t6 = t12 - t7, t5 = t11 - t6, t4 = t10 + t5;
        o[0] = OUT(t0 + t7); o[7] = OUT(t0 - t7); o[1] = OUT(t1 + t6); o[6] = OUT(t1 - t6);
        o[2] = OUT(t2 + t5); o[5] = OUT(t2 - t5); o[4] = OUT(t3 + t4); o[3] = OUT(t3 - t4);
    }
}

/* ---- the scan --------------------------------------------------------------------------- */

static int decode_block(Jpeg *j, Comp *c, short *blk)
{
    memset(blk, 0, 64 * sizeof *blk);
    int s = huff_decode(j, &j->dc[c->td]);
    int diff = s ? extend(get_bits(j, s), s) : 0;
    c->pred += diff;
    blk[0] = (short)c->pred;
    for (int k = 1; k < 64; k++) {
        int rs = huff_decode(j, &j->ac[c->ta]), r = rs >> 4;
        s = rs & 15;
        if (s) { k += r; blk[k > 63 ? 63 : zigzag[k]] = (short)extend(get_bits(j, s), s); }   /* (past 63: libjpeg's spare entries are 63) */
        else { if (r != 15) break; k += 15; }
    }
    return 0;
}

/* the one interleaved scan: MCUs of 4 Y blocks, Cb, Cr (jdhuff.c's decode_mcu:
   once the data has run short, MCUs are left empty) */
static void scan(Jpeg *j)
{
    short blk[64];
    int mx = (j->width + 15) / 16, my = (j->height + 15) / 16;
    for (int y = 0; y < my; y++)
        for (int x = 0; x < mx; x++) {
            int skip = j->short_data;
            for (int i = 0; i < 3; i++) {
                Comp *c = &j->c[i];
                for (int by = 0; by < c->v; by++)
                    for (int bx = 0; bx < c->h; bx++) {
                        if (skip) memset(blk, 0, sizeof blk); else decode_block(j, c, blk);
                        int px = (x * c->h + bx) * 8, py = (y * c->v + by) * 8;
                        idct_float(blk, j->fq[c->tq], c->plane + py * (c->bw * 8) + px, c->bw * 8);
                    }
            }
        }
}

/* ---- upsampling (jdsample.c, fancy) ---------------------------------------------------- */

/* the component's row r, the rows past its top and bottom repeated */
static const unsigned char *crow(const Comp *c, int r)
{
    if (r < 0) r = 0;
    if (r >= c->dh) r = c->dh - 1;
    return c->plane + r * (c->bw * 8);
}

static void h2v2_fancy(const unsigned char *in0, const unsigned char *in1, int dw, unsigned char *out)
{
    int this = in0[0] * 3 + in1[0], next = in0[1] * 3 + in1[1], last;
    *out++ = (unsigned char)((this * 4 + 8) >> 4);
    *out++ = (unsigned char)((this * 3 + next + 7) >> 4);
    last = this; this = next;
    for (int i = 2; i < dw; i++) {
        next = in0[i] * 3 + in1[i];
        *out++ = (unsigned char)((this * 3 + last + 8) >> 4);
        *out++ = (unsigned char)((this * 3 + next + 7) >> 4);
        last = this; this = next;
    }
    *out++ = (unsigned char)((this * 3 + last + 8) >> 4);
    *out++ = (unsigned char)((this * 4 + 7) >> 4);
}

/* output row y of chroma component c, upsampled 2x2 */
static void upsample_row(const Comp *c, int y, unsigned char *out)
{
    int r = y >> 1;
    h2v2_fancy(crow(c, r), crow(c, y & 1 ? r + 1 : r - 1), c->dw, out);
}

/* ---- colour --------------------------------------------------------------------------------- */

/* 0..255, by a table (Y + a chroma term is within -256..511) */
static unsigned char g_clamp[768];
#define CLAMP(v) g_clamp[(v) + 256]

/* the decoder: 0 when the stream is not one it knows */
static int jpeg_run(Jpeg *j, gptr dst)
{
    while (j->p + 2 <= j->end) {
        if (j->p[0] != 0xFF) { j->p++; continue; }
        int m = j->p[1];
        j->p += 2;
        if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7) || m == 0xFF) { if (m == 0xFF) j->p--; continue; }
        if (m == 0xD9) { j->eoi = 1; break; }
        if (j->p + 2 > j->end) break;
        int len = u16be(j->p);
        const unsigned char *seg = j->p + 2, *next = j->p + len;
        if (len < 2 || next > j->end) break;
        switch (m) {
        case 0xDB:                                     /* quantization tables */
            for (const unsigned char *s = seg; s < next;) {
                int pq = s[0] >> 4, tq = s[0] & 3; s++;
                for (int k = 0; k < 64; k++) { j->q[tq][zigzag[k]] = (unsigned short)(pq ? u16be(s) : s[0]); s += pq ? 2 : 1; }
                fq_build(j->fq[tq], j->q[tq]);
            }
            break;
        case 0xC4:                                     /* Huffman tables */
            for (const unsigned char *s = seg; s < next;) {
                int tc = s[0] >> 4, th = s[0] & 3, n = 0;
                for (int k = 0; k < 16; k++) n += s[1 + k];
                huff_build(tc ? &j->ac[th] : &j->dc[th], s + 1, s + 17, n);
                s += 17 + n;
            }
            break;
        case 0xDD: if (u16be(seg)) return 0; break;   /* (restart markers: none in the game) */
        case 0xC0: case 0xC1: {                        /* the frame */
            j->height = u16be(seg + 1); j->width = u16be(seg + 3); j->nc = seg[5];
            if (seg[0] != 8 || j->nc != 3 || j->c[0].plane || j->width < 16) return 0;
            for (int i = 0; i < 3; i++) {
                Comp *c = &j->c[i];
                c->id = seg[6 + 3 * i]; c->h = seg[7 + 3 * i] >> 4; c->v = seg[7 + 3 * i] & 15; c->tq = seg[8 + 3 * i] & 3;
                if (c->h != (i ? 1 : 2) || c->v != (i ? 1 : 2)) return 0;
            }
            int mx = (j->width + 15) / 16, my = (j->height + 15) / 16;
            for (int i = 0; i < 3; i++) {
                Comp *c = &j->c[i];
                c->bw = mx * c->h; c->bh = my * c->v;
                c->dw = i ? (j->width + 1) / 2 : j->width; c->dh = i ? (j->height + 1) / 2 : j->height;
                c->plane = calloc((size_t)c->bw * 8, (size_t)c->bh * 8);
                if (!c->plane) return 0;
            }
            break; }
        case 0xDA: {                                   /* a scan, to the next marker */
            if (seg[0] != 3 || !j->c[0].plane) return 0;
            for (int i = 0; i < 3; i++) {
                if (seg[1 + 2 * i] != j->c[i].id) return 0;
                j->c[i].td = seg[2 + 2 * i] >> 4 & 3; j->c[i].ta = seg[2 + 2 * i] & 3; j->c[i].pred = 0;
            }
            j->p = next; j->bits = j->rbits = 0; j->nbytes = 0; j->marker = 0; j->short_data = 0;
            scan(j);
            scan_end(j, next);
            while (j->p + 1 < j->end && !(j->p[0] == 0xFF && j->p[1] != 0 && !(j->p[1] >= 0xD0 && j->p[1] <= 0xD7))) j->p++;
            continue; }
        default:
            if ((m >= 0xC2 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC)) return 0;   /* progressive etc. */
            break;
        }
        j->p = next;
    }
    if (!j->c[0].plane) return 0;
    /* tables of jdcolor.c */
    static int crr[256], cbb[256], crg[256], cbg[256], built;
    if (!built) {
        for (int i = 0; i < 256; i++) {
            int x = i - 128;
            crr[i] = (91881 * x + 32768) >> 16; cbb[i] = (116130 * x + 32768) >> 16;
            crg[i] = -46802 * x; cbg[i] = -22554 * x + 32768;
        }
        for (int i = 0; i < 768; i++) g_clamp[i] = (unsigned char)(i < 256 ? 0 : i > 511 ? 255 : i - 256);
        built = 1;
    }
    unsigned char *r0 = malloc((size_t)j->width * 3 + 16), *r1 = r0 + j->width + 8, *r2 = r1 + j->width + 4;   /* (h2v2 writes 2*dw) */
    if (!r0) return 0;
    for (int y = 0; y < j->height; y++) {
        u8 *o = MEM(dst + (u32)(y * j->width * 2));
        const unsigned char *yr = crow(&j->c[0], y);
        upsample_row(&j->c[1], y, r1);
        upsample_row(&j->c[2], y, r2);
        for (int x = 0; x < j->width; x++) {
            int Y = yr[x], cb = r1[x], cr = r2[x];
            int r = CLAMP(Y + crr[cr]), g = CLAMP(Y + ((cbg[cb] + crg[cr]) >> 16)) & 0xF8, b = CLAMP(Y + cbb[cb]) & 0xF8;
            unsigned v = ((b << 5 | g) << 2) | r >> 3;                       /* blue high (the PC's texture order) */
            o[2 * x] = (u8)v; o[2 * x + 1] = (u8)(v >> 8);                  /* (little-endian, as U16) */
        }
    }
    free(r0);
    return 1;
}

/* decode the JPEG at src (size bytes) to dst; mode 2: 15-bit RGB (the only one the game uses) */
void DC2CALL PcJpeg_Decode(gptr dst, gptr src, u32 size, u32 mode)  /* 0x4D2990 */
{
    if (mode != 2) return;
    Jpeg *j = calloc(1, sizeof *j);
    if (!j) return;
    STALL_BEGIN();
    j->p = (const unsigned char *)MEM(src); j->end = j->p + size;
    jpeg_run(j, dst);
    if (!j->eoi) { U8(0x87B728) = 0xFF; U8(0x87B729) = 0xD9; }  /* the port's source ran dry: its fake EOI */
    for (int i = 0; i < 4; i++) free(j->c[i].plane);
    free(j);
    STALL_END(STALL_JPEG);
}
