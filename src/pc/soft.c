/* The PC port's software rasterizers (asm, 0x4C3C81..0x4C61C0): what draws
 * into the back buffer when there is no 3D device (0x75DEF0 bit 0; the
 * drawers in drawot.c fill their parameters at 0x75CEC0).
 *
 * Polygons are scan-converted into per-row edge tables (left/right x in
 * 16.16, with u, v or three colour channels at each end), then each row is
 * drawn by a span routine picked from a table by the pixel format (16, 24 or
 * 32 bits), MMX or not, and the blending (opaque or abr 0-3). The setup
 * (0x4C3C81) writes the chosen routines' addresses into game memory; here
 * those addresses select the C copies, so the memory stays the original's.
 *
 * Copied instruction for instruction where it shows: 16-bit pixels go through
 * the port's converters to 8:8:8 and back (0x73C978 / 0x73C97C, by the green
 * mask), the non-MMX additive blends fill a channel that carried but leave the
 * carry bit, the subtractive ones clear more than the channel that borrowed,
 * the MMX ones saturate (and only the MMX average modulates the texture by
 * the vertex colour). The state the asm keeps in game memory (edge tables,
 * steps, the destination and texture as 32-bit words) is kept there too. */
#include <string.h>
#include "dc2/types.h"
#include "dc2/pcdraw.h"

/* the state at 0x73C92C.. */
#define S_TEX    0x73C92Cu      /* the texture's pixels (a host pointer, as a word) */
#define S_DST    0x73C930u      /* the back buffer's pixels */
#define S_EU     0x73C934u      /* an edge's u, v, x at its first row (16.16) */
#define S_EV     0x73C938u
#define S_EX     0x73C93Cu
#define S_DU     0x73C940u      /* a span's u, v steps; an edge's */
#define S_DV     0x73C944u
#define S_DX     0x73C948u      /* an edge's x step */
#define S_C0     0x73C94Cu      /* a gouraud edge's colours at its first row (16.16) */
#define S_C1     0x73C950u
#define S_C2     0x73C954u
#define S_D0     0x73C958u      /* their steps (an edge's, then a span's) */
#define S_D1     0x73C95Cu
#define S_D2     0x73C960u
#define S_N      0x73C964u      /* a textured edge's rows left */
#define S_MINY   0x73C968u      /* the first row drawn (0x7FFF: none) */
#define S_FLAT   0x73C96Cu      /* the flat colour; 16-bit: as 8:8:8 at 0x73C970 */
#define S_FLAT8  0x73C970u
#define S_MOD    0x73C974u      /* the vertex colour (8:8:8, 6 bits each) */
#define S_CIN    0x73C978u      /* 16-bit pixel -> 8:8:8, and back */
#define S_COUT   0x73C97Cu
#define S_BASE   0x73C980u      /* the span tables' index: format * 5 (+ 15 with MMX) */
#define T_L      0x73C984u      /* per row (480): left x, right x (16.16; 0x7FFF0000 / 0x80000000 when empty) */
#define T_R      0x73D104u
#define T_LU     0x73D884u      /* textured: u, v at the left and right ends */
#define T_LV     0x73E004u
#define T_RU     0x73E784u
#define T_RV     0x73EF04u
#define T_LC0    0x73F684u      /* gouraud: three colours at the left and right ends */
#define T_LC1    0x73FE04u
#define T_LC2    0x740584u
#define T_RC0    0x740D04u
#define T_RC1    0x741484u
#define T_RC2    0x741C04u
#define P_PACK   0x742384u      /* the vertex colour packer (by the format's channel order, 0x742388) */
#define P_GLYPH  0x742390u      /* the 28x28 glyph copy (0x742394) */
#define P_TEX    0x7423ACu      /* the chosen spans: textured (0x7423B0), flat (0x74242C), gouraud (0x7424A8) */
#define P_FLAT   0x742428u
#define P_GOUR   0x7424A4u
#define PITCH    0x87BB3Cu
#define PIXBITS  0x87BB42u
#define ROWS     0x780u         /* 480 rows x 4 */

int g_soft_mmx = -1;            /* MMX present: -1 asks the CPU (0x4C2A60) */

static u8 *g_dst;
static const u8 *g_tex;

static u32 shld(u32 d, u32 s, int n) { return d << n | s >> (32 - n); }
static u32 ror8(u32 v) { return v >> 8 | v << 24; }
static u32 rol8(u32 v) { return v << 8 | v >> 24; }

/* ---- pixel formats -------------------------------------------------------------------------- */

/* 16-bit pixel (ax) <-> 8:8:8 (edx): 0x4C3D28 / 0x4C3D73 (555), 0x4C3D41 / 0x4C3D86 (565),
   0x4C3D5A / 0x4C3D99 (444); the registers as the asm leaves them */
static void cin(u32 *eax, u32 *edx)
{
    u32 a = *eax, d = *edx;
    switch (U32(S_CIN)) {
    case 0x4C3D41: d = shld(d, a, 24); a <<= 21; d = shld(d, a, 8); a <<= 6; d = shld(d, a, 8); d &= 0xF8FCF8; break;
    case 0x4C3D5A: d = shld(d, a, 28); a <<= 24; d = shld(d, a, 8); a <<= 4; d = shld(d, a, 8); d &= 0xF0F0F0; break;
    default:       d = shld(d, a, 25); a <<= 22; d = shld(d, a, 8); a <<= 5; d = shld(d, a, 8); d &= 0xF8F8F8; break;
    }
    *eax = a; *edx = d;
}
static void cout(u32 *eax, u32 *edx)
{
    u32 a = *eax, d = *edx;
    switch (U32(S_COUT)) {
    case 0x4C3D86: a = shld(a, d, 13); d <<= 16; a = shld(a, d, 6); d <<= 8; a = shld(a, d, 5); break;
    case 0x4C3D99: a = shld(a, d, 12); d <<= 16; a = shld(a, d, 4); d <<= 8; a = shld(a, d, 4); break;
    default:       a = shld(a, d, 13); d <<= 16; a = shld(a, d, 5); d <<= 8; a = shld(a, d, 5); break;
    }
    *eax = a; *edx = d;
}

/* three channels (in bits 16-23 of eax, 8-15 of ebx, 0-7 of edx: 16.16 colours or
   bytes) as a pixel of the format: 0x4C3DAC */
static u32 to_pixel(u32 eax, u32 ebx, u32 edx)
{
    return (eax >> (U32(0x87BB44) & 31) & U32(0x87BB50)) | (ebx >> (U32(0x87BB48) & 31) & U32(0x87BB54))
         | (edx >> (U32(0x87BB4C) & 31) & U32(0x87BB58));
}

/* the same as 8:8:8, 6 bits each, in the format's channel order: 0x4C3DE3 / 0x4C3E01 */
static u32 pack(u32 eax, u32 ebx, u32 edx)
{
    if (U32(P_PACK) == 0x4C3E01) return (eax >> 16 & 0xFC) | (ebx >> 8 & 0xFC00) | (edx & 0xFC0000);
    return (eax & 0xFC0000) | (ebx >> 8 & 0xFC00) | (edx >> 16 & 0xFC);
}

/* ---- blending ------------------------------------------------------------------------------- */

/* non-MMX: a channel that carried filled (the carry bit stays) */
static u32 carry_fill(u32 v)
{
    if (v & 0x1000000) v |= 0xFF0000;
    if (v & 0x10000) v |= 0xFF00;
    if (v & 0x100) v |= 0xFF;
    return v;
}

/* non-MMX: d - s by bytes; a borrow clears that byte and the top one (the third
   channel's also clears the second: done on the value rotated); s rotated too */
static u32 sub3(u32 d, u32 *s)
{
    u32 a = *s, t;
    t = (d & 0xFF) - (a & 0xFF); d = (d & ~0xFFu) | (t & 0xFF); if (t & 0x100) d &= 0xFFFF00;
    t = (d >> 8 & 0xFF) - (a >> 8 & 0xFF); d = (d & ~0xFF00u) | (t & 0xFF) << 8; if (t & 0x100) d &= 0xFF00FF;
    a = ror8(a); d = ror8(d);
    t = (d >> 8 & 0xFF) - (a >> 8 & 0xFF); d = (d & ~0xFF00u) | (t & 0xFF) << 8; if (t & 0x100) d &= 0xFF0000FF;
    *s = a;
    return rol8(d);
}

/* MMX, by bytes: paddusb, psubusb */
static u32 addus(u32 a, u32 b)
{
    u32 r = 0;
    for (int k = 0; k < 32; k += 8) { u32 v = (a >> k & 0xFF) + (b >> k & 0xFF); r |= (v > 0xFF ? 0xFF : v) << k; }
    return r;
}
static u32 subus(u32 a, u32 b)
{
    u32 r = 0;
    for (int k = 0; k < 32; k += 8) { u32 x = a >> k & 0xFF, y = b >> k & 0xFF; r |= (x > y ? x - y : 0) << k; }
    return r;
}
/* the bytes as words times the vertex colour's (low 16 bits) >> 7 */
static u32 modw(u32 v, u32 m, int k) { return (u16)((v >> k & 0xFF) * (m >> k & 0xFF)) >> 7; }
/* MMX: packuswb (words, here never negative) */
static u32 sat8(u32 w) { return w > 0xFF ? 0xFF : w; }
/* the texel modulated and averaged with the destination (pmullw, psrlw 7, paddusw, psrlw 1) */
static u32 mod_avg(u32 d, u32 t)
{
    u32 r = 0, m = U32(S_MOD);
    for (int k = 0; k < 32; k += 8) {
        u32 s = (d >> k & 0xFF) + modw(t, m, k);
        r |= sat8((s > 0xFFFF ? 0xFFFF : s) >> 1) << k;
    }
    return r;
}
static u32 mod_only(u32 t)                      /* (glyphs) */
{
    u32 r = 0, m = U32(S_MOD);
    for (int k = 0; k < 32; k += 8) r |= sat8(modw(t, m, k)) << k;
    return r;
}

static void put24(u8 *d, u32 v) { d[0] = (u8)v; d[1] = (u8)(v >> 8); d[2] = (u8)(v >> 16); }
static u32 get32(const u8 *s) { u32 v; memcpy(&v, s, 4); return v; }
static u16 get16(const u8 *s) { u16 v; memcpy(&v, s, 2); return v; }
static void put16(u8 *d, u32 v) { u16 w = (u16)v; memcpy(d, &w, 2); }
static void put32(u8 *d, u32 v) { memcpy(d, &v, 4); }

/* a blend of the destination (8:8:8) with a source colour, by the span's kind:
   1 average, 2 add, 3 subtract, 4 add a quarter (non-MMX); 6 add, 7 subtract,
   8 add a quarter (MMX) */
static u32 blend(int op, u32 d, u32 s)
{
    switch (op) {
    case 1: return (d + s) >> 1;
    case 2: return carry_fill(d + s);
    case 3: return sub3(d, &s);
    case 4: return carry_fill(d + (s >> 2));
    case 6: return addus(d, s);
    case 7: return subus(d, s);
    case 8: return addus(d, s >> 2);
    }
    return s;
}

/* ---- spans -------------------------------------------------------------------------------- */

/* the span routines: format (2, 3, 4 bytes), blend (0 opaque; 5 the MMX textured average) */
typedef struct { u32 addr; u8 bytes, op; } Span;

/* textured (0x7423B0) */
static const Span k_tex[] = {
    {0x4C4769, 2, 0}, {0x4C47B6, 2, 1}, {0x4C481D, 2, 2}, {0x4C48AC, 2, 3}, {0x4C4938, 2, 4},
    {0x4C49CA, 2, 5}, {0x4C4A5C, 2, 6}, {0x4C4ACB, 2, 7}, {0x4C4B3A, 2, 8},
    {0x4C4BAC, 3, 0}, {0x4C4C02, 3, 1}, {0x4C4C63, 3, 2}, {0x4C4CE6, 3, 3}, {0x4C4D6A, 3, 4},
    {0x4C4DF0, 3, 5}, {0x4C4E76, 3, 6}, {0x4C4ED9, 3, 7}, {0x4C4F3C, 3, 8},
    {0x4C4FA2, 4, 0}, {0x4C4FEC, 4, 1}, {0x4C5044, 4, 2}, {0x4C50BE, 4, 3}, {0x4C5139, 4, 4},
    {0x4C51B6, 4, 5}, {0x4C5231, 4, 6}, {0x4C5289, 4, 7}, {0x4C52E1, 4, 8},
};
/* flat (0x74242C) */
static const Span k_flat[] = {
    {0x4C533C, 2, 0}, {0x4C534C, 2, 1}, {0x4C5370, 2, 2}, {0x4C53BC, 2, 3}, {0x4C5405, 2, 4},
    {0x4C5454, 2, 6}, {0x4C5480, 2, 7}, {0x4C54AC, 2, 8},
    {0x4C54DD, 3, 0}, {0x4C54FF, 3, 1}, {0x4C5527, 3, 2}, {0x4C5571, 3, 3}, {0x4C55BF, 3, 4},
    {0x4C560C, 3, 6}, {0x4C5635, 3, 7}, {0x4C565E, 3, 8},
    {0x4C568C, 4, 0}, {0x4C569B, 4, 1}, {0x4C56B6, 4, 2}, {0x4C56F3, 4, 3}, {0x4C5730, 4, 4},
    {0x4C5770, 4, 6}, {0x4C578E, 4, 7}, {0x4C57AC, 4, 8},
};
/* gouraud (0x7424A8) */
static const Span k_gour[] = {
    {0x4C57CF, 2, 0}, {0x4C5807, 2, 1}, {0x4C5855, 2, 2}, {0x4C58CB, 2, 3}, {0x4C593C, 2, 4},
    {0x4C59B5, 2, 6}, {0x4C5A0D, 2, 7}, {0x4C5A65, 2, 8},
    {0x4C5AC0, 3, 0}, {0x4C5B07, 3, 1}, {0x4C5B59, 3, 2}, {0x4C5BCD, 3, 3}, {0x4C5C3F, 3, 4},
    {0x4C5CB6, 3, 6}, {0x4C5D0B, 3, 7}, {0x4C5D60, 3, 8},
    {0x4C5DB8, 4, 0}, {0x4C5DF6, 4, 1}, {0x4C5E3B, 4, 2}, {0x4C5EA2, 4, 3}, {0x4C5F07, 4, 4},
    {0x4C5F71, 4, 6}, {0x4C5FBB, 4, 7}, {0x4C6005, 4, 8},
};

static const Span *span_find(const Span *t, u32 n, u32 addr)
{
    for (u32 i = 0; i < n; i++) if (t[i].addr == addr) return &t[i];
    return 0;                                   /* (the port would jump there) */
}

/* the 24- and 32-bit spans mask the destination before the non-MMX blends */
static u32 dst_word(const u8 *d, int op) { u32 v = get32(d); return op >= 1 && op <= 4 ? v & 0xFCFCFC : v; }

static void put_pixel(u8 *d, u32 bytes, u32 v) { if (bytes == 3) put24(d, v); else put32(d, v); }

/* a textured span: row's left u, v; w pixels from x */
static void span_tex(const Span *s, u8 *row, s32 x, s32 w, u32 r)
{
    u8 *d = row + x * s->bytes;
    u32 u = U32(T_LU + r), v = U32(T_LV + r), du = U32(S_DU), dv = U32(S_DV);
    for (; w; w--, d += s->bytes, u += du, v += dv) {
        if (s->bytes == 2) {
            u32 t = get16(g_tex + ((v >> 7 & 0x1FE00) + (u >> 15 & 0x1FE)));
            if (!t) continue;
            if (!s->op) { put16(d, t); continue; }
            u32 eax = t, edx = 0, ts, ds;
            cin(&eax, &edx); ts = edx;
            if (s->op == 5) {                   /* (MMX: modulated, then averaged) */
                u32 m = 0, mm = U32(S_MOD);
                eax = (eax & 0xFFFF0000) | get16(d);
                cin(&eax, &edx); ds = edx;
                for (int k = 0; k < 32; k += 8) {
                    u32 q = (ds >> k & 0xFF) + modw(ts, mm, k);
                    m |= sat8((q > 0xFFFF ? 0xFFFF : q) >> 1) << k;
                }
                edx = m;
            } else {
                if (s->op == 8) ts >>= 2;       /* (shr edx, 2 before the destination's read) */
                eax = (eax & 0xFFFF0000) | get16(d);
                cin(&eax, &edx); ds = edx;
                if (s->op == 3) { eax = ts; edx = sub3(ds, &eax); }
                else edx = blend(s->op == 8 ? 6 : s->op, ds, ts);
            }
            cout(&eax, &edx);
            put16(d, eax);
            continue;
        }
        u32 t = get32(g_tex + ((v >> 6 & 0x3FC00) + (u >> 14 & 0x3FC)));
        u32 mask = s->op == 0 ? (s->bytes == 3 ? 0xFFFFFF : 0xFFFFFFFF) : s->op >= 5 && s->op <= 7 ? 0xFFFFFF : 0xFCFCFC;
        t &= mask;
        if (!t) continue;
        if (!s->op) { put_pixel(d, s->bytes, t); continue; }
        u32 dw = dst_word(d, s->op);
        u32 o = s->op == 5 ? mod_avg(dw, t) : blend(s->op, dw, t);
        put_pixel(d, s->bytes, o);
    }
}

/* a flat span: the colour at 0x73C96C (16-bit: 0x73C970 as 8:8:8) */
static void span_flat(const Span *s, u8 *row, s32 x, s32 w)
{
    u8 *d = row + x * s->bytes;
    u32 c = U32(S_FLAT);
    if (!s->op) {
        for (; w; w--, d += s->bytes)
            if (s->bytes == 2) put16(d, c);
            else if (s->bytes == 3) { put16(d, c); d[2] = U8(S_FLAT + 2); }
            else put32(d, c);
        return;
    }
    if (s->bytes == 2) {
        u32 c8 = U32(S_FLAT8), eax = 0, edx = 0;
        for (; w; w--, d += 2) {
            eax = (eax & 0xFFFF0000) | get16(d);
            cin(&eax, &edx);
            if (s->op == 3) { eax = c8; edx = sub3(edx, &eax); }
            else edx = blend(s->op, edx, c8);
            cout(&eax, &edx);
            put16(d, eax);
        }
        return;
    }
    for (; w; w--, d += s->bytes) put_pixel(d, s->bytes, blend(s->op, dst_word(d, s->op), c));
}

/* a gouraud span: three colours from the row's left end, stepped */
static void span_gour(const Span *s, u8 *row, s32 x, s32 w, u32 r)
{
    u8 *d = row + x * s->bytes;
    u32 c0 = U32(T_LC0 + r), c1 = U32(T_LC1 + r), c2 = U32(T_LC2 + r);
    u32 d0 = U32(S_D0), d1 = U32(S_D1), d2 = U32(S_D2);
    for (; w; w--, d += s->bytes, c0 += d0, c1 += d1, c2 += d2) {
        if (s->bytes == 2 && !s->op) { put16(d, to_pixel(c0, c1, c2)); continue; }
        u32 c = pack(c0, c1, c2);
        if (s->bytes == 2) {
            /* (the packer leaves eax = c0 & mask: the destination read into its low half) */
            u32 eax = (U32(P_PACK) == 0x4C3E01 ? c0 >> 16 & 0xFC : c0 & 0xFC0000) & 0xFFFF0000;
            u32 edx = U32(P_PACK) == 0x4C3E01 ? c2 & 0xFC0000 : c2 >> 16 & 0xFC;
            eax |= get16(d);
            cin(&eax, &edx);
            if (s->op == 3) { u32 cc = c; edx = sub3(edx, &cc); }
            else edx = blend(s->op, edx, c);
            cout(&eax, &edx);
            put16(d, eax);
            continue;
        }
        if (!s->op) { put_pixel(d, s->bytes, c); continue; }
        put_pixel(d, s->bytes, blend(s->op, dst_word(d, s->op), c));
    }
}

/* ---- edges ---------------------------------------------------------------------------------- */

/* corners are words y | x << 16; rows 0..479; the first row drawn kept at 0x73C968 */
static int edge_start(u32 *a, u32 *b, u32 *n)
{
    u16 dy = (u16)((u16)*b - (u16)*a);
    if (!dy) return 0;
    if (!((s16)*b > (s16)*a)) { u32 t = *a; *a = *b; *b = t; dy = (u16)-dy; }
    *n = dy;
    if ((s16)*b <= 0 || (s16)*a >= 0x1E0) return 0;
    s16 y = (s16)*a;
    if (y < (s16)U16(S_MINY)) U16(S_MINY) = (u16)(y < 0 ? 0 : y);
    return 1;
}
static u32 edge_rows(u32 a, u32 b, u32 *n, u32 *skip)
{
    *skip = 0;
    if ((s16)a < 0) *skip = (u16)(0 - (u16)a);
    *n -= *skip;
    u16 over = (u16)((u16)b - 0x1E0);
    if ((s16)over > 0) *n -= over;
    return ((s16)a < 0 ? 0 : a & 0xFFFF) * 4;
}

static void edge_flat(u32 a, u32 b)                                  /* 0x4C44D0 */
{
    u32 n;
    if (!edge_start(&a, &b, &n)) return;
    U32(S_EX) = a & 0xFFFF0000;
    U32(S_DX) = (u32)((s32)((b & 0xFFFF0000) - (a & 0xFFFF0000)) / (s32)n);
    u32 skip, r = edge_rows(a, b, &n, &skip);
    if (skip) U32(S_EX) += skip * U32(S_DX);
    u32 x = U32(S_EX), dx = U32(S_DX);
    do {
        if ((s32)x < S32(T_L + r)) U32(T_L + r) = x;
        if ((s32)x > S32(T_R + r)) U32(T_R + r) = x;
        x += dx; r += 4;
    } while (--n);
}

/* uv: u << 16 | v */
static void edge_tex(u32 a, u32 b, u32 ua, u32 ub)                   /* 0x4C4336 */
{
    u32 n;
    u16 dy = (u16)((u16)b - (u16)a);
    if (!dy) return;
    if (!((s16)b > (s16)a)) { u32 t = ua; ua = ub; ub = t; }
    u32 c = (ub & 0xFFFF0000) - (ua & 0xFFFF0000);
    c = (c & 0xFFFF0000) | (u16)((u16)ub - (u16)ua);
    if (!edge_start(&a, &b, &n)) return;
    U32(S_EU) = ua & 0xFFFF0000;
    U32(S_EV) = ua << 16;
    u32 q = c & 0xFFFF0000;
    if (q) { s32 v = (s32)(q | 0x8000) / (s32)n; U16(S_EU) = (u16)(v < 0 ? 0xFFFF : 0); q = (u32)v; }
    U32(S_DU) = q;
    q = c << 16;
    if (q) { s32 v = (s32)(q | 0x8000) / (s32)n; U16(S_EV) = (u16)(v < 0 ? 0xFFFF : 0); q = (u32)v; }
    U32(S_DV) = q;
    U32(S_EX) = a & 0xFFFF0000;
    U32(S_DX) = (u32)((s32)((b & 0xFFFF0000) - (a & 0xFFFF0000)) / (s32)n);
    u32 skip, r = edge_rows(a, b, &n, &skip);
    if (skip) {
        U32(S_EU) += skip * U32(S_DU); U32(S_EV) += skip * U32(S_DV); U32(S_EX) += skip * U32(S_DX);
    }
    U32(S_N) = n;
    u32 u = U32(S_EU), du = U32(S_DU), v = U32(S_EV), dv = U32(S_DV), x = U32(S_EX), dx = U32(S_DX);
    do {
        if ((s32)x < S32(T_L + r)) { U32(T_L + r) = x; U32(T_LU + r) = u; U32(T_LV + r) = v; }
        if ((s32)x > S32(T_R + r)) { U32(T_R + r) = x; U32(T_RU + r) = u; U32(T_RV + r) = v; }
        u += du; v += dv; x += dx; r += 4;
    } while (--U32(S_N));
}

/* colours: c0 (byte 0), c1 (byte 1), c2 (byte 2) of the packet's colour words */
static void edge_gour(u32 a, u32 b, u32 ca, u32 cb)                  /* 0x4C45A3 */
{
    u32 n;
    u16 dy = (u16)((u16)b - (u16)a);
    if (!dy) return;
    if (!((s16)b > (s16)a)) { u32 t = ca; ca = cb; cb = t; }
    u32 ecx = (cb & 0xFF00FF) - (ca & 0xFF0000);
    ecx = (ecx & 0xFFFF0000) | (u16)((u16)ecx - (ca & 0xFF));
    u32 edx = (cb & 0xFFFF0000) | (u16)((cb >> 8 & 0xFF) - (ca >> 8 & 0xFF));
    if (!edge_start(&a, &b, &n)) return;
    U32(S_C2) = ca & 0xFF0000;
    U32(S_C1) = (ca >> 8 & 0xFF) << 16;
    U32(S_C0) = (ca & 0xFF) << 16;
    U32(S_D1) = (u32)((s32)(edx << 16) / (s32)n);
    U32(S_D2) = (u32)((s32)(ecx & 0xFFFF0000) / (s32)n);
    U32(S_D0) = (u32)((s32)(ecx << 16) / (s32)n);
    U32(S_EX) = a & 0xFFFF0000;
    U32(S_DX) = (u32)((s32)((b & 0xFFFF0000) - (a & 0xFFFF0000)) / (s32)n);
    u32 skip, r = edge_rows(a, b, &n, &skip);
    if (skip) {
        U32(S_C0) += skip * U32(S_D0); U32(S_C1) += skip * U32(S_D1);
        U32(S_C2) += skip * U32(S_D2); U32(S_EX) += skip * U32(S_DX);
    }
    u32 c0 = U32(S_C0), c1 = U32(S_C1), c2 = U32(S_C2), x = U32(S_EX), dx = U32(S_DX);
    do {
        if ((s32)x < S32(T_L + r)) { U32(T_L + r) = x; U32(T_LC0 + r) = c0; U32(T_LC1 + r) = c1; U32(T_LC2 + r) = c2; }
        if ((s32)x > S32(T_R + r)) { U32(T_R + r) = x; U32(T_RC0 + r) = c0; U32(T_RC1 + r) = c1; U32(T_RC2 + r) = c2; }
        c0 += U32(S_D0); c1 += U32(S_D1); c2 += U32(S_D2); x += dx; r += 4;
    } while (--n);
}

/* ---- the rasterizers ------------------------------------------------------------------------ */

/* the corners' edges: 0-1, 1-2, then 2-3, 3-0 (four, flag 0x80) or 2-0 */
static int corners(gptr prm, u32 *ia, u32 *ib)
{
    static const u8 a4[4] = {0, 1, 2, 3}, b4[4] = {1, 2, 3, 0}, b3[3] = {1, 2, 0};
    int n = U8(prm + 3) & 0x80 ? 4 : 3;
    for (int k = 0; k < n; k++) { ia[k] = a4[k]; ib[k] = n == 4 ? b4[k] : b3[k]; }
    return n;
}

/* rows from the first drawn until one is empty: each clipped to x 0..640, drawn,
   its edges reset; per-row setup and the span by the caller's kind */
enum { R_FLAT, R_TEX, R_GOUR };
static void rows(int kind, const Span *s)
{
    u32 r = U32(S_MINY);
    if (r == 0x7FFF) return;
    u32 pitch = U32(PITCH);
    u8 *row = g_dst + r * pitch;
    for (r *= 4; r < ROWS; r += 4, row += pitch) {
        u32 lx = U32(T_L + r), rx = U32(T_R + r);
        if (lx == 0x7FFF0000) return;
        s32 l = (s32)lx >> 16, rr = (s32)rx >> 16, w = rr - l;
        if (w && rr > 0 && l < 0x280) {
            if (kind == R_TEX) {
                U32(S_DU) = (u32)((s32)(U32(T_RU + r) - U32(T_LU + r)) / w);
                U32(S_DV) = (u32)((s32)(U32(T_RV + r) - U32(T_LV + r)) / w);
            } else if (kind == R_GOUR) {
                U32(S_D0) = (u32)((s32)(U32(T_RC0 + r) - U32(T_LC0 + r)) / w);
                U32(S_D1) = (u32)((s32)(U32(T_RC1 + r) - U32(T_LC1 + r)) / w);
                U32(S_D2) = (u32)((s32)(U32(T_RC2 + r) - U32(T_LC2 + r)) / w);
            }
            if (l < 0) {
                u32 c = (u32)-l;
                w -= (s32)c;
                if (kind == R_TEX) { U32(T_LU + r) += c * U32(S_DU); U32(T_LV + r) += c * U32(S_DV); }
                else if (kind == R_GOUR) {
                    U32(T_LC0 + r) += c * U32(S_D0); U32(T_LC1 + r) += c * U32(S_D1); U32(T_LC2 + r) += c * U32(S_D2);
                }
                l = 0;
            }
            if (rr - 0x280 > 0) w -= rr - 0x280;
            if (s) {
                if (kind == R_TEX) span_tex(s, row, l, w, r);
                else if (kind == R_FLAT) span_flat(s, row, l, w);
                else span_gour(s, row, l, w, r);
            }
        }
        U32(T_L + r) = 0x7FFF0000;
        U32(T_R + r) = 0x80000000;
    }
}

static const Span *pick(u32 table, u32 slot, gptr prm, const Span *t, u32 n)
{
    U32(slot) = U32(table + 4 * ((U8(prm + 3) & 7) + U32(S_BASE)));
    return span_find(t, n, U32(slot));
}

#define CORNER(prm, k) U32((prm) + 0x10 + 4 * (k))

void Raster_Flat(void *dst, gptr prm)                                /* 0x4C3FFE */
{
    g_dst = dst; U32(S_DST) = (u32)(uintptr_t)dst;
    u32 c = to_pixel(U32(prm - 2), U32(prm - 1), U32(prm));
    if (!c) return;                             /* (a colour that comes out 0 is not drawn) */
    U32(S_FLAT) = c;
    if (U8(PIXBITS) == 0x10) { u32 eax = c, edx = U32(prm); cin(&eax, &edx); U32(S_FLAT8) = edx; }
    else U32(S_FLAT) &= 0xFCFCFC;
    const Span *s = pick(0x74242C, P_FLAT, prm, k_flat, sizeof k_flat / sizeof *k_flat);
    U32(S_MINY) = 0x7FFF;
    u32 ia[4], ib[4];
    for (int k = 0, n = corners(prm, ia, ib); k < n; k++) edge_flat(CORNER(prm, ia[k]), CORNER(prm, ib[k]));
    rows(R_FLAT, s);
}

void Raster_Tex(void *dst, const void *tex, gptr prm)                /* 0x4C3E1F */
{
    g_dst = dst; U32(S_DST) = (u32)(uintptr_t)dst;
    g_tex = tex; U32(S_TEX) = (u32)(uintptr_t)tex;
    U32(S_MOD) = pack(U32(prm - 2), U32(prm - 1), U32(prm));
    const Span *s = pick(0x7423B0, P_TEX, prm, k_tex, sizeof k_tex / sizeof *k_tex);
    U32(S_MINY) = 0x7FFF;
    u32 ia[4], ib[4];
    for (int k = 0, n = corners(prm, ia, ib); k < n; k++) {
        gptr ua = prm + 0x20 + 2 * ia[k], ub = prm + 0x20 + 2 * ib[k];
        edge_tex(CORNER(prm, ia[k]), CORNER(prm, ib[k]), (u32)U8(ua) << 16 | U8(ua + 1), (u32)U8(ub) << 16 | U8(ub + 1));
    }
    rows(R_TEX, s);
}

void Raster_Gouraud(void *dst, gptr prm)                             /* 0x4C4149 */
{
    g_dst = dst; U32(S_DST) = (u32)(uintptr_t)dst;
    const Span *s = pick(0x7424A8, P_GOUR, prm, k_gour, sizeof k_gour / sizeof *k_gour);
    U32(S_MINY) = 0x7FFF;
    u32 ia[4], ib[4];
    for (int k = 0, n = corners(prm, ia, ib); k < n; k++)
        edge_gour(CORNER(prm, ia[k]), CORNER(prm, ib[k]), U32(prm + 4 * ia[k]), U32(prm + 4 * ib[k]));
    rows(R_GOUR, s);
}

/* a 28 x 28 glyph (rows of 32 texels: 16-bit, else 4 bytes each) at the corner,
   unclipped; MMX: modulated by the vertex colour */
void Raster_Glyph(void *dst, const void *glyph, gptr prm)            /* 0x4C42F3 */
{
    U32(S_MOD) = pack(U32(prm - 2), U32(prm - 1), U32(prm));
    u32 pitch = U32(PITCH);
    u8 *d = (u8 *)dst + (u32)U16(prm + 0x10) * pitch;
    const u8 *s = glyph;
    u32 x = U16(prm + 0x12), f = U32(P_GLYPH);
    int mmx = f == 0x4C60CD || f == 0x4C6125 || f == 0x4C6177;
    u32 bytes = f == 0x4C607A || f == 0x4C6125 ? 3 : f == 0x4C60A8 || f == 0x4C6177 ? 4 : 2;
    d += x * bytes;
    for (int y = 0; y < 28; y++, d += pitch - 28 * bytes, s += bytes == 2 ? 8 : 16)
        for (int k = 0; k < 28; k++, d += bytes) {
            if (bytes == 2) {
                u32 t = get16(s); s += 2;
                if (!t) continue;
                if (mmx) { u32 eax = t, edx = 0; cin(&eax, &edx); edx = mod_only(edx); cout(&eax, &edx); t = eax; }
                put16(d, t);
            } else {
                u32 t = get32(s); s += 4;
                if (!t) continue;
                if (mmx) t = mod_only(t);
                put_pixel(d, bytes, t);
            }
        }
}

/* ---- the setup for the pixel format --------------------------------------------------------- */

static int has_mmx(void)                                             /* 0x4C2A60 */
{
    if (g_soft_mmx >= 0) return g_soft_mmx;
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    return __builtin_cpu_supports("mmx");
#else
    return 1;
#endif
}

void PcSoft_Init(void)                                               /* 0x4C3C81 */
{
    if (U8(PIXBITS) == 0x10) {
        u32 g = U32(0x87BB54);
        U32(S_CIN) = g == 0x3E0 ? 0x4C3D28 : g == 0x7E0 ? 0x4C3D41 : 0x4C3D5A;
        U32(S_COUT) = g == 0x3E0 ? 0x4C3D73 : g == 0x7E0 ? 0x4C3D86 : 0x4C3D99;
    }
    U32(P_PACK) = U32(0x742388 + 4 * U8(0x87BB41));
    int m = has_mmx() & 1;
    u32 f = (u32)(U8(PIXBITS) >> 3) - 2;
    U32(P_GLYPH) = U32(0x742394 + 4 * (f + (m ? 3 : 0)));
    U32(S_BASE) = f * 5 + (m ? 15 : 0);
}
