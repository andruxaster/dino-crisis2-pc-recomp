/* The PC port's DrawOTag (0x4B50F0) and everything it draws with: the packet
 * drawers, render-state helpers, the texture cache that turns VRAM pages into
 * Direct3D textures, the pixel converters. A copy of the port's code, call
 * for call (Direct3D through include/dc2/gpudev.h), so the renderer behind it
 * sees exactly what the original sends; the harness checks the calls.
 *
 * Only the port's normal path is here: the flags at 0x75DEF0 are 0 (bit 0
 * would select a second set of drawers, never set by the game).
 *
 * Floats: the game runs the x87 at single precision (Direct3D sets it), so
 * every operation rounds to a float; float constants are objects (no x87
 * excess precision on literals). */
#include <string.h>
#include "dc2/types.h"
#include "dc2/gpu.h"
#include "dc2/gpudev.h"
#include "dc2/pcio.h"
#include "dc2/pcdraw.h"

#define F32(a)      (*(float *)MEM(a))

/* the port's globals */
#define G_TPAGE     0x87BAF4u      /* draw mode: tpage (u16), dfe 0x87BAF6, dtd 0x87BAF7 */
#define G_TWIN      0x87BAECu      /* texture window: x, y, w, h (s16) */
#define G_SCALE_X   0x8644BCu      /* screen scale (float) */
#define G_SCALE_Y   0x8644B8u
#define G_VTX       0x8658C0u      /* the vertex buffer: D3DTLVERTEX x 4 (0x20 each) */
#define G_UVTAB     0x865940u      /* (i + 0.512) / 256, i = 0..255 (float) */
#define SH_FOG      0x73B680u      /* last FOGENABLE */
#define SH_COLOROP  0x73B684u      /* last stage-0 COLOROP: 1 modulate, 2 modulate2x */
#define SH_ABR      0x73B688u      /* last blend: abr (0 also for opaque) */
#define SH_SHADE    0x73B68Cu      /* last SHADEMODE */
#define SH_DEPTH    0x73B690u      /* last tpage & 0xC00 */
#define TEXC        0x75E488u      /* texture cache: 32 pages x 32 slots x 0x18 */
#define CLUTROW     0x75CEE8u      /* 512 x (stamp, converted palette) */
#define PIXFMT      0x87BB40u      /* the texture format: +2 bits, +3 bytes, +4.. shifts, masks */
#define FVF_TL      0x1C4u         /* XYZRHW | DIFFUSE | SPECULAR | TEX1 */

static const float k_rhw = 0.1f, k_fog_scale = 10.0f, k_255 = 255.0f, k_1_128 = 0.0078125f;

static u32 texc(u32 page, u32 slot) { return TEXC + (page * 32 + slot) * 0x18; }

/* ---- render-state helpers ------------------------------------------------------------ */

/* the vertex colour (a8 r8 g8 b8) of a packet; also the stage-0 colour op */
static u32 vertex_colour(u32 r, u32 g, u32 b, u32 code, u32 tpage)  /* 0x4B5F20 */
{
    u32 a = 0xFF;
    if ((code & 2) && U32(0x73B67C)) {
        u32 abr = tpage >> 5 & 3;
        a = abr == 0 ? 0x80 : abr == 3 ? 0x40 : 0xFF;
    }
    if (code & 4) {
        if ((U8(0x75DEF0) & 4) && (tpage & 0x1000)) {
            if (code & 1) r = g = b = 0x80;
            if (U32(SH_COLOROP) != 2) { Dev_SetTextureStageState(0, 1, 5); U32(SH_COLOROP) = 2; }
        } else {
            if (code & 1) r = g = b = 0xFF;
            else {
                r <<= 1; if (r > 0xFF) r = 0xFF;
                g <<= 1; if (g > 0xFF) g = 0xFF;
                b <<= 1; if (b > 0xFF) b = 0xFF;
            }
            if (U32(SH_COLOROP) != 1) { Dev_SetTextureStageState(0, 1, 4); U32(SH_COLOROP) = 1; }
        }
    } else if (U32(SH_COLOROP) != 1) { Dev_SetTextureStageState(0, 1, 4); U32(SH_COLOROP) = 1; }
    return ((a << 8 | r) << 8 | g) << 8 | b;
}

/* fog per vertex (into the specular's alpha, OR'd: the buffer keeps earlier bits) */
static void fog(u32 on)                                              /* 0x4B5E60 */
{
    if (!U32(0x865EEC)) return;
    if (on && U32(0x865EE8) == 2) {
        float range = F32(0x73B678) - F32(0x73B674);
        for (u32 v = G_VTX + 0x14; v < G_VTX + 0x94; v += 0x20) {
            float f = F32(0x73B678) - F32(v - 0xC) * k_fog_scale;
            f = f / range;
            f = f * k_255;
            if (f < 0.0f || f != f) f = 0.0f;           /* (NaN: C0 set too) */
            else if (!(f <= k_255)) f = k_255;
            U32(v) |= (u32)(s32)f << 24;
        }
    }
    if (on != U32(SH_FOG)) {
        U32(SH_FOG) = on;
        Dev_SetRenderState(0x1C, on ? 1 : 0);
    }
}

/* blending by the packet's semi-transparency and the tpage's abr */
static void blend(u32 code, u32 tpage)                               /* 0x4B60A0 */
{
    static const u8 src[4] = {5, 2, 1, 5}, dst[4] = {6, 2, 4, 2};
    u32 abr = 0;
    if (code & 2) abr = tpage >> 5 & 3;
    else if (!U32(SH_ABR)) return;
    if (abr == U32(SH_ABR)) return;
    Dev_SetRenderState(0x13, src[abr]);
    Dev_SetRenderState(0x14, dst[abr]);
    U32(SH_ABR) = abr;
}

static void shade(u32 mode)                                          /* 0x4B6190 */
{
    if (mode == U32(SH_SHADE)) return;
    Dev_SetRenderState(9, mode);
    U32(SH_SHADE) = mode;
}

/* tpage 0x400: no depth test or write; 0x800: always pass */
static void depth(u32 tpage)                                         /* 0x4B61C0 */
{
    u32 m = tpage & 0xC00;
    if (m == U32(SH_DEPTH)) return;
    U32(SH_DEPTH) = m;
    if (tpage & 0x400) { Dev_SetRenderState(0xE, 0); Dev_SetRenderState(0x17, 8); Dev_SetRenderState(7, 0); }
    else { Dev_SetRenderState(0xE, 1); Dev_SetRenderState(0x17, 4); Dev_SetRenderState(7, 1); }
    if (tpage & 0x800) Dev_SetRenderState(0x17, 8);
}

/* ---- vertices -------------------------------------------------------------------------- */

/* vertex i from the packet's float x, y, z at xyz: screen-scaled, rhw 0.1/z */
static void vxyz(u32 i, gptr xyz)
{
    gptr v = G_VTX + i * 0x20;
    F32(v) = F32(G_SCALE_X) * F32(xyz);
    F32(v + 4) = F32(G_SCALE_Y) * F32(xyz + 4);
    U32(v + 8) = U32(xyz + 8);
    F32(v + 0xC) = k_rhw / F32(xyz + 8);
}
static void vcol(u32 i, u32 c) { U32(G_VTX + i * 0x20 + 0x10) = c; }
static void vuv(u32 i, gptr uv)
{
    U32(G_VTX + i * 0x20 + 0x18) = U32(G_UVTAB + 4 * U8(uv));
    U32(G_VTX + i * 0x20 + 0x1C) = U32(G_UVTAB + 4 * U8(uv + 1));
}
static void draw(u32 type, u32 n) { Dev_DrawPrimitive(type, FVF_TL, G_VTX, n, 0); }

/* ---- the texture cache ------------------------------------------------------------------- */

/* a 16- or 32-bit texel from a PS1 colour: the format's shifts and masks;
   0 stays transparent, a colour that comes out 0 becomes 1 */
static u32 dev_colour(u32 p)
{
    u32 c = ((p & 0x3E0) << 14 >> U32(PIXFMT + 8) & U32(PIXFMT + 0x14))
          | ((p & 0x1F) << 19 >> U32(PIXFMT + 4) & U32(PIXFMT + 0x10))
          | ((p & 0x7C00) << 9 >> U32(PIXFMT + 0xC) & U32(PIXFMT + 0x18));
    if (!c) { if (!(p & 0xFFFF)) return 0; c = 1; }
    return c | U32(PIXFMT + 0x1C);
}

/* VRAM row `row` from x (n colours) into its converted palette; the row's stamp++ */
void PcDraw_ClutRowConvert(u32 x, u32 row, s32 n)                    /* 0x4B4BF0 */
{
    gptr *slot = (gptr *)MEM(CLUTROW + 4 + row * 8);
    if (!*slot) *slot = Crt_malloc(0x1000);
    gptr d = *slot + (x << (U8(PIXFMT + 3) >> 1));
    gptr s = VRAM_BASE + 2 * ((row << 10) + x);
    for (; n > 0; n--, s += 2) {
        u32 c = dev_colour(U16(s));
        if (U8(PIXFMT + 3) == 2) { U16(d) = (u16)c; d += 2; } else { U32(d) = c; d += 4; }
    }
    U32(CLUTROW + row * 8)++;
}

/* the converted palette of clut (converting its row on first use) */
static gptr clut_converted(u32 clut)                                 /* 0x4B6900 */
{
    u32 row = (clut & 0xFFFF) >> 6 & 0x1FF, x = (clut & 0x3F) << 4;
    if (!U32(CLUTROW + 4 + row * 8)) PcDraw_ClutRowConvert(0, row, 0x400);
    return U32(CLUTROW + 4 + row * 8) + (x << (U8(PIXFMT + 3) >> 1));
}

/* the converters (0x73C900 [mode]): a w x h window of VRAM repeated over the
   256 x 256 texture; dst a host pointer, rows `pitch` bytes */
static void conv_4bit(u8 *dst, gptr src, gptr pal, s32 w, s32 h, s32 pitch)   /* 0x4C2A89 */
{
    int b16 = U8(PIXFMT + 2) == 0x10;
    for (s32 ry = 0x100; ry; ) {
        gptr s = src; ry -= h;
        for (s32 y = h; y; y--, s += 0x800, dst += pitch) {
            u8 *d = dst;
            for (s32 rx = 0x100; rx; ) {
                gptr t = s; rx -= w;
                for (s32 x = w; x; x -= 8, t += 4) {
                    u32 v = U32(t);
                    for (int k = 0; k < 8; k++, v >>= 4)
                        if (b16) { *(u16 *)d = U16(pal + 2 * (v & 0xF)); d += 2; }
                        else { *(u32 *)d = U32(pal + 4 * (v & 0xF)); d += 4; }
                }
            }
        }
    }
}

static void conv_8bit(u8 *dst, gptr src, gptr pal, s32 w, s32 h, s32 pitch)   /* 0x4C2C2A */
{
    int b16 = U8(PIXFMT + 2) == 0x10;
    for (s32 ry = 0x100; ry; ) {
        gptr s = src; ry -= h;
        for (s32 y = h; y; y--, s += 0x800, dst += pitch) {
            u8 *d = dst;
            for (s32 rx = 0x100; rx; ) {
                gptr t = s; rx -= w;
                for (s32 x = w; x; x -= 4, t += 4) {
                    u32 v = U32(t);
                    for (int k = 0; k < 4; k++, v >>= 8)
                        if (b16) { *(u16 *)d = U16(pal + 2 * (v & 0xFF)); d += 2; }
                        else { *(u32 *)d = U32(pal + 4 * (v & 0xFF)); d += 4; }
                }
            }
        }
    }
}

/* 15-bit: the colours converted (no alpha, 0 stays 0) */
static void conv_15bit(u8 *dst, gptr src, gptr pal, s32 w, s32 h, s32 pitch)  /* 0x4C2D5F */
{
    int b16 = U8(PIXFMT + 2) == 0x10;
    (void)pal;
    for (s32 ry = 0x100; ry; ) {
        gptr s = src; ry -= h;
        for (s32 y = h; y; y--, s += 0x800, dst += pitch) {
            u8 *d = dst;
            for (s32 rx = 0x100; rx; ) {
                gptr t = s; rx -= w;
                for (s32 x = w; x; x--, t += 2) {
                    u32 p = U16(t);
                    u32 c = ((p & 0x1F) << 19 >> U32(PIXFMT + 4) & U32(PIXFMT + 0x10))
                          | ((p & 0x3E0) << 14 >> U32(PIXFMT + 8) & U32(PIXFMT + 0x14))
                          | ((p & 0x7C00) << 9 >> U32(PIXFMT + 0xC) & U32(PIXFMT + 0x18));
                    if (b16) { *(u16 *)d = (u16)c; d += 2; } else { *(u32 *)d = c; d += 4; }
                }
            }
        }
    }
}

typedef void (*Conv)(u8 *, gptr, gptr, s32, s32, s32);
static const Conv k_conv[3] = {conv_4bit, conv_8bit, conv_15bit};   /* (modes 2 and 3: 15-bit) */

/* a slot for (page, clut, mode) and the current texture window: its index,
   0x20 if not there (a palette whose row changed since: marked for reload) */
static u32 texc_find(u32 page, u32 clut, u32 mode)                   /* 0x4B6CD0 */
{
    for (u32 i = 0; i < 32; i++) {
        gptr e = texc(page, i);
        if (!U8(e)) return 0x20;
        if (mode & 2) {
            if (U8(e + 1) == 2 && !memcmp(MEM(e + 8), MEM(G_TWIN), 8)) return i;
        } else if (U16(e + 2) == clut && !memcmp(MEM(e + 8), MEM(G_TWIN), 8)) {
            if (U32(e + 4) != U32(CLUTROW + ((s32)clut >> 6) * 8)) U8(e) = 2;
            return i;
        }
    }
    return 32;
}

/* the VRAM address of a texture window (tw: x, y) of page in mode */
static gptr texc_src(u32 page, u32 mode, gptr tw)
{
    s32 y = ((page & 0x10) << 4) + S16(tw + 2), x = (page & 0xF) << 7;
    switch (mode) {
    case 0: x += S16(tw) >> 1; break;
    case 1: x += S16(tw); break;
    case 2: x += S16(tw) * 2; break;
    }
    return VRAM_BASE + (u32)(y << 11) + (u32)x;
}

/* the staging surface filled from VRAM (a w x h window repeated) */
static int texc_fill(gptr src, u32 mode, u32 clut, s32 w, s32 h)
{
    u8 desc[SD_SIZE];
    gptr pal = mode & 2 ? 0 : clut_converted(clut);
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    if (Surf_Lock(U32(A_STAGING), 0, desc, 1)) return 0;
    k_conv[mode & 2 ? 2 : mode](*(u8 **)(desc + SD_SURFACE), src, pal, w, h, *(s32 *)(desc + SD_PITCH));
    Surf_Unlock(U32(A_STAGING), 0);
    return 1;
}

/* a 256 x 256 texture surface (and its IDirect3DTexture2) */
static int tex_create_surface(u32 w, u32 h, gptr surf, gptr tex, u32 fmt)   /* 0x4B5A30 */
{
    u8 desc[SD_SIZE];
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE;
    *(u32 *)(desc + SD_FLAGS) = 0x101007;
    *(u32 *)(desc + SD_HEIGHT) = h; *(u32 *)(desc + SD_WIDTH) = w;
    memcpy(desc + SD_PIXFMT, MEM(0x87BB60 + fmt * 0x40), 0x20);
    *(u32 *)(desc + SD_CAPS) = 0x1000;
    *(u32 *)(desc + SD_CAPS + 4) = 0x10;
    *(u32 *)(desc + SD_CAPS + 0x10) = 0;
    if (DD_CreateSurface(desc, (u32 *)MEM(surf))) return 0;
    Obj_QueryInterface(U32(surf), MEM(0x7049D0), (u32 *)MEM(tex));
    return 1;
}

static const u8 k_full_rect[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0};   /* 0, 0, 256, 256 */

/* a new slot for (page, clut, mode): the texture made from VRAM; its texture
   (0: no free slot or no surface) */
static u32 texc_create(u32 page, u32 clut, u32 mode)                 /* 0x4B6510 */
{
    u32 i = 0;
    while (i < 32 && U8(texc(page, i))) i++;
    if (i == 32) return 0;
    gptr e = texc(page, i);
    if (!tex_create_surface(0x100, 0x100, e + 0x10, e + 0x14, 0)) return 0;
    if (U8(0x75DEF0) & 0x20) { u32 key[2] = {0, 0}; Surf_SetColorKey(U32(e + 0x10), 8, key); }
    if (!texc_fill(texc_src(page, mode, G_TWIN), mode, clut, S16(G_TWIN + 4), S16(G_TWIN + 6))) return 0;
    if (!(mode & 2)) { U16(e + 2) = (u16)clut; U32(e + 4) = U32(CLUTROW + ((s32)clut >> 6) * 8); }
    Surf_Blt(U32(e + 0x10), k_full_rect, U32(A_STAGING), k_full_rect, 0x1000000, 0);
    U8(e) = 1; U8(e + 1) = (u8)mode;
    U32(e + 8) = U32(G_TWIN); U32(e + 0xC) = U32(G_TWIN + 4);
    return U32(e + 0x14);
}

/* a slot marked dirty: filled again from VRAM (its own window and mode) */
static void texc_reload(u32 page, u32 i)                             /* 0x4B6A00 */
{
    gptr e = texc(page, i);
    u32 mode = U8(e + 1);
    U8(e) = 1;
    if (!texc_fill(texc_src(page, mode, e + 8), mode, U16(e + 2), S16(e + 0xC), S16(e + 0xE))) return;
    Surf_Blt(U32(e + 0x10), k_full_rect, U32(A_STAGING), k_full_rect, 0x1000000, 0);
    if (!(mode & 2)) U32(e + 4) = U32(CLUTROW + (U16(e + 2) >> 6) * 8);
}

/* the texture for (tpage, clut) set on stage 0 */
static void tex_bind(u32 tpage, u32 clut)                            /* 0x4B6470 */
{
    u32 page = tpage & 0x1F, mode = tpage >> 7 & 3, tex;
    u32 i = texc_find(page, clut, mode);
    if (i == 0x20) {
        Dev_EndScene();
        tex = texc_create(page, clut, mode);
    } else {
        gptr e = texc(page, i);
        tex = U32(e + 0x14);
        if (U8(e) != 2) { Dev_SetTexture(0, tex); return; }
        Dev_EndScene();
        texc_reload(page, i);
    }
    Dev_BeginScene();
    Dev_SetTexture(0, tex);
}

/* ---- polygons ------------------------------------------------------------------------------ */

static u32 pkt_colour(gptr p, gptr rgb, u32 tpage) { return vertex_colour(U8(rgb), U8(rgb + 1), U8(rgb + 2), U8(p + 7), tpage); }
static u32 dm_tpage(void) { return U32(G_TPAGE) & 0xFFFF; }

/* the end of each drawer: texture (or none), depth, fog, blending, shading, the call */
static void finish(u32 tpage_depth, u32 fogon, u32 code, u32 tpage, u32 shademode, u32 type, u32 n)
{
    depth(tpage_depth);
    fog(fogon);
    blend(code, tpage);
    shade(shademode);
    draw(type, n);
}

static void poly_f(gptr p, u32 n)                                    /* 0x4B5D00 F3, 0x4B6F50 F4 */
{
    u32 c = pkt_colour(p, p + 4, dm_tpage());
    for (u32 i = 0; i < n; i++) { vxyz(i, p + 8 + 0xC * i); vcol(i, c); }
    Dev_SetTexture(0, 0);
    finish(dm_tpage(), 0, U8(p + 7), dm_tpage(), 1, n == 3 ? 4 : 5, n);
}

/* a texel of 4-bit page tpage at uv with clut, as the device colour */
static u32 texel4(u32 clut, u32 tpage, u32 uv)                       /* 0x4B6EE0 */
{
    u32 y = ((tpage & 0x10) << 4) + (u32)((s32)uv >> 8);
    u32 b = U8(VRAM_BASE + ((((y << 4) + (tpage & 0xF)) << 7) + ((u32)((s32)uv >> 1) & 0x7F)));
    u32 i = b >> ((~uv & 1) << 2) & 0xF;
    gptr pal = clut_converted(clut);
    return U8(PIXFMT + 3) == 2 ? U16(pal + 2 * i) : U32(pal + 4 * i);
}

/* a textured triangle whose 3 uvs are one texel: drawn flat in that texel's
   colour (the packet changed for good). Reads the PS1 layout (clut +0xE,
   tpage +0x16) on the PC one, as the port does. */
static void flat_from_texel(gptr p, u32 uv)                          /* 0x4B6DB0 */
{
    u32 c = texel4(U16(p + 0xE), U16(p + 0x16), uv & 0xFFFF);
    u32 g = (c & U32(PIXFMT + 0x14)) << U32(PIXFMT + 8) >> 16;
    u32 b = (c & U32(PIXFMT + 0x18)) << U32(PIXFMT + 0xC) >> 16;
    u32 r = (c & U32(PIXFMT + 0x10)) << U32(PIXFMT + 4) >> 16;
    u32 ch[3] = {r, g, b};
    for (int k = 0; k < 3; k++) {
        float f = (float)(s32)U8(p + 4 + k) * k_1_128;
        f = (float)(long long)ch[k] * f;
        u32 v = (u32)(s32)f;
        U8(p + 4 + k) = (u8)(v > 0xFF ? 0xFF : v);
    }
    U8(p + 7) &= 0xFB;
}

static void poly_ft3(gptr p)                                         /* 0x4B6260 */
{
    int flat = 0;
    if (U16(p + 0x14) == U16(p + 0x24) && U16(p + 0x14) == U16(p + 0x34)) { flat_from_texel(p, U16(p + 0x14)); flat = 1; }
    u32 tp = U16(p + 0x26), c = pkt_colour(p, p + 4, tp);
    for (u32 i = 0; i < 3; i++) { vxyz(i, p + 8 + 0x10 * i); vcol(i, c); }
    U32(G_VTX + 0x68) = 0;
    if (!flat) {
        for (u32 i = 0; i < 3; i++) vuv(i, p + 0x14 + 0x10 * i);
        tex_bind(tp, U16(p + 0x16));
    } else Dev_SetTexture(0, 0);
    finish(tp, tp & 0x2000, U8(p + 7), tp, 1, 4, 3);
}

static void poly_ft4(gptr p)                                         /* 0x4B70E0 */
{
    u32 tp = U16(p + 0x26), c = pkt_colour(p, p + 4, tp);
    for (u32 i = 0; i < 4; i++) { vxyz(i, p + 8 + 0x10 * i); vcol(i, c); vuv(i, p + 0x14 + 0x10 * i); }
    tex_bind(tp, U16(p + 0x16));
    finish(tp, tp & 0x2000, U8(p + 7), tp, 1, 5, 4);
}

static void poly_g(gptr p, u32 n)                                    /* 0x4B7300 G3, 0x4B76F0 G4 */
{
    u32 c[4];
    for (u32 i = 0; i < n; i++) c[i] = pkt_colour(p, p + 4 + 0x10 * i, dm_tpage());
    for (u32 i = 0; i < n; i++) { vxyz(i, p + 8 + 0x10 * i); vcol(i, c[i]); }
    if (n == 3) U32(G_VTX + 0x68) = 0;
    Dev_SetTexture(0, 0);
    finish(0, 0, U8(p + 7), dm_tpage(), 2, n == 3 ? 4 : 5, n);
}

static void poly_gt(gptr p, u32 n)                                   /* 0x4B74D0 GT3, 0x4B7920 GT4 */
{
    u32 tp = U16(p + 0x2A), c[4];
    for (u32 i = 0; i < n; i++) c[i] = pkt_colour(p, p + 4 + 0x14 * i, tp);
    for (u32 i = 0; i < n; i++) { vxyz(i, p + 8 + 0x14 * i); vcol(i, c[i]); vuv(i, p + 0x14 + 0x14 * i); }
    tex_bind(tp, U16(p + 0x16));
    finish(0, tp & 0x2000, U8(p + 7), tp, 2, n == 3 ? 4 : 5, n);
}

/* ---- lines, tiles, sprites --------------------------------------------------------------- */

static void line_f(gptr p, u32 n)                                    /* 0x4B7BD0 F2, 0x4B7E30 F3, 0x4B8140 F4 */
{
    u32 c = pkt_colour(p, p + 4, dm_tpage());
    for (u32 i = 0; i < n; i++) { vxyz(i, p + 8 + 0xC * i); vcol(i, c); }
    Dev_SetTexture(0, 0);
    finish(0, 0, U8(p + 7), dm_tpage(), 1, 3, n);
}

static void line_g(gptr p, u32 n)                                    /* 0x4B7CE0 G2, 0x4B7F80 G3, 0x4B82D0 G4 */
{
    u32 c[4];
    for (u32 i = 0; i < n; i++) c[i] = pkt_colour(p, p + 4 + 0x10 * i, dm_tpage());
    for (u32 i = 0; i < n; i++) { vxyz(i, p + 8 + 0x10 * i); vcol(i, c[i]); }
    Dev_SetTexture(0, 0);
    finish(0, 0, U8(p + 7), dm_tpage(), 2, 3, n);
}

/* the 4 corners of a screen rect: x0, x1 (already scaled), y0, y1; one z */
static void quad(float x0, float x1, float y0, float y1, gptr z, u32 c)
{
    F32(G_VTX) = x0; F32(G_VTX + 4) = y0;
    F32(G_VTX + 0x20) = x1; F32(G_VTX + 0x24) = y0;
    F32(G_VTX + 0x40) = x0; F32(G_VTX + 0x44) = y1;
    F32(G_VTX + 0x60) = x1; F32(G_VTX + 0x64) = y1;
    float rhw = k_rhw / F32(z);
    for (u32 i = 0; i < 4; i++) { U32(G_VTX + i * 0x20 + 8) = U32(z); F32(G_VTX + i * 0x20 + 0xC) = rhw; vcol(i, c); }
}

static void tile(gptr p)                                             /* 0x4B8500 */
{
    u32 c = pkt_colour(p, p + 4, dm_tpage());
    quad(F32(G_SCALE_X) * F32(p + 8), ((float)S16(p + 0x14) + F32(p + 8)) * F32(G_SCALE_X),
         F32(G_SCALE_Y) * F32(p + 0xC), ((float)S16(p + 0x16) + F32(p + 0xC)) * F32(G_SCALE_Y), p + 0x10, c);
    Dev_SetTexture(0, 0);
    finish(0x400, 0, U8(p + 7), dm_tpage(), 1, 5, 4);
}

static void tile1(gptr p)                                            /* 0x4B8660 */
{
    u32 c = pkt_colour(p, p + 4, dm_tpage());
    vxyz(0, p + 8); vcol(0, c);
    Dev_SetTexture(0, 0);
    finish(0x400, 0, U8(p + 7), dm_tpage(), 1, 1, 1);
}

/* sprites: width and height from the packet (or 8, 16), uvs from the table
   (the far edge: u0 + w*ustep - 1, read past the table as the port does) */
static void sprite(gptr p, u32 code, s32 w, s32 h, s32 step, const float *size)
{
    u32 c = vertex_colour(U8(p + 4), U8(p + 5), U8(p + 6), code, dm_tpage());
    float x1 = size ? (F32(p + 8) + *size) * F32(G_SCALE_X) : ((float)w + F32(p + 8)) * F32(G_SCALE_X);
    float y1 = size ? (F32(p + 0xC) + *size) * F32(G_SCALE_Y) : ((float)h + F32(p + 0xC)) * F32(G_SCALE_Y);
    quad(F32(G_SCALE_X) * F32(p + 8), x1, F32(G_SCALE_Y) * F32(p + 0xC), y1, p + 0x10, c);
    u32 u0 = U32(G_UVTAB + 4 * U8(p + 0x14)), v0 = U32(G_UVTAB + 4 * U8(p + 0x15));
    u32 u1 = U32(G_UVTAB - 4 + 4 * (u32)(s32)(U8(p + 0x14) + w * step));
    u32 v1 = U32(G_UVTAB - 4 + 4 * (u32)(s32)(U8(p + 0x15) + h * step));
    U32(G_VTX + 0x18) = u0; U32(G_VTX + 0x1C) = v0;
    U32(G_VTX + 0x38) = u1; U32(G_VTX + 0x3C) = v0;
    U32(G_VTX + 0x58) = u0; U32(G_VTX + 0x5C) = v1;
    U32(G_VTX + 0x78) = u1; U32(G_VTX + 0x7C) = v1;
    tex_bind(dm_tpage(), U16(p + 0x16));
    finish(0x400, 0, U8(p + 7), dm_tpage(), 1, 5, 4);
}

static const float k_8 = 8.0f, k_16 = 16.0f;

/* ---- the font: 28 x 28 4-bit glyphs (PC_SetBuffer's data) cached as 32 x 32 textures ---------- */

#define GLYPHS      0x8644C0u      /* 256 x (glyph u16, clut u16, stamp, surface, texture, used u16) */

static void conv_glyph(u8 *dst, gptr src, gptr pal, s32 pitch)      /* 0x4C2E4E */
{
    int b16 = U8(PIXFMT + 2) == 0x10, bpp = b16 ? 2 : 4;
    for (int y = 0; y < 28; y++, dst += pitch)
        for (int x = 0; x < 28; x += 2) {
            u32 b = U8(src++), t[2] = {b >> 4, b & 0xF};
            for (int k = 0; k < 2; k++)
                if (b16) *(u16 *)(dst + (x + k) * bpp) = U16(pal + 2 * t[k]);
                else *(u32 *)(dst + (x + k) * bpp) = U32(pal + 4 * t[k]);
        }
}

static const u8 k_glyph_rect[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0x1C, 0, 0, 0, 0x1C, 0, 0, 0};

static void glyph_make(u32 i, u32 glyph, u32 clut)                   /* 0x4B91E0 */
{
    gptr pal = clut_converted(clut), src = U32(0x865EE4) + glyph * 0x188, e = GLYPHS + i * 0x14;
    if (!U32(e + 8)) {
        if (!tex_create_surface(0x20, 0x20, e + 8, e + 0xC, 0)) return;
        if (U8(0x75DEF0) & 0x20) { u32 key[2] = {0, 0}; Surf_SetColorKey(U32(e + 8), 8, key); }
    }
    u8 desc[SD_SIZE];
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    if (Surf_Lock(U32(A_STAGING), 0, desc, 1)) return;
    conv_glyph(*(u8 **)(desc + SD_SURFACE), src, pal, *(s32 *)(desc + SD_PITCH));
    Surf_Unlock(U32(A_STAGING), 0);
    Surf_Blt(U32(e + 8), k_glyph_rect, U32(A_STAGING), k_glyph_rect, 0x1000000, 0);
    U16(e) = (u16)glyph; U16(e + 2) = (u16)clut;
    U32(e + 4) = U32(CLUTROW + ((s32)clut >> 6) * 8);
}

/* the glyph's texture set (made in the first free entry if not there); the entry used this frame */
static u32 glyph_bind(u32 glyph, u32 clut)                           /* 0x4B9100 */
{
    s32 i, fr = -1;
    for (i = 0; i < 256; i++) {
        gptr e = GLYPHS + i * 0x14;
        if (U16(e) == glyph && U16(e + 2) == clut && U32(e + 4) == U32(CLUTROW + (clut >> 6) * 8)) break;
        if (fr == -1 && !U16(e + 0x10)) fr = i;
    }
    if (i == 0x100 && fr != -1) {
        i = fr;
        Dev_EndScene();
        glyph_make((u32)fr, glyph, clut);
        Dev_BeginScene();
    }
    Dev_SetTexture(0, U32(GLYPHS + i * 0x14 + 0xC));
    U16(GLYPHS + i * 0x14 + 0x10) = 1;
    return (u32)i;
}

static void text_quad(gptr p)                                        /* 0x4B8EC0 (code 0x6C) */
{
    static const double k_uv = 0.03125;
    u32 c = vertex_colour(U8(p + 4), U8(p + 5), U8(p + 6), U8(p + 7), 0);
    static const u8 xy[4] = {8, 0x14, 0x20, 0x2C}, uv[4] = {0x10, 0x1C, 0x28, 0x34};
    for (u32 i = 0; i < 4; i++) {
        gptr v = G_VTX + i * 0x20;
        F32(v) = F32(G_SCALE_X) * F32(p + xy[i]);
        F32(v + 4) = F32(G_SCALE_Y) * F32(p + xy[i] + 4);
        U32(v + 8) = 0x3C23D70A; U32(v + 0xC) = 0x3DCCCCCD; U32(v + 0x10) = c;
    }
    for (u32 i = 0; i < 4; i++) {
        F32(G_VTX + i * 0x20 + 0x18) = (float)((double)(s32)(U8(p + uv[i]) << 1) * k_uv);
        F32(G_VTX + i * 0x20 + 0x1C) = (float)((double)(s32)(U8(p + uv[i] + 1) << 1) * k_uv);
    }
    glyph_bind(U16(p + 0x1E), U16(p + 0x12));
    finish(0x400, 0, 0, 0, 1, 5, 4);
}

/* the copy of the last frame (0x865D5C), when there is one: 0x865D4A asks for a new copy */
static void screen_quad(gptr p)                                      /* 0x4B9EA0 (code 0x8C) */
{
    if (!U16(0x865D4A)) U16(0x865D4A) = 1;
    if ((U8(0x75DEF0) & 8) || !U16(0x865D48)) return;
    u32 c = vertex_colour(U8(p + 4), U8(p + 5), U8(p + 6), U8(p + 7), U8(p + 0x13));
    float sx = F32(G_SCALE_X), sy = F32(G_SCALE_Y), s = F32(0x865D54);
    float x0 = sx * F32(p + 8), y0 = sy * F32(p + 0xC);
    float x1 = ((float)U16(p + 0x14) + F32(p + 8)) * sx, y1 = ((float)U16(p + 0x16) + F32(p + 0xC)) * sy;
    float tw = (float)U16(0x865D50), th = (float)U16(0x865D52);
    float u0 = (float)U16(p + 0x10) * s / tw, v0 = (float)U8(p + 0x12) * s / th;
    float u1 = (float)(s32)(U16(p + 0x18) + U16(p + 0x10)) * s / tw, v1 = (float)(s32)(U16(p + 0x1A) + U8(p + 0x12)) * s / th;
    float xs[4] = {x0, x1, x0, x1}, ys[4] = {y0, y0, y1, y1}, us[4] = {u0, u1, u0, u1}, vs[4] = {v0, v0, v1, v1};
    for (u32 i = 0; i < 4; i++) {
        gptr v = G_VTX + i * 0x20;
        F32(v) = xs[i]; F32(v + 4) = ys[i]; U32(v + 8) = 0x3F7D70A4; U32(v + 0xC) = 0x3DCCCCCD;
        U32(v + 0x10) = c; U32(v + 0x14) = 0;     /* (the port leaves a stack word there; fog is off) */
        F32(v + 0x18) = us[i]; F32(v + 0x1C) = vs[i];
    }
    Dev_SetTexture(0, U32(0x865D60));
    finish(0x400, 0, U8(p + 7), U8(p + 0x13), 1, 5, 4);
}

/* a group of background mask tiles: their vertex buffer from +8, count +0xC, texture +0xE */
static void mask_marker(gptr p)                                      /* 0x4BA120 (code 0x88) */
{
    if (!U32(0x86449C)) { Dev_EndScene(); Pc_MasksUpload(0); Dev_BeginScene(); }
    Dev_EndScene();
    Dev_BeginScene();
    depth(0x400);
    shade(1);
    blend(0, 0);
    Dev_SetTexture(0, U32(0x8644AC + 4 * U16(p + 0xE)));
    Dev_DrawPrimitiveVB(4, U32(0x86449C), U32(p + 8), U16(p + 0xC), 0x14);
}

/* ---- VRAM moves -------------------------------------------------------------------------------- */

static int rect_meet(const s32 *a, const s32 *b)       /* IntersectRect: right and bottom exclusive */
{
    s32 l = a[0] > b[0] ? a[0] : b[0], t = a[1] > b[1] ? a[1] : b[1];
    s32 r = a[2] < b[2] ? a[2] : b[2], bo = a[3] < b[3] ? a[3] : b[3];
    return l < r && t < bo;
}

static void slot_release(gptr e)
{
    if (U32(e + 0x14)) Obj_Release(U32(e + 0x14));
    if (U32(e + 0x10)) Obj_Release(U32(e + 0x10));
}

/* textures over a VRAM rect: marked for reload (mark), else released; the page
   to the left too for its 8- and 15-bit textures, which reach into this one */
void PcDraw_InvalidateRect(gptr rect, u32 mark)                      /* 0x4B4790 */
{
    s32 x = S16(rect), y = S16(rect + 2);
    s32 r[4] = {x, y, S16(rect + 4) + x - 1, S16(rect + 6) + y - 1};
    for (u32 cy = (u32)y & ~0xFFu; cy < ((u32)(r[3] + 0x100) & ~0xFFu); cy += 0x100)
        for (u32 cx = (u32)x & ~0x3Fu; cx < ((u32)(r[2] + 0x40) & ~0x3Fu); cx += 0x40) {
            s32 pr[4] = {(s32)cx, (s32)cy, (s32)cx + 0x3F, (s32)cy + 0xFF};
            u32 page = (u32)(((s32)cx >> 6) + ((s32)cy >> 4));
            if (!rect_meet(pr, r)) continue;
            for (u32 i = 0; i < 32; i++) {
                gptr e = texc(page, i);
                if (!U8(e)) break;
                if (mark) U8(e) = 2;
                else { slot_release(e); memset(MEM(e), 0, 0x18); }
            }
            if (!(page & 0xF)) continue;
            for (u32 i = 0; i < 32; i++) {
                gptr e = texc(page - 1, i);
                if (!U8(e)) break;
                if (!U8(e + 1)) continue;
                if (mark) { U8(e) = 2; continue; }
                slot_release(e);
                memmove(MEM(e), MEM(e + 0x18), (31 - i) * 0x18);
                memset(MEM(texc(page - 1, 31)), 0, 0x18);
                i--;
            }
        }
}

/* the rect at r (clipped to VRAM, in the packet) copied to x, y; then the textures there */
static void vram_copy(gptr r, s32 x, s32 y)                          /* 0x4B4A60 */
{
    if (!S16(r + 4) || !S16(r + 6)) return;
    if (S16(r) < 0) { x += S16(r); S16(r + 4) = (s16)(S16(r + 4) + S16(r)); S16(r) = 0; }
    else if (S16(r) + S16(r + 4) > 0x400) S16(r + 4) = (s16)(0x400 - S16(r));
    if (S16(r + 2) < 0) { y += S16(r + 2); S16(r + 6) = (s16)(S16(r + 6) + S16(r + 2)); S16(r + 2) = 0; }
    else if (S16(r + 2) + S16(r + 6) > 0x200) S16(r + 6) = (s16)(0x200 - S16(r + 2));
    /* 0x4C3BE6: rows of w/2 dwords, forward */
    gptr s = VRAM_BASE + ((u32)U16(r + 2) << 11) + 2 * (u32)U16(r), d = VRAM_BASE + ((u32)y << 11) + 2 * (u32)x;
    u32 n = U16(r + 4) >> 1, skip = (0x400 - (u32)U16(r + 4)) * 2;
    for (u32 h = U16(r + 6); h; h = (h - 1) & 0xFFFF) {
        for (u32 k = 0; k < n; k++, s += 4, d += 4) U32(d) = U32(s);
        s += skip; d += skip;
    }
    S16(r) = (s16)x; S16(r + 2) = (s16)y;
    PcDraw_InvalidateRect(r, 1);
}

/* rows of rect moved to x, y where they differ, their palettes converted again */
static void vram_move(gptr r, s32 x, s32 y)                          /* 0x4B4B30 */
{
    gptr s = VRAM_BASE + 2 * (u32)((S16(r + 2) << 10) + S16(r)), d = VRAM_BASE + 2 * (u32)((y << 10) + x);
    u32 n = (u32)S16(r + 4) * 2;
    for (s32 row = y; row < S16(r + 6) + y; row++, s += 0x800, d += 0x800) {
        if (!memcmp(MEM(d), MEM(s), n)) continue;
        memcpy(MEM(d), MEM(s), n);
        PcDraw_ClutRowConvert((u32)S16(r), (u32)row, S16(r + 4));
    }
}

/* ---- the copy of the frame (code 0x8C draws it) ---------------------------------------------- */

/* a texture size the device takes for w x h (powers of 2, minimum, maximum, square) */
static void tex_size(u32 w, u32 h, gptr ow, gptr oh)                 /* 0x4B5AE0 */
{
    u32 caps = U8(0x865E44);
    if (caps & 2) {
        u32 a = U32(0x865E6C) ? U32(0x865E6C) : 1;
        while ((s32)a < (s32)w) a <<= 1;
        u32 b = U32(0x865E70) ? U32(0x865E70) : 1;
        while ((s32)b < (s32)h) b <<= 1;
        w = a; h = b;
    }
    if (w > U32(0x865E74)) w = U32(0x865E74);
    if (h > U32(0x865E78)) h = U32(0x865E78);
    if ((caps & 0x20) && w != h) { if ((s32)w > (s32)h) h = w; else w = h; }
    U32(ow) = w; U32(oh) = h;                   /* (dwords: the second write covers half the first) */
}

static const float k_1_320 = 0.003125f, k_65536 = 65536.0f;

/* the back buffer copied into the grab surface by hand, scaled (when Blt fails) */
static void grab_by_hand(void)                                       /* 0x4B5B80 */
{
    s32 stx = (s32)(F32(G_SCALE_X) * k_65536), sty = (s32)(F32(G_SCALE_Y) * k_65536);
    u8 desc[SD_SIZE];
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    if (Surf_Lock(U32(0x865D58), 0, desc, 1)) return;
    s32 dpitch = *(s32 *)(desc + SD_PITCH);
    u8 *d = *(u8 **)(desc + SD_SURFACE);
    if (Surf_Lock(U32(A_BACK), 0, desc, 1)) return;
    s32 spitch = *(s32 *)(desc + SD_PITCH);
    const u8 *s = *(u8 **)(desc + SD_SURFACE);
    u32 fy = 0;
    for (int y = 0; y < 0xF0; y++, fy += (u32)sty, d += dpitch) {
        const u8 *row = s + (s32)(fy >> 16 | (fy & 0x80000000 ? 0xFFFF0000 : 0)) * spitch;
        u32 fx = 0;
        for (int x = 0; x < 0x140; x++, fx += (u32)stx)
            if (U8(0x87BBC3) == 2) ((u16 *)d)[x] = *(const u16 *)(row + ((fx >> 15) & 0x1FFFE));
            else ((u32 *)d)[x] = *(const u32 *)(row + ((fx >> 14) & 0x3FFFC));
    }
    Surf_Unlock(U32(A_BACK), 0);
    Surf_Unlock(U32(0x865D58), 0);
}

/* the frame copied into the grab texture (0x865D5C), made on first use */
static void grab(void)                                               /* 0x4B5820 */
{
    if (U8(0x75DEF0) & 9) return;
    if (!U32(0x865D58)) {
        u8 desc[SD_SIZE];
        U32(0x865D54) = 0x3F800000; U16(0x865D4C) = 0x140; U16(0x865D4E) = 0xF0;
        memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
        *(u32 *)(desc + SD_FLAGS) = 0x1007;
        *(u32 *)(desc + SD_HEIGHT) = U16(0x865D4E); *(u32 *)(desc + SD_WIDTH) = U16(0x865D4C);
        memcpy(desc + SD_PIXFMT, MEM(0x87BBE0), 0x20);
        *(u32 *)(desc + SD_CAPS) = 0x1000; *(u32 *)(desc + SD_CAPS + 4) = 0;
        if (DD_CreateSurface(desc, (u32 *)MEM(0x865D58))) return;
        tex_size(U16(0x865D4C), U16(0x865D4E), 0x865D50, 0x865D52);
        if (!tex_create_surface(U16(0x865D50), U16(0x865D52), 0x865D5C, 0x865D60, 2)) return;
        if (U16(0x865D50) < 0x140) {
            U16(0x865D4C) = U16(0x865D50);
            F32(0x865D54) = (float)U16(0x865D50) * k_1_320;
            U16(0x865D4E) = (u16)(s32)((float)U16(0x865D4E) * F32(0x865D54));
        }
    }
    u32 r1[4] = {0, 0, 0x140, 0xF0}, r2[4] = {0, 0, U16(0x865D4C), U16(0x865D4E)};
    if (Surf_Blt(U32(0x865D58), r1, U32(A_BACK), MEM(0x73B660), 0x1000000, 0)) grab_by_hand();
    Surf_Blt(U32(0x865D5C), r2, U32(0x865D58), r1, 0x1000000, 0);
}

/* ---- the walk --------------------------------------------------------------------------------- */

void PcDraw_DrawOTag(gptr ot)                                        /* 0x4B50F0 */
{
    if (!U8(0x87BAF7)) return;
    if (U16(0x865D4A)) U16(0x865D4A) = 0;
    Dev_BeginScene();
    for (gptr p = ot; U32(p) != 0xFFFFFFFF; p = U32(p) & 0xFFFFFF) {
        if (!(U32(p) & 0xFF000000)) continue;
        switch (U8(p + 7) & 0xFC) {
        case 0x20: poly_f(p, 3); break;
        case 0x24: poly_ft3(p); break;
        case 0x28: poly_f(p, 4); break;
        case 0x2C: poly_ft4(p); break;
        case 0x30: poly_g(p, 3); break;
        case 0x34: poly_gt(p, 3); break;
        case 0x38: poly_g(p, 4); break;
        case 0x3C: poly_gt(p, 4); break;
        case 0x40: line_f(p, 2); break;
        case 0x48: line_f(p, 3); break;
        case 0x4C: line_f(p, 4); break;
        case 0x50: line_g(p, 2); break;
        case 0x58: line_g(p, 3); break;
        case 0x5C: line_g(p, 4); break;
        case 0x60: tile(p); break;
        case 0x64: sprite(p, U8(p + 7), S16(p + 0x18), S16(p + 0x1A), 1, 0); break;
        case 0x68: tile1(p); break;
        case 0x6C: text_quad(p); break;
        case 0x74: sprite(p, U8(p + 7), 8, 8, 1, &k_8); break;
        case 0x7C: sprite(p, U8(p + 7), 16, 16, 1, &k_16); break;
        case 0x80: sprite(p, U8(p + 7) | 4, S16(p + 0x18), S16(p + 0x1A), 2, 0); break;
        case 0x88: mask_marker(p); break;
        case 0x8C: screen_quad(p); break;
        case 0xE0: case 0xE8:                       /* draw mode: dtd, dfe, tpage, texture window */
            U8(0x87BAF7) = U8(p + 6) & 1;
            U8(0x87BAF6) = U8(p + 6) >> 1 & 1;
            U16(G_TPAGE) = U16(p + 4);
            if (U32(p + 8)) { U32(G_TWIN) = U32(U32(p + 8)); U32(G_TWIN + 4) = U32(U32(p + 8) + 4); }
            break;
        case 0xEC: vram_copy(p + 8, S32(p + 0x10), S32(p + 0x14)); break;
        case 0xF0:                                  /* texture window */
            U32(G_TWIN) = U32(U32(p + 8)); U32(G_TWIN + 4) = U32(U32(p + 8) + 4);
            break;
        case 0xFC: vram_move(p + 8, S32(p + 0x10), S32(p + 0x14)); break;
        }
    }
    Dev_EndScene();
    for (gptr a = 0x8644D0; a < 0x8658D0; a += 0x14) U16(a) = 0;
    U16(0x865DAC) = 0;
    if (U16(0x865D4A)) { grab(); U16(0x865D48) = 1; } else U16(0x865D48) = 0;
}
