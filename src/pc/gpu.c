/* See include/dc2/gpu.h. */
#include <string.h>
#include "dc2/gpu.h"
#include "dc2/pcdraw.h"


/* ---- primitives and order tables ------------------------------------------------- */

/* p linked at the head of ot (bit 31 of the link: the packet has a code) */
void DC2CALL PcGpu_AddPrim(gptr ot, gptr p)                         /* 0x4C05D0 */
{
    U32(p) = (U8(p + 7) ? 0x80000000u : 0) | U32(ot);
    U32(ot) = p;
}

/* an order table of n entries cleared, linked from the last to the first,
   the first ending at the terminator 0x73C04C */
void DC2CALL PcGpu_ClearOTagR(gptr ot, s32 n)                       /* 0x4C0870 */
{
    gptr e = ot + (u32)n * 4 - 4;
    for (s32 i = n - 1; i > 0; i--, e -= 4) U32(e) = e - 4;
    U32(e) = 0x73C04C;
}

/* a display environment: the area shown */
void DC2CALL PcGpu_SetDefDispEnv(gptr env, s32 x, s32 y, s32 w, s32 h)   /* 0x4C07F0 */
{
    U16(env) = (u16)x; U16(env + 2) = (u16)y; U16(env + 4) = (u16)w; U16(env + 6) = (u16)h;
}

/* a drawing environment: the clip area, texture window 256 x 256, dithered,
   drawing to the shown area, no background clear (the offset left as it is) */
void DC2CALL PcGpu_SetDefDrawEnv(gptr env, s32 x, s32 y, s32 w, s32 h)   /* 0x4C0820 */
{
    U16(env) = (u16)x; U16(env + 2) = (u16)y; U16(env + 4) = (u16)w; U16(env + 6) = (u16)h;
    U16(env + 0xC) = 0; U16(env + 0xE) = 0; U16(env + 0x10) = 0x100; U16(env + 0x12) = 0x100;
    U8(env + 0x16) = 1; U8(env + 0x17) = 1; U32(env + 0x18) = 0;
}

/* a VRAM copy as a packet (code 0xEC): the rect to x, y when the table is drawn */
void DC2CALL PcGpu_SetDrawVramOp(gptr p, gptr rect, u32 x, u32 y)  /* 0x4C0740 */
{
    U32(p + 4) = 0xEC000000;
    U32(p + 8) = U32(rect); U32(p + 0xC) = U32(rect + 4);
    U32(p + 0x10) = x; U32(p + 0x14) = y;
}

/* the chain p0 .. p1 linked at the head of ot */
void DC2CALL PcGpu_AddPrims(gptr ot, gptr p0, gptr p1)              /* 0x4C0600 */
{
    U32(p1) = (U8(p1 + 7) ? 0x80000000u : 0) | U32(ot);
    U32(ot) = p0;
}

void DC2CALL PcGpu_SetSprt(gptr p) { U8(p + 7) = 0x64; U32(p + 0x10) = 0x3C23D70A; }   /* 0x4C0690: z 0.01 */
void DC2CALL PcGpu_SetTile(gptr p) { U8(p + 7) = 0x60; U32(p + 0x10) = 0x3C23D70A; }   /* 0x4C06A0 */
void DC2CALL PcGpu_SetMaskMarker(gptr p) { U32(p + 4) = 0x88000000; }                 /* 0x4C06B0 */

void DC2CALL PcGpu_SetSemiTrans(gptr p, u32 on)                     /* 0x4C06C0 */
{
    if (on & 1) U8(p + 7) |= 2; else U8(p + 7) &= 0xFD;
}

/* the drawing area: an empty packet in this port (code 0xE4, not drawn) */
void DC2CALL PcGpu_SetDrawArea(gptr p, gptr r)                      /* 0x4C06E0 */
{
    (void)r;
    U32(p + 4) = 0xE4000000;
}

void DC2CALL PcGpu_SetDrawMode(gptr p, u32 dfe, u32 dtd, u32 tpage, u32 tw) /* 0x4C06F0 */
{
    U32(p + 4) = 0xE8000000;
    U16(p + 4) = (u16)tpage;
    if (dfe) U8(p + 6) |= 1;
    if (dtd) U8(p + 6) |= 2;
    U32(p + 8) = tw;
}

u32 DC2CALL PcGpu_GetTPage(u32 tp, u32 abr, u32 x, u32 y)          /* 0x4C08A0 */
{
    u32 r = ((tp & 3) << 2 | (abr & 3)) << 3;
    r = (r | (y & 0x200)) << 2;
    return r | ((u32)((s32)x >> 6) & 0xF) | ((u32)((s32)y >> 4) & 0x10);
}

/* ---- the software VRAM ------------------------------------------------------------ */

static gptr vram_at(s32 x, s32 y) { return VRAM_BASE + 2 * (u32)((y << 10) + x); }

/* rep movsd + movsb: forward, dwords then the rest (as the original copies) */
static void copy_fwd(gptr d, gptr s, u32 n)
{
    u32 i = 0;
    for (; i + 4 <= n; i += 4) U32(d + i) = U32(s + i);
    for (; i < n; i++) U8(d + i) = U8(s + i);
}

/* rect (x, y, w, h) from p; nothing when it does not fit in 1024 x 512 */
void DC2CALL PcGpu_LoadImage(gptr rect, gptr p)                     /* 0x4B4D20 */
{
    s32 x = S16(rect), y = S16(rect + 2), w = S16(rect + 4);
    if (x + w > 0x400 || y + S16(rect + 6) > 0x200) return;
    gptr v = vram_at(x, y);
    for (s32 r = 0; r < S16(rect + 6); r++, v += 0x800, p += 2 * w)
        copy_fwd(v, p, (u32)(2 * w));
    PcDraw_InvalidateRect(rect, 0);              /* (the textures over it released) */
}

/* CLUT rows: each row of rect written only when it differs (and then its
   palette reconverted) */
void DC2CALL PcGpu_LoadImageIfChanged(gptr rect, gptr p)            /* 0x4B4DB0 */
{
    s32 x = S16(rect), w = S16(rect + 4);
    u32 n = (u32)(2 * w);
    gptr v = vram_at(x, S16(rect + 2));
    for (s32 y = S16(rect + 2); y < S16(rect + 2) + S16(rect + 6); y++, v += 0x800, p += n) {
        if (!memcmp(MEM(v), MEM(p), n)) continue;
        copy_fwd(v, p, n);
        PcDraw_ClutRowConvert((u32)S16(rect), (u32)y, S16(rect + 4));
    }
}

/* rect (x, y, w, h) of VRAM into p */
void DC2CALL PcGpu_StoreImage(gptr rect, gptr p)                    /* 0x4B49F0 */
{
    s32 w = S16(rect + 4);
    gptr v = vram_at(S16(rect), S16(rect + 2));
    for (s32 r = 0; r < S16(rect + 6); r++, v += 0x800, p += 2 * w)
        copy_fwd(p, v, (u32)(2 * w));
}
