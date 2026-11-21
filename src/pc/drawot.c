/* The PC port's DrawOTag (0x4B50F0) and everything it draws with: the packet
 * drawers, render-state helpers, the texture cache that turns VRAM pages into
 * Direct3D textures, the pixel converters. A copy of the port's code, call
 * for call (Direct3D through include/dc2/gpudev.h), so the renderer behind it
 * sees exactly what the original sends; the harness checks the calls.
 *
 * Both of the port's paths are here: the Direct3D one, and the one without a
 * 3D device (0x75DEF0 bit 0, never set by the game) that draws into the back
 * buffer through the port's software rasterizers (src/pc/soft.c).
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
#include "dc2/orig.h"
#include "dc2/game.h"


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

static int surface_create(u32 w, u32 h, gptr surf, u32 fmt);

/* a surface filled with 0 */
static void surf_clear(u32 surf)                                     /* 0x4B93C0 */
{
    u8 fx[0x64];
    memset(fx, 0, sizeof fx); *(u32 *)fx = 0x64;
    Surf_Blt(surf, 0, 0, 0, 0x1000400, fx);
}

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

/* a surface (the staging one, or the texture itself without a 3D device) filled
   from VRAM (a w x h window repeated) */
static int texc_fill(u32 surf, gptr src, u32 mode, u32 clut, s32 w, s32 h)
{
    u8 desc[SD_SIZE];
    gptr pal = mode & 2 ? 0 : clut_converted(clut);
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    if (Surf_Lock(surf, 0, desc, 1)) return 0;
    k_conv[mode & 2 ? 2 : mode](*(u8 **)(desc + SD_SURFACE), src, pal, w, h, *(s32 *)(desc + SD_PITCH));
    Surf_Unlock(surf, 0);
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
   (0: no free slot or no surface). Without a 3D device: a plain surface filled
   directly, the slot's index returned. */
static u32 texc_create(u32 page, u32 clut, u32 mode)                 /* 0x4B6510 */
{
    u32 i = 0;
    while (i < 32 && U8(texc(page, i))) i++;
    if (i == 32) return 0;
    gptr e = texc(page, i);
    u32 r;
    if (U8(0x75DEF0) & 1) {
        if (!surface_create(0x100, 0x100, e + 0x10, 0)) return 0;
        if (!texc_fill(U32(e + 0x10), texc_src(page, mode, G_TWIN), mode, clut, S16(G_TWIN + 4), S16(G_TWIN + 6))) return 0;
        if (!(mode & 2)) { U16(e + 2) = (u16)clut; U32(e + 4) = U32(CLUTROW + ((s32)clut >> 6) * 8); }
        U32(e + 0x14) = 0;
        r = i;
    } else {
        if (!tex_create_surface(0x100, 0x100, e + 0x10, e + 0x14, 0)) return 0;
        if (U8(0x75DEF0) & 0x20) { u32 key[2] = {0, 0}; Surf_SetColorKey(U32(e + 0x10), 8, key); }
        if (!texc_fill(U32(A_STAGING), texc_src(page, mode, G_TWIN), mode, clut, S16(G_TWIN + 4), S16(G_TWIN + 6))) return 0;
        if (!(mode & 2)) { U16(e + 2) = (u16)clut; U32(e + 4) = U32(CLUTROW + ((s32)clut >> 6) * 8); }
        Surf_Blt(U32(e + 0x10), k_full_rect, U32(A_STAGING), k_full_rect, 0x1000000, 0);
        r = U32(e + 0x14);
    }
    U8(e) = 1; U8(e + 1) = (u8)mode;
    U32(e + 8) = U32(G_TWIN); U32(e + 0xC) = U32(G_TWIN + 4);
    return r;
}

/* a slot marked dirty: filled again from VRAM (its own window and mode) */
static void texc_reload(u32 page, u32 i)                             /* 0x4B6A00 */
{
    gptr e = texc(page, i);
    u32 mode = U8(e + 1);
    U8(e) = 1;
    if (U8(0x75DEF0) & 1) {                     /* (no 3D device: the surface itself) */
        if (!texc_fill(U32(e + 0x10), texc_src(page, mode, e + 8), mode, U16(e + 2), S16(e + 0xC), S16(e + 0xE))) return;
    } else {
        if (!texc_fill(U32(A_STAGING), texc_src(page, mode, e + 8), mode, U16(e + 2), S16(e + 0xC), S16(e + 0xE))) return;
        Surf_Blt(U32(e + 0x10), k_full_rect, U32(A_STAGING), k_full_rect, 0x1000000, 0);
    }
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
/* a float moved through the x87 (fld/fstp): a signalling NaN comes out quiet */
static u32 x87_move(u32 v)
{
    if ((v & 0x7F800000) == 0x7F800000 && (v & 0x7FFFFF) && !(v & 0x400000)) v |= 0x400000;
    return v;
}

static void sprite(gptr p, u32 code, s32 w, s32 h, s32 step, const float *size)
{
    u32 c = vertex_colour(U8(p + 4), U8(p + 5), U8(p + 6), code, dm_tpage());
    /* in the port's order: the far corner's texel is read from the table after the
       vertices written so far (an index off the table's start reads them) */
#define V(i, f) (G_VTX + (i) * 0x20 + (f))
    float x0 = F32(G_SCALE_X) * F32(p + 8), y0 = F32(G_SCALE_Y) * F32(p + 0xC);
    F32(V(0, 0)) = x0; F32(V(0, 4)) = y0;
    U32(V(0, 0x18)) = x87_move(U32(G_UVTAB + 4 * U8(p + 0x14))); U32(V(0, 0x1C)) = x87_move(U32(G_UVTAB + 4 * U8(p + 0x15)));
    F32(V(1, 0)) = size ? (F32(p + 8) + *size) * F32(G_SCALE_X) : ((float)w + F32(p + 8)) * F32(G_SCALE_X);
    F32(V(1, 4)) = y0;
    U32(V(1, 0x1C)) = U32(V(0, 0x1C));
    U32(V(1, 0x18)) = x87_move(U32(G_UVTAB - 4 + 4 * (u32)(s32)(U8(p + 0x14) + w * step)));
    F32(V(2, 0)) = x0;
    float y1 = size ? (F32(p + 0xC) + *size) * F32(G_SCALE_Y) : ((float)h + F32(p + 0xC)) * F32(G_SCALE_Y);
    F32(V(2, 4)) = y1;
    U32(V(2, 0x18)) = U32(V(0, 0x18));
    U32(V(3, 0)) = U32(V(1, 0));
    u32 v1 = x87_move(U32(G_UVTAB - 4 + 4 * (u32)(s32)(U8(p + 0x15) + h * step)));
    U32(V(3, 0x18)) = U32(V(1, 0x18));
    U32(V(2, 0x1C)) = v1; U32(V(3, 0x1C)) = v1;
    F32(V(3, 4)) = y1;
    float rhw = k_rhw / F32(p + 0x10);
    for (u32 i = 0; i < 4; i++) { U32(V(i, 8)) = U32(p + 0x10); F32(V(i, 0xC)) = rhw; vcol(i, c); }
#undef V
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
    u8 desc[SD_SIZE];
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    if (U8(0x75DEF0) & 1) {                     /* (no 3D device: a plain 32 x 32 surface, drawn into) */
        if (!U32(e + 8)) {
            if (!surface_create(0x20, 0x20, e + 8, 0)) return;
            surf_clear(U32(e + 8));
            U32(e + 0xC) = 0;
        }
        if (Surf_Lock(U32(e + 8), 0, desc, 1)) return;
        conv_glyph(*(u8 **)(desc + SD_SURFACE), src, pal, *(s32 *)(desc + SD_PITCH));
        Surf_Unlock(U32(e + 8), 0);
    } else {
        if (!U32(e + 8)) {
            if (!tex_create_surface(0x20, 0x20, e + 8, e + 0xC, 0)) return;
            if (U8(0x75DEF0) & 0x20) { u32 key[2] = {0, 0}; Surf_SetColorKey(U32(e + 8), 8, key); }
        }
        if (Surf_Lock(U32(A_STAGING), 0, desc, 1)) return;
        conv_glyph(*(u8 **)(desc + SD_SURFACE), src, pal, *(s32 *)(desc + SD_PITCH));
        Surf_Unlock(U32(A_STAGING), 0);
        Surf_Blt(U32(e + 8), k_glyph_rect, U32(A_STAGING), k_glyph_rect, 0x1000000, 0);
    }
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
        if (U8(0x75DEF0) & 1) glyph_make((u32)fr, glyph, clut);
        else {
            Dev_EndScene();
            glyph_make((u32)fr, glyph, clut);
            Dev_BeginScene();
        }
    }
    if (!(U8(0x75DEF0) & 1)) Dev_SetTexture(0, U32(GLYPHS + i * 0x14 + 0xC));
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
    if (!U32(0x86449C)) { Dev_EndScene(); PcDraw_MasksUpload(0); Dev_BeginScene(); }
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

/* port-only (the small-memory builds, at each room change): every cached
   texture dropped, as the PS1 kept only what is in VRAM; each is made again
   from VRAM the next time it is drawn, so only the room's own stay */
void PcDraw_DropTextures(void)
{
    for (u32 page = 0; page < 32; page++)
        for (u32 i = 0; i < 32; i++) {
            gptr e = texc(page, i);
            if (U8(e)) slot_release(e);
            memset(MEM(e), 0, 0x18);
        }
    Dev_SetTexture(0, 0);
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

/* the same, called directly by game code (the vehicle gun's sight) */
void PcDraw_VramCopy(gptr r, s32 x, s32 y) { vram_copy(r, x, y); }   /* 0x4B4A60 */

/* rows of rect moved to x, y where they differ, their palettes converted again */
static void vram_move(gptr r, s32 x, s32 y)                          /* 0x4B4B30 */
{
    gptr s = VRAM_BASE + 2 * (u32)((S16(r + 2) << 10) + S16(r)), d = VRAM_BASE + 2 * (u32)((y << 10) + x);
    u32 n = (u32)S16(r + 4) * 2;
    for (s32 row = y; row < S16(r + 6) + y; row++, s += 0x800, d += 0x800) {
        if (!memcmp(MEM(d), MEM(s), n)) continue;
        {   /* rep movsd then movsb: forwards, overlapping copies repeat the start */
            u32 k = 0;
            for (; k + 4 <= n; k += 4) U32(d + k) = U32(s + k);
            for (; k < n; k++) U8(d + k) = U8(s + k);
        }
        PcDraw_ClutRowConvert((u32)S16(r), (u32)row, S16(r + 4));
    }
}

/* the same, for game code (the extra crisis character pictures: Gpu_MoveImage) */
void PcDraw_MoveImage(gptr r, s32 x, s32 y) { vram_move(r, x, y); }   /* 0x4B4B30 */

/* ---- the copy of the frame (code 0x8C draws it) ---------------------------------------------- */

/* a texture size the device takes for w x h (powers of 2, minimum, maximum, square) */
static void tex_fit(u32 w, u32 h, u32 *ow, u32 *oh)                  /* 0x4B5AE0 */
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
    *ow = w; *oh = h;
}

/* the same into game memory (dwords: the second write covers half the first) */
static void tex_size(u32 w, u32 h, gptr ow, gptr oh)
{
    u32 a, b;
    tex_fit(w, h, &a, &b);
    U32(ow) = a; U32(oh) = b;
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

/* ---- code 0x84: a sprite put together from pieces of VRAM, drawn scaled ---------------------- */

/* (no game code makes the packet: x, y +8, scale x, y +0x10 (floats), pieces a +0x18, count
   +0x1A, clut +0x1C, tpage +0x1E (0x400: mirrored)) */
#define SPR     0x865D98u      /* its texture, one slot of 0x28: w, h, texture w, h (u16); bounds x0, x1, y0, y1 (s16);
                                  clut, n, used this frame, uses (u16); the pieces' sum, the palette's stamp; surface, texture */
#define PIECES  0x75CEB8u      /* pieces (8 bytes): x s16, y s8, w/8 | h/8 << 4, tpage u16, u, v */

static const float k_half = 0.5f;

/* a 4-bit piece, w x h texels from src, over dst (colour 0 left out); flip bit 0 mirrors
   it in x, bit 1 in y. (Only the 4-bit converters are reachable: the port indexes its
   table 0x73C90C by bits 9-10 plus (tpage & 0x180) * 4, past the table for other modes.) */
static void conv_piece(u8 *dst, gptr src, gptr pal, u32 w, u32 h, s32 pitch, u32 flip)
{                                       /* 0x4C306E, 0x4C3216 (x), 0x4C33CE (y), 0x4C3585 (both) */
    int b16 = U8(PIXFMT + 2) == 0x10, bpp = b16 ? 2 : 4;
    s32 sx = 4, sy = 0x800;
    if (flip & 1) { src += (w >> 1) - 4; sx = -4; }
    if (flip & 2) { src += (h - 1) << 11; sy = -0x800; }
    u32 y = h;
    do {
        gptr s = src; u8 *d = dst; u32 x = w;
        do {
            u32 v = U32(s);
            for (int k = 0; k < 8; k++, d += bpp) {
                u32 i = (flip & 1 ? v >> (28 - 4 * k) : v >> 4 * k) & 0xF;
                if (b16) { u32 c = U16(pal + 2 * i); if (c) *(u16 *)d = (u16)c; }
                else { u32 c = U32(pal + 4 * i); if (c) *(u32 *)d = c; }
            }
            s += (u32)sx; x -= 8;
        } while (x);
        src += (u32)sy; dst += pitch;
    } while (--y);
}

/* piece E converted into the locked staging surface (desc), around 160, 128 */
static void piece_draw(gptr E, u8 *desc, gptr pal)
{
    s32 dx = S16(E), dy = S8(E + 2);
    u32 tp = U16(E + 4), b = U8(E + 3);
    u32 u = tp & 0x180 ? U8(E + 6) : (u32)U8(E + 6) >> 1;
    gptr src = VRAM_BASE + ((((tp & 0x10) << 4) + U8(E + 7)) << 11) + u + ((tp & 0xF) << 7);
    s32 pitch = *(s32 *)(desc + SD_PITCH);
    u8 *dst = *(u8 **)(desc + SD_SURFACE) + (dx + 0xA0) * (s32)U8(PIXFMT + 3) + (dy + 0x80) * pitch;
    conv_piece(dst, src, pal, (b & 0xF) << 3, b >> 1 & 0x78, pitch, (tp >> 9 & 3) + (tp & 0x180) * 4);
}

/* slot i: its texture and surface released, emptied */
static void spr_free(u32 i)                                          /* 0x4B9C00 */
{
    gptr e = SPR + i * 0x28;
    if (U32(e + 0x24)) Obj_Release(U32(e + 0x24));
    if (U32(e + 0x20)) Obj_Release(U32(e + 0x20));
    memset(MEM(e), 0, 0x28);
}

/* the staging surface (bounds of slot e, from 160, 128) copied to the slot's surface: dr */
static void spr_copy(gptr e, const s32 *sr, const s32 *dr)
{
    Surf_Blt(U32(e + 0x20), dr, U32(A_STAGING), sr, 0x1000000, 0);
}

/* slot i made for pieces a .. a+n-1: drawn into the staging surface, their bounds
   copied to a new texture (smaller when the device's is: the height scaled) */
static void spr_make(u32 i, u32 a, u32 n, u32 clut)                  /* 0x4B97D0 */
{
    s32 x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    u32 sum = 0;
    u8 desc[SD_SIZE];
    surf_clear(U32(A_STAGING));
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    if (Surf_Lock(U32(A_STAGING), 0, desc, 1)) return;
    gptr pal = clut_converted(clut);
    for (u32 k = 0; k < n; k++) {
        gptr E = PIECES + (a + k) * 8;
        s32 dx = S16(E), dy = S8(E + 2), w = (s32)((U8(E + 3) & 0xF) << 3), h = (s32)(U8(E + 3) >> 1 & 0x78);
        piece_draw(E, desc, pal);
        if (x0 > dx) x0 = dx;                   /* (the bounds start at 0, 0, 0, 0) */
        if (x1 < dx + w) x1 = dx + w;
        if (y0 > dy) y0 = dy;
        if (y1 < h + dy) y1 = h + dy;
        sum += U32(E) + U32(E + 4);
    }
    Surf_Unlock(U32(A_STAGING), 0);
    spr_free(i);
    gptr e = SPR + i * 0x28;
    S16(e + 8) = (s16)x0; S16(e + 0xA) = (s16)x1; S16(e + 0xC) = (s16)y0; S16(e + 0xE) = (s16)y1;
    U16(e) = (u16)(x1 - x0); U16(e + 2) = (u16)(y1 - y0);
    U16(e + 0x12) = (u16)n; U16(e + 0x10) = (u16)clut;
    U32(e + 0x18) = sum; U32(e + 0x1C) = U32(CLUTROW + (clut >> 6) * 8);
    s32 sr[4] = {x0 + 0xA0, y0 + 0x80, x1 + 0xA0, y1 + 0x80}, dr[4] = {0, 0, U16(e), U16(e + 2)};
    if (U8(0x75DEF0) & 1) {                     /* (no 3D device: a plain surface) */
        if (!surface_create(U16(e), U16(e + 2), e + 0x20, 0)) return;
        spr_copy(e, sr, dr);
        U16(e + 4) = (u16)((U16(e) + 3) & ~3u); U16(e + 6) = (u16)((U16(e + 2) + 3) & ~3u);
        return;
    }
    Dev_EndScene();
    u32 tw, th;
    tex_fit(U16(e), U16(e + 2), &tw, &th);
    if (!tex_create_surface(tw, th, e + 0x20, e + 0x24, 0)) return;   /* (the scene left ended) */
    s32 w = U16(e);
    if ((s32)tw < w) {
        float f = (float)(s32)tw / (float)w;
        f = f * (float)(s32)U16(e + 2);
        U16(e + 2) = (u16)(s32)f;
        U16(e) = (u16)tw;
        dr[2] = (s32)(tw & 0xFFFF); dr[3] = U16(e + 2);
    }
    spr_copy(e, sr, dr);
    U16(e + 4) = (u16)tw; U16(e + 6) = (u16)th;
    Dev_BeginScene();
}

/* slot i's pieces drawn again (their palette changed) into its texture */
static void spr_redo(u32 i, u32 a, u32 n, u32 clut)                  /* 0x4B9C40 */
{
    u8 desc[SD_SIZE];
    surf_clear(U32(A_STAGING));
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    if (Surf_Lock(U32(A_STAGING), 0, desc, 1)) return;
    gptr pal = clut_converted(clut);
    for (u32 k = 0; k < n; k++) piece_draw(PIECES + (a + k) * 8, desc, pal);
    Surf_Unlock(U32(A_STAGING), 0);
    gptr e = SPR + i * 0x28;
    U32(e + 0x1C) = U32(CLUTROW + (clut >> 6) * 8);
    s32 sr[4] = {S16(e + 8) + 0xA0, S16(e + 0xC) + 0x80, S16(e + 0xA) + 0xA0, S16(e + 0xE) + 0x80};
    s32 dr[4] = {0, 0, U16(e), U16(e + 2)};
    if (U8(0x75DEF0) & 1) { spr_copy(e, sr, dr); return; }
    Dev_EndScene();
    spr_copy(e, sr, dr);
    Dev_BeginScene();
}

/* the slot for pieces a .. a+n-1 in clut: the one made for them (by their sum; drawn
   again if the palette changed and it is not used yet this frame), else made in the
   least used slot not used this frame; marked used, its texture set */
#define SPR_SLOTS 1
static u32 spr_find(u32 a, u32 n, u32 clut)                          /* 0x4B9690 */
{
    u32 best = 0xFFFF, bi = a, i;               /* (none free: the port makes "slot" a) */
    for (i = 0; i < SPR_SLOTS; i++) {
        gptr e = SPR + i * 0x28;
        if (!U32(e)) { spr_make(i, a, n, clut); break; }
        if (U16(e + 0x12) == n && U16(e + 0x10) == clut) {
            u32 sum = 0;
            for (u32 k = 0; k < n * 8; k += 4) sum += U32(PIECES + a * 8 + k);
            if (U32(e + 0x18) == sum) {
                if (U32(e + 0x1C) != U32(CLUTROW + (clut >> 6) * 8) && !U16(e + 0x14)) spr_redo(i, a, n, clut);
                break;
            }
        }
        if ((s32)best > (s32)U16(e + 0x16) && !U16(e + 0x14)) { best = U16(e + 0x16); bi = i; }
    }
    if (i == 1) { i = bi; spr_make(i, a, n, clut); }   /* (the port tests for 1, its slot count) */
    gptr e = SPR + i * 0x28;
    U16(e + 0x16)++;
    U16(e + 0x14) = 1;
    if (!(U8(0x75DEF0) & 1)) Dev_SetTexture(0, U32(e + 0x24));
    return i;
}

static void sprite_scaled(gptr p)                                    /* 0x4B9400 (code 0x84) */
{
    u32 c = vertex_colour(U8(p + 4), U8(p + 5), U8(p + 6), U8(p + 7), U16(p + 0x1E));
    gptr e = SPR + spr_find(U16(p + 0x18), U16(p + 0x1A), U16(p + 0x1C)) * 0x28;
    float x = F32(p + 8), y = F32(p + 0xC), sx = F32(p + 0x10), sy = F32(p + 0x14);
    float xa, xb;
    if (U8(p + 0x1F) & 4) {
        xa = (x - (float)S16(e + 8) * sx) * F32(G_SCALE_X);
        xb = (x - (float)S16(e + 0xA) * sx) * F32(G_SCALE_X);
    } else {
        xa = ((float)S16(e + 8) * sx + x) * F32(G_SCALE_X);
        xb = ((float)S16(e + 0xA) * sx + x) * F32(G_SCALE_X);
    }
    float ya = ((float)S16(e + 0xC) * sy + y) * F32(G_SCALE_Y), yb = ((float)S16(e + 0xE) * sy + y) * F32(G_SCALE_Y);
    float tw = (float)(s32)U16(e + 4), th = (float)(s32)U16(e + 6);
    float u0 = k_half / tw, v0 = k_half / th;
    float u1 = ((float)(s32)U16(e) - k_half) / tw, v1 = ((float)(s32)U16(e + 2) - k_half) / th;
    float xs[4] = {xa, xb, xa, xb}, ys[4] = {ya, ya, yb, yb}, us[4] = {u0, u1, u0, u1}, vs[4] = {v0, v0, v1, v1};
    for (u32 i = 0; i < 4; i++) {
        gptr v = G_VTX + i * 0x20;
        F32(v) = xs[i]; F32(v + 4) = ys[i]; U32(v + 8) = 0x3C23D70A; U32(v + 0xC) = 0x3DCCCCCD;
        U32(v + 0x10) = c;
        F32(v + 0x18) = us[i]; F32(v + 0x1C) = vs[i];
    }
    depth(0x400);
    blend(U8(p + 7), U16(p + 0x1E));
    shade(1);
    draw(5, 4);
}

/* ---- without a 3D device (0x75DEF0 bit 0): drawn into the back buffer by the port's
   software rasterizers (soft.c) ------------------------------------------------ */

#define PA      0x75CEC0u      /* the rasterizers' parameters: colour (r, g, b; abr + 1 when
                                  semi-transparent, | 0x80: four corners), the gouraud colours
                                  2-4 (+4); corners y, x (s16, +0x10); their uvs (u16, +0x20) */
#define BB_PITCH 0x87BB3Cu     /* the back buffer's pitch */

static u16 pa_ftol(float f) { return (u16)(long long)f; }

/* corner k from the packet's floats x, y (screen-scaled, truncated) */
static void pa_xy(u32 k, gptr xy)
{
    U16(PA + 0x12 + 4 * k) = pa_ftol(F32(G_SCALE_X) * F32(xy));
    U16(PA + 0x10 + 4 * k) = pa_ftol(F32(G_SCALE_Y) * F32(xy + 4));
}

/* the colour and its flags: blending by tpage's abr (byte), four corners */
static void pa_head(gptr p, u32 tp, u32 quad)
{
    U32(PA) = U32(p + 4);
    U8(PA + 3) = (u8)((U8(p + 7) & 2 ? (tp >> 5 & 3) + 1 : 0) | quad);
}

static u8 *pa_lock(u32 surf, u8 *desc)
{
    memset(desc, 0, SD_SIZE); *(u32 *)desc = SD_SIZE;
    Surf_Lock(surf, 0, desc, 1);                /* (not checked) */
    return *(u8 **)(desc + SD_SURFACE);
}

static void pa_flat(void (*raster)(void *, gptr))
{
    u8 d[SD_SIZE];
    raster(pa_lock(U32(A_BACK), d), PA);
    Surf_Unlock(U32(A_BACK), 0);
}

/* the texture surface for (tpage, clut), made or reloaded first, locked: its pixels */
static u8 *pa_tex_lock(u32 tpage, u32 clut, u32 *surf, u8 *desc)     /* 0x4BA430 */
{
    u32 page = tpage & 0x1F, mode = tpage >> 7 & 3;
    u32 i = texc_find(page, clut, mode);
    if (i == 0x20) i = texc_create(page, clut, mode);       /* (its index here; 0 if it failed) */
    else if (U8(texc(page, i)) == 2) texc_reload(page, i);
    *surf = U32(texc(page, i) + 0x10);
    return pa_lock(*surf, desc);
}

static void pa_textured(u32 tpage, u32 clut)                         /* 0x4BA3B0 */
{
    u8 d[SD_SIZE], td[SD_SIZE];
    u8 *dst = pa_lock(U32(A_BACK), d);
    u32 ts;
    u8 *tex = pa_tex_lock(tpage, clut, &ts, td);
    Raster_Tex(dst, tex, PA);
    Surf_Unlock(ts, 0);
    Surf_Unlock(U32(A_BACK), 0);
}

static void pa_uv(u32 k, gptr uv) { U16(PA + 0x20 + 2 * k) = U16(uv); }

/* polygons: quads' corners in the order 0, 1, 3, 2 (gouraud colours not reordered) */
static void pa_poly_f(gptr p, u32 n)                                 /* 0x4BA1D0 F3, 0x4BA4E0 F4 */
{
    static const u8 f3[3] = {8, 0x14, 0x20}, f4[4] = {8, 0x14, 0x2C, 0x20};
    pa_head(p, U8(G_TPAGE), n == 4 ? 0x80 : 0);
    for (u32 k = 0; k < n; k++) pa_xy(k, p + (n == 4 ? f4 : f3)[k]);
    pa_flat(Raster_Flat);
}

static void pa_poly_ft(gptr p, u32 n)                                /* 0x4BA2D0 FT3, 0x4BA600 FT4 */
{
    static const u8 o3[3] = {0, 0x10, 0x20}, o4[4] = {0, 0x10, 0x30, 0x20};
    pa_head(p, U8(p + 0x26), n == 4 ? 0x80 : 0);
    for (u32 k = 0; k < n; k++) pa_xy(k, p + 8 + (n == 4 ? o4 : o3)[k]);
    for (u32 k = 0; k < n; k++) pa_uv(k, p + 0x14 + (n == 4 ? o4 : o3)[k]);
    pa_textured(U16(p + 0x26), U16(p + 0x16));
}

static void pa_poly_g(gptr p, u32 n)                                 /* 0x4BA720 G3, 0x4BA910 G4 */
{
    static const u8 o3[3] = {0, 0x10, 0x20}, o4[4] = {0, 0x10, 0x30, 0x20};
    U32(PA) = U32(p + 4);
    for (u32 k = 1; k < n; k++) U32(PA + 4 * k) = U32(p + 4 + 0x10 * k);
    U8(PA + 3) = (u8)((U8(p + 7) & 2 ? (U8(G_TPAGE) >> 5 & 3) + 1 : 0) | 0x80);   /* (G3 too: its 4th corner stale) */
    for (u32 k = 0; k < n; k++) pa_xy(k, p + 8 + (n == 4 ? o4 : o3)[k]);
    pa_flat(Raster_Gouraud);
}

static void pa_poly_gt(gptr p, u32 n)                                /* 0x4BA830 GT3, 0x4BAA50 GT4 */
{
    static const u8 o3[3] = {0, 0x14, 0x28}, o4[4] = {0, 0x14, 0x3C, 0x28};
    pa_head(p, U8(p + 0x2A), n == 4 ? 0x80 : 0);
    for (u32 k = 0; k < n; k++) pa_xy(k, p + 8 + (n == 4 ? o4 : o3)[k]);
    for (u32 k = 0; k < n; k++) pa_uv(k, p + 0x14 + (n == 4 ? o4 : o3)[k]);
    pa_textured(U16(p + 0x2A), U16(p + 0x16));
}

/* a screen rect from x0, y0 (scaled) to x1, y1 (already scaled): corners
   (x0, y0), (x1, y0), (x1, y1), (x0, y1) */
static void pa_rect(gptr p, u16 x1, u16 y1)
{
    u16 x0 = pa_ftol(F32(G_SCALE_X) * F32(p + 8)), y0 = pa_ftol(F32(G_SCALE_Y) * F32(p + 0xC));
    U16(PA + 0x12) = x0; U16(PA + 0x10) = y0;
    U16(PA + 0x16) = x1; U16(PA + 0x14) = y0;
    U16(PA + 0x1A) = x1; U16(PA + 0x18) = y1;
    U16(PA + 0x1E) = x0; U16(PA + 0x1C) = y1;
}

static void pa_tile(gptr p)                                          /* 0x4BAB70 */
{
    pa_head(p, U8(G_TPAGE), 0x80);
    pa_rect(p, pa_ftol(((float)S16(p + 0x14) + F32(p + 8)) * F32(G_SCALE_X)),
            pa_ftol(((float)S16(p + 0x16) + F32(p + 0xC)) * F32(G_SCALE_Y)));
    pa_flat(Raster_Flat);
}

static void raster_dot(void *dst, gptr prm) { (void)dst; (void)prm; }   /* 0x4C4148: empty */

static void pa_tile1(gptr p)                                         /* 0x4BAC90 */
{
    pa_head(p, U8(G_TPAGE), 0);
    pa_xy(0, p + 8);
    pa_flat(raster_dot);
}

/* sprites: w, h from the packet (size 0) or 8, 16; uvs u0..u0+w-1 (bytes) */
static void pa_sprite(gptr p, const float *size)                     /* 0x4BAD40, 0x4BAE70 (8), 0x4BAF80 (16) */
{
    u32 tp = U32(G_TPAGE);
    pa_head(p, tp & 0xFF, 0x80);
    u16 x1, y1;
    u8 du, dv;
    if (size) {
        x1 = pa_ftol((F32(p + 8) + *size) * F32(G_SCALE_X));
        y1 = pa_ftol((F32(p + 0xC) + *size) * F32(G_SCALE_Y));
        du = dv = (u8)(*size == 8.0f ? 7 : 0xF);
    } else {
        x1 = pa_ftol(((float)S16(p + 0x18) + F32(p + 8)) * F32(G_SCALE_X));
        y1 = pa_ftol(((float)S16(p + 0x1A) + F32(p + 0xC)) * F32(G_SCALE_Y));
        du = (u8)(U8(p + 0x18) - 1); dv = (u8)(U8(p + 0x1A) - 1);
    }
    pa_rect(p, x1, y1);
    u8 u0 = U8(p + 0x14), v0 = U8(p + 0x15), u1 = (u8)(u0 + du), v1 = (u8)(v0 + dv);
    U8(PA + 0x20) = u0; U8(PA + 0x21) = v0; U8(PA + 0x22) = u1; U8(PA + 0x23) = v0;
    U8(PA + 0x24) = u1; U8(PA + 0x25) = v1; U8(PA + 0x26) = u0; U8(PA + 0x27) = v1;
    pa_textured(tp & 0xFFFF, U16(p + 0x16));
}

/* a pixel of the back buffer's format (bytes 2, 3 or 4) from a 32-bit read of the source */
static void pa_put(u8 *d, const u8 *s, u32 bytes, int glyph24)
{
    if (bytes == 2) { u32 c = *(const u16 *)s; if (c) *(u16 *)d = (u16)c; return; }
    u32 c = *(const u32 *)s;
    if (!c) return;
    if (bytes == 4) { *(u32 *)d = c; return; }
    *(u16 *)d = (u16)c;
    d[glyph24 ? 1 : 2] = (u8)(c >> 16);         /* (the text's copy writes the third byte at +1) */
}

/* text: the glyph's 28 x 28 texture scaled to the quad (or copied when 28 x 28) */
static void pa_text(gptr p)                                          /* 0x4BB090 */
{
    pa_head(p, U8(G_TPAGE), 0x80);
    pa_xy(0, p + 8);
    s32 w = (s32)(long long)((F32(p + 0x14) - F32(p + 8)) * F32(G_SCALE_X));
    s32 h = (s32)(long long)((F32(p + 0x24) - F32(p + 0xC)) * F32(G_SCALE_Y));
    if (!w || !h) return;
    u32 g = glyph_bind(U16(p + 0x1E), U16(p + 0x12));
    u8 d[SD_SIZE], gd[SD_SIZE];
    u8 *dst = pa_lock(U32(A_BACK), d);
    const u8 *src = pa_lock(U32(GLYPHS + g * 0x14 + 8), gd);
    if (w == 0x1C && h == 0x1C) Raster_Glyph(dst, src, PA);
    else {
        u32 bytes = U8(PIXFMT + 2) >> 3, pitch = U32(BB_PITCH);
        u8 *row = dst + S16(PA + 0x10) * (s32)pitch + (s32)bytes * S16(PA + 0x12);
        s32 us = (s32)((u32)U8(p + 0x34) << 17) / w, vs = (s32)((u32)U8(p + 0x35) << 17) / h;
        u32 v = 0;
        for (s32 y = h; y > 0; y--, v += (u32)vs, row += pitch) {
            u32 u = 0;
            const u8 *srow = src + ((s32)v >> 16 << (bytes == 2 ? 6 : 7));
            for (s32 x = 0; x < (s32)bytes * w; x += (s32)bytes, u += (u32)us)
                pa_put(row + x, srow + (bytes == 2 ? (u >> 15 & 0x1FFFE) : (u >> 14 & 0x3FFFC)), bytes, 1);
        }
    }
    Surf_Unlock(U32(GLYPHS + g * 0x14 + 8), 0);
    Surf_Unlock(U32(A_BACK), 0);
}

/* code 0x84: the pieces' surface scaled into the back buffer, clipped to 640 x 480 (0x73B668) */
static void pa_sprite_scaled(gptr p)                                 /* 0x4BB3B0 */
{
    gptr e = SPR + spr_find(U16(p + 0x18), U16(p + 0x1A), U16(p + 0x1C)) * 0x28;
    U32(PA) = U32(p + 4);
    s32 W = U16(e), H = U16(e + 2);
    s32 w = (s32)(long long)((float)W * F32(p + 0x10) * F32(G_SCALE_X));
    s32 h = (s32)(long long)((float)H * F32(p + 0x14) * F32(G_SCALE_Y));
    s32 x, u0, n = w;
    if (U8(p + 0x1F) & 4) {                     /* mirrored: right to left */
        x = (s32)(long long)((F32(p + 8) - (float)S16(e + 0xA) * F32(p + 0x10)) * F32(G_SCALE_X));
        u0 = (W << 16) - 1;
        n = -w;
    } else {
        x = (s32)(long long)(((float)S16(e + 8) * F32(p + 0x10) + F32(p + 8)) * F32(G_SCALE_X));
        u0 = 0;
    }
    s32 y = (s32)(long long)(((float)S16(e + 0xC) * F32(p + 0x14) + F32(p + 0xC)) * F32(G_SCALE_Y));
    s32 us = (W << 16) / n, v0 = 0, vs = (H << 16) / h;
    if (n < 0) n = -n;
    s32 mx = S32(0x73B668), my = S32(0x73B66C);
    if (x + n < 0 || x > mx || y + h < 0 || y > my) return;
    if (x < 0) { u0 = (s32)((u32)u0 - (u32)us * (u32)x); n += x; x = 0; }
    if (x + n >= mx) n = mx - x;
    if (y < 0) { v0 = (s32)(0u - (u32)vs * (u32)y); h += y; y = 0; }
    if (y + h > my) h = my - y;
    u8 d[SD_SIZE], sd[SD_SIZE];
    u8 *dst = pa_lock(U32(A_BACK), d);
    const u8 *src = pa_lock(U32(e + 0x20), sd);
    u32 bytes = U8(PIXFMT + 2) >> 3, pitch = U32(BB_PITCH);
    u8 *row = dst + (s32)pitch * y + (s32)bytes * x;
    s32 v = v0;
    for (s32 r = h; r > 0; r--, v = (s32)((u32)v + (u32)vs), row += pitch) {
        const u8 *srow = src + (s32)(U16(e + 4) * (bytes == 2 ? 2 : 4)) * (v >> 16);
        s32 u = u0;
        for (s32 k = 0; k < (s32)bytes * n; k += (s32)bytes, u = (s32)((u32)u + (u32)us))
            pa_put(row + k, srow + (u >> 16) * (bytes == 2 ? 2 : 4), bytes, 0);
    }
    Surf_Unlock(U32(e + 0x20), 0);
    Surf_Unlock(U32(A_BACK), 0);
}

/* ---- the walk --------------------------------------------------------------------------------- */

/* the packets both paths share: draw mode, texture window, VRAM moves */
static void draw_mode(gptr p)
{
    switch (U8(p + 7) & 0xFC) {
    case 0xE0: case 0xE8:                           /* draw mode: dtd, dfe, tpage, texture window */
        U8(0x87BAF7) = U8(p + 6) & 1;
        U8(0x87BAF6) = U8(p + 6) >> 1 & 1;
        U16(G_TPAGE) = U16(p + 4);
        if (U32(p + 8)) { U32(G_TWIN) = U32(U32(p + 8)); U32(G_TWIN + 4) = U32(U32(p + 8) + 4); }
        break;
    case 0xEC: vram_copy(p + 8, S32(p + 0x10), S32(p + 0x14)); break;
    case 0xF0:                                      /* texture window */
        U32(G_TWIN) = U32(U32(p + 8)); U32(G_TWIN + 4) = U32(U32(p + 8) + 4);
        break;
    case 0xFC: vram_move(p + 8, S32(p + 0x10), S32(p + 0x14)); break;
    }
}

void PcDraw_DrawOTag(gptr ot)                                        /* 0x4B50F0 */
{
    if (!U8(0x87BAF7)) return;
    if (U16(0x865D4A)) U16(0x865D4A) = 0;
    if (U8(0x75DEF0) & 1) {                     /* no 3D device: lines, codes 0x80, 0x88, 0x8C not drawn */
        for (gptr p = ot; U32(p) != 0xFFFFFFFF; p = U32(p) & 0xFFFFFF) {
            if (!(U32(p) & 0xFF000000)) continue;
            switch (U8(p + 7) & 0xFC) {
            case 0x20: pa_poly_f(p, 3); break;
            case 0x24: pa_poly_ft(p, 3); break;
            case 0x28: pa_poly_f(p, 4); break;
            case 0x2C: pa_poly_ft(p, 4); break;
            case 0x30: pa_poly_g(p, 3); break;
            case 0x34: pa_poly_gt(p, 3); break;
            case 0x38: pa_poly_g(p, 4); break;
            case 0x3C: pa_poly_gt(p, 4); break;
            case 0x60: pa_tile(p); break;
            case 0x64: pa_sprite(p, 0); break;
            case 0x68: pa_tile1(p); break;
            case 0x6C: pa_text(p); break;
            case 0x74: pa_sprite(p, &k_8); break;
            case 0x7C: pa_sprite(p, &k_16); break;
            case 0x84: pa_sprite_scaled(p); break;
            default: draw_mode(p); break;
            }
        }
        goto done;
    }
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
        case 0x84: sprite_scaled(p); break;
        case 0x88: mask_marker(p); break;
        case 0x8C: screen_quad(p); break;
        default: draw_mode(p); break;
        }
    }
    Dev_EndScene();
done:
    for (gptr a = 0x8644D0; a < 0x8658D0; a += 0x14) U16(a) = 0;
    U16(0x865DAC) = 0;
    if (U16(0x865D4A)) { grab(); U16(0x865D48) = 1; } else U16(0x865D48) = 0;
}

/* ---- the frame: the room's background, present, clear, fog, the draw and display environments ---- */

static const float k_256 = 256.0f, k_320 = 320.0f, k_64 = 64.0f, k_240 = 240.0f, k_0_1 = 0.1f, k_1_255 = 0.003921568859368563f;

/* a plain surface (w, h rounded up to 4) */
static int surface_create(u32 w, u32 h, gptr surf, u32 fmt)          /* 0x4B6960 */
{
    u8 desc[SD_SIZE];
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    *(u32 *)(desc + SD_FLAGS) = 0x1007;
    *(u32 *)(desc + SD_HEIGHT) = (h + 3) & ~3u; *(u32 *)(desc + SD_WIDTH) = (w + 3) & ~3u;
    memcpy(desc + SD_PIXFMT, MEM(0x87BB60 + fmt * 0x40), 0x20);
    *(u32 *)(desc + SD_CAPS) = U8(0x75DEF0) & 1 ? 0x840 : 0x1800;
    *(u32 *)(desc + SD_CAPS + 0x10) = 0;
    return DD_CreateSurface(desc, (u32 *)MEM(surf)) == 0;
}

/* the 320 x 240 background image (15-bit, blue high) into the plain surface and its
   textures, when it changed (a sum of some of its words) */
static void bg_upload(gptr img)                                      /* 0x4BE120 */
{
    u32 sum = 0;
    for (u32 r = 0; r < 15; r++)
        for (u32 k = 0; k < 20; k++) sum += U32(img + r * 0x2800 + k * 0x20);
    if (sum == U32(0x75CEA0)) return;
    U32(0x75CEA0) = sum;
    u8 desc[SD_SIZE];
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    if (Surf_Lock(U32(0x75CEA4), 0, desc, 1)) return;
    u8 *d = *(u8 **)(desc + SD_SURFACE);
    for (u32 n = 0; n < 320 * 240; n++, img += 2) {
        u32 p = U16(img);
        u32 c = ((p & 0x3E0) << 14 >> U32(0x87BBC8) & U32(0x87BBD4)) | ((p & 0x1F) << 19 >> U32(0x87BBC4) & U32(0x87BBD0))
              | ((p & 0x7C00) << 9 >> U32(0x87BBCC) & U32(0x87BBD8)) | U32(0x87BBDC);
        if (U8(0x87BBC3) == 2) *(u16 *)d = (u16)c; else *(u32 *)d = c;
        d += U8(0x87BBC3);
    }
    Surf_Unlock(U32(0x75CEA4), 0);
    if (U8(0x75DEF0) & 1) return;
    if (U32(0x75CEB4)) {
        u32 a[4] = {0, 0, 0x100, 0xF0}, b[4] = {0, 0, 0x40, 0xF0}, c[4] = {0x100, 0, 0x140, 0xF0};
        Surf_Blt(U32(0x75CEA8), a, U32(0x75CEA4), a, 0x1000000, 0);
        Surf_Blt(U32(0x75CEAC), b, U32(0x75CEA4), c, 0x1000000, 0);
    } else {
        u32 a[4] = {0, 0, 0x140, 0xF0};
        Surf_Blt(U32(0x75CEA8), a, U32(0x75CEA4), a, 0x1000000, 0);
    }
}

#define BGV 0x73B798u              /* two strips of 4 vertices: the 256- and the 64-wide part */

/* the room's background drawn behind everything: the camera's JPEG (img),
   `scroll` lines from its top, on the textures made on first use */
void PcDraw_Background(gptr img, u32 scroll)                         /* 0x4BDC90 */
{
    if (!U32(0x75CE9C)) {
        surface_create(0x140, 0xF0, 0x75CEA4, 2);
        if (!(U8(0x75DEF0) & 1)) {
            tex_size(0x140, 0xF0, 0x75CE90, 0x75CE98);
            if (!tex_create_surface(U32(0x75CE90), U32(0x75CE98), 0x75CEA8, 0x75CEB0, 2)) return;
            if (U32(0x75CE90) < 0x140) {
                tex_size(0x140 - U32(0x75CE90), 0xF0, 0x75CE94, 0x75CE98);
                tex_create_surface(U32(0x75CE94), U32(0x75CE98), 0x75CEAC, 0x75CEB4, 2);
                float x = F32(G_SCALE_X) * k_256, x2 = F32(G_SCALE_X) * k_320;
                F32(BGV + 0x20) = F32(BGV + 0x60) = F32(BGV + 0x80) = F32(BGV + 0xC0) = x;
                F32(BGV + 0xA0) = F32(BGV + 0xE0) = x2;
                U32(BGV) = U32(BGV + 0x40) = 0;
                U32(BGV + 0x18) = U32(BGV + 0x58) = U32(BGV + 0x98) = U32(BGV + 0xD8) = 0;
                U32(BGV + 0x1C) = U32(BGV + 0x3C) = U32(BGV + 0x9C) = U32(BGV + 0xBC) = 0;
                F32(BGV + 0x38) = F32(BGV + 0x78) = 1.0f;
                F32(BGV + 0xB8) = F32(BGV + 0xF8) = k_64 / (float)U32(0x75CE94);
                float v = k_240 / (float)U32(0x75CE98);
                F32(BGV + 0xFC) = F32(BGV + 0xDC) = v;
                F32(BGV + 0x7C) = F32(BGV + 0x5C) = v;
            } else {
                float x = F32(G_SCALE_X) * k_320;
                U32(BGV + 0x40) = U32(BGV) = 0;
                F32(BGV + 0x60) = F32(BGV + 0x20) = x;
                U32(BGV + 0x3C) = U32(BGV + 0x1C) = 0;
                U32(BGV + 0x58) = U32(BGV + 0x18) = 0;
                F32(BGV + 0x78) = F32(BGV + 0x38) = k_320 / (float)U32(0x75CE90);
                F32(BGV + 0x7C) = F32(BGV + 0x5C) = k_240 / (float)U32(0x75CE98);
            }
        }
        U32(0x75CE9C) = 1;
    }
    bg_upload(img);
    if (U8(0x75DEF0) & 1) {                     /* (no 3D device: the plain surface copied to the back buffer) */
        u32 sr[4] = {0, scroll, 0x140, 0xF0};
        u32 dr[4] = {0, 0, (u32)(s32)(F32(G_SCALE_X) * k_320), (u32)(s32)((float)(s32)(0xF0 - scroll) * F32(G_SCALE_Y))};
        Surf_Blt(U32(A_BACK), dr, U32(0x75CEA4), sr, 0x1000000, 0);
        return;
    }
    float s = (float)(s32)scroll, rest = (float)(s32)(0xF0 - scroll);
    F32(BGV + 0x3C) = F32(BGV + 0x1C) = s / (float)U32(0x75CE98);
    F32(BGV + 0x64) = F32(BGV + 0x44) = rest * F32(G_SCALE_Y);
    Dev_SetRenderState(0x1B, 0);
    Dev_SetRenderState(0xF, 0);
    depth(0x400);
    fog(0);
    shade(1);
    Dev_BeginScene();
    if (U32(0x75CEB4)) {
        F32(BGV + 0xBC) = F32(BGV + 0x9C) = s / (float)U32(0x75CE98);
        F32(BGV + 0xE4) = F32(BGV + 0xC4) = rest * F32(G_SCALE_Y);
        Dev_SetTexture(0, U32(0x75CEB0)); Dev_DrawPrimitive(5, FVF_TL, BGV, 4, 1);
        Dev_SetTexture(0, U32(0x75CEB4)); Dev_DrawPrimitive(5, FVF_TL, BGV + 0x80, 4, 1);
    } else {
        Dev_SetTexture(0, U32(0x75CEB0)); Dev_DrawPrimitive(5, FVF_TL, BGV, 4, 1);
    }
    Dev_EndScene();
    Dev_SetRenderState(0x1B, 1);
    Dev_SetRenderState(0xF, 1);
}

/* fog: colour, start and end (float bits), by the fog mode 0x865EE8 */
void PcDraw_SetFarColor(u32 rgb, u32 start, u32 end)                 /* 0x4B4E60 */
{
    if (U8(0x75DEF0) & 1) return;
    U32(0x73B674) = start; U32(0x865EF0) = rgb; U32(0x73B678) = end;
    Dev_SetRenderState(0x22, rgb);
    switch (U32(0x865EE8)) {
    case 0: Dev_SetRenderState(0x23, 3); Dev_SetRenderState(0x24, U32(0x73B674)); Dev_SetRenderState(0x25, U32(0x73B678)); break;
    case 1: {
        Dev_SetRenderState(0x23, 3);
        float a = F32(0x73B674) * k_0_1, b = F32(0x73B678) * k_0_1;
        u32 ua, ub; memcpy(&ua, &a, 4); memcpy(&ub, &b, 4);
        Dev_SetRenderState(0x24, ua); Dev_SetRenderState(0x25, ub);
        break; }
    case 2: Dev_SetRenderState(0x23, 0); break;
    }
}

void PcDraw_SetFont(gptr font)                                       /* 0x4BD480 */
{
    if (U32(0x865EE4)) Crt_free(U32(0x865EE4));
    U32(0x865EE4) = font;
}

void PcDraw_SetFogEnable(u32 on)                                     /* 0x4B4F50 */
{
    if (U8(0x75DEF0) & 1) return;
    Dev_SetRenderState(0x1C, on ? 1 : 0);
    U32(0x865EEC) = on ? 1 : 0;
}

/* bit 0: the back buffer cleared (colour and depth); bit 1: the front buffer
   (copied from the back buffer when windowed) */
void PcDraw_Clear(u32 mode)                                          /* 0x4B4F90 */
{
    u8 fx[0x64];
    if (mode & 1) {
        if (U8(0x75DEF0) & 1) {
            memset(fx, 0, sizeof fx); *(u32 *)fx = 0x64;
            Surf_Blt(U32(A_BACK), 0, 0, 0, 0x1000400, fx);
        } else Vp_Clear(U32(A_VIEWPORT), 1, 0x73B660, 3);
    }
    if (mode & 2) {
        if (U32(0x75DEF0) & 0x200) { Surf_Blt(U32(A_PRIMARY), MEM(0x75BE58), U32(A_BACK), MEM(0x73B660), 0x1000000, 0); return; }
        memset(fx, 0, sizeof fx); *(u32 *)fx = 0x64;
        Surf_Blt(U32(A_PRIMARY), 0, 0, 0, 0x1000400, fx);
    }
}

/* the frame shown (flip, or a copy when windowed), then the back buffer cleared */
void PcDraw_Present(void)                                            /* 0x4B5070 */
{
    u32 hr;
    if (U32(0x75DEF0) & 0x200) hr = Surf_Blt(U32(A_PRIMARY), MEM(0x75BE58), U32(A_BACK), MEM(0x73B660), 0x1000000, 0);
    else hr = Surf_Flip(U32(A_PRIMARY), U32(A_BACK), 1);
    if (hr == 0x887601C2) Surf_Restore(U32(A_PRIMARY));
    if (U8(0x87BAF8)) { PcDraw_Clear(1); return; }
    if (!(U8(0x75DEF0) & 1)) Vp_Clear(U32(A_VIEWPORT), 1, 0x73B660, 2);
}

/* the viewport's background colour */
void PcDraw_SetBackground(u32 r, u32 g, u32 b)                       /* 0x4BB7C0 */
{
    if (!U32(A_MATERIAL) || !U32(A_VIEWPORT)) return;
    u8 m[0x50];
    memset(m, 0, sizeof m);
    *(u32 *)m = 0x50;
    for (int k = 0; k < 2; k++) {               /* diffuse and ambient: r, g, b, 1 */
        *(float *)(m + 4 + 0x10 * k) = (float)(s32)(r & 0xFF) * k_1_255;
        *(float *)(m + 8 + 0x10 * k) = (float)(s32)(g & 0xFF) * k_1_255;
        *(float *)(m + 0xC + 0x10 * k) = (float)(s32)(b & 0xFF) * k_1_255;
        *(u32 *)(m + 0x10 + 0x10 * k) = 0x3F800000;
    }
    *(u32 *)(m + 0x4C) = 0x20;
    Mat_SetMaterial(U32(A_MATERIAL), m);
    Vp_SetBackground(U32(A_VIEWPORT), U32(0x75DEE8));
}

/* PutDispEnv: a new display area shows the frame first */
void PcDraw_PutDispEnv(gptr env)                                     /* 0x4C0770 */
{
    if (U16(0x87BAC2) != U16(env + 2)) PcDraw_Present();
    memcpy(MEM(0x87BAC0), MEM(env), 0x14);
}

/* PutDrawEnv: the background colour when it changes; the environment kept (draw mode, window) */
void PcDraw_PutDrawEnv(gptr env)                                     /* 0x4C07A0 */
{
    if (U8(0x87BAF9) != U8(env + 0x19) || U8(0x87BAFA) != U8(env + 0x1A) || U8(0x87BAFB) != U8(env + 0x1B))
        PcDraw_SetBackground(U8(env + 0x19), U8(env + 0x1A), U8(env + 0x1B));
    memcpy(MEM(0x87BAE0), MEM(env), 0x5C);
}

/* ---- the background masks (0x4BE330..0x4BEC80) ------------------------------------------------
 * Tiles of the room's background drawn over the models: 16x16 screen
 * squares, two triangles each (6 TL vertices of 0x20 in the array
 * 0x864490, count 0x864494, copied into the vertex buffer 0x86449C), their
 * pixels 18x18 (a border) from 0x864498, packed 14x14 to a 256x256 texture
 * page (0x8644A0 surfaces, 0x8644AC textures; one 512x512 texture of four
 * pages when the device allows it, 0x865E74). The vertices' specular words
 * keep the tile: v0 x, v1 y, v2 its page, v3 its depth, v4 (on a group's
 * first tile) the vertices of the group (tiles of one page and depth). */
#define MK_VERTS 0x864490u
#define MK_N     0x864494u
#define MK_PIX   0x864498u
#define MK_VB    0x86449Cu

/* the vertex buffer and textures released (all: the arrays too) */
static void masks_release(u32 all)                                   /* 0x4BE330 */
{
    if (all) {
        if (U32(MK_VERTS)) { Crt_free(U32(MK_VERTS)); U32(MK_VERTS) = 0; }
        if (U32(MK_PIX)) { Crt_free(U32(MK_PIX)); U32(MK_PIX) = 0; }
    }
    if (U32(MK_VB)) { Obj_Release(U32(MK_VB)); U32(MK_VB) = 0; }
    for (gptr s = 0x8644A0; s < 0x8644AC; s += 4) {
        if (U32(s + 0xC)) { Obj_Release(U32(s + 0xC)); U32(s + 0xC) = 0; }
        if (U32(s)) { Obj_Release(U32(s)); U32(s) = 0; }
    }
}

static void masks_vb(u32 n)
{
    u32 desc[4] = {0x10, 0x10000, 0x1C4, n};
    D3D_CreateVertexBuffer(desc, (u32 *)MEM(MK_VB));
}

void PcDraw_MasksCreate(u32 n, gptr pixels)                          /* 0x4BE3C0 */
{
    masks_release(1);
    masks_vb(n);
    U32(MK_VERTS) = Crt_malloc(n * 0x20);
    U32(MK_N) = n;
    U32(MK_PIX) = pixels;
}

/* (int * float, rounded to float as the x87 stores it) */
static float mk_mul(s32 a, gptr k) { return (float)((xreal)a * F32(k)); }

/* tile i at screen x, y (320x240) and depth z: its 6 vertices */
void PcDraw_MasksSetQuad(u32 i, u32 x, u32 y, u32 z)                /* 0x4BE430 */
{
    gptr v = U32(MK_VERTS) + i * 0xC0;
    float X = mk_mul((s32)x, 0x8644BC), Y = mk_mul((s32)y, 0x8644B8);
    float X2 = mk_mul((s32)x + 16, 0x8644BC), Y2 = mk_mul((s32)y + 16, 0x8644B8);
    F32(v + 0x80) = X; F32(v + 0x40) = X; F32(v) = X;
    F32(v + 0x64) = Y; F32(v + 0x24) = Y; F32(v + 4) = Y;
    F32(v + 0xA0) = X2; F32(v + 0x60) = X2; F32(v + 0x20) = X2;
    F32(v + 0xA4) = Y2; F32(v + 0x84) = Y2; F32(v + 0x44) = Y2;
    s32 si = (s32)i, page = si / 196;
    s32 u0 = (si % 14) * 18 + 1, v0 = ((si % 196) / 14) * 18 + 1;
    float U0, V0, U1, V1;
    if (U32(0x865E74) == 0x100) {                       /* a texture per page */
        U0 = mk_mul(u0, 0x704BAC); V0 = mk_mul(v0, 0x704BAC);
        U1 = mk_mul(u0 + 16, 0x704BAC); V1 = mk_mul(v0 + 16, 0x704BAC);
        U32(v + 0x54) = (u32)page;
    } else {                                            /* four pages in one 512x512 */
        u0 += (page % 2) << 8; v0 += (page / 2) << 8;
        U32(v + 0x54) = 0;
        U0 = mk_mul(u0, 0x704BA8); V0 = mk_mul(v0, 0x704BA8);
        U1 = mk_mul(u0 + 16, 0x704BA8); V1 = mk_mul(v0 + 16, 0x704BA8);
    }
    F32(v + 0x7C) = V0; F32(v + 0x3C) = V0; F32(v + 0x1C) = V0;
    F32(v + 0x78) = U1; F32(v + 0xB8) = U1; F32(v + 0x38) = U1;
    F32(v + 0xBC) = V1; F32(v + 0x5C) = V1; F32(v + 0x9C) = V1;
    F32(v + 0x98) = U0; F32(v + 0x58) = U0; F32(v + 0x18) = U0;
    for (u32 k = 0; k < 6; k++) { U32(v + 0x10 + 0x20 * k) = 0xFFFFFFFF; U32(v + 8 + 0x20 * k) = 0x3C23D70A; U32(v + 0xC + 0x20 * k) = 0x41200000; }
    U32(v + 0x14) = x; U32(v + 0x34) = y; U32(v + 0x74) = z; U32(v + 0x94) = 0;
}

/* all tiles d lines lower */
void PcDraw_MasksShift(u32 d)                                        /* 0x4BE6A0 */
{
    float f = mk_mul((s32)d, 0x8644B8);
    for (s32 i = 0; i < S32(MK_N); i++) { gptr y = U32(MK_VERTS) + 0x20 * i + 4; F32(y) = (float)((xreal)F32(y) - f); }
}

/* tiles by page, then nearest last (qsort comparator) */
s32 DC2CALL Masks_Compare(gptr a, gptr b)                           /* 0x4BEA80 */
{
    if (U32(a + 0x54) > U32(b + 0x54)) return 1;
    if (U32(a + 0x54) < U32(b + 0x54)) return -1;
    if (U32(a + 0x74) < U32(b + 0x74)) return 1;
    return U32(a + 0x74) > U32(b + 0x74) ? -1 : 0;
}

/* the tiles' pixels into a 256x768 surface, 14x14 to a page, then each page
   into its texture (or its quarter of the 512x512 one) */
static void masks_pack(void)                                         /* 0x4BE770 */
{
    gptr ss = Tmp_Push(4);
    u8 desc[SD_SIZE];
    surface_create(0x100, 0x300, ss, 0);
    memset(desc, 0, sizeof desc); *(u32 *)desc = SD_SIZE; *(u32 *)(desc + SD_PIXFMT) = 0x20;
    Surf_Lock(U32(ss), 0, desc, 1);
    u8 *base = *(u8 **)(desc + SD_SURFACE);
    s32 pitch = *(s32 *)(desc + SD_PITCH);
    gptr src = U32(MK_PIX);
    s32 page = 0, pbase = 0, col = 0, row = 0;
    for (s32 q = 0; q < S32(MK_N) / 6; q++) {
        u32 bpp = U8(0x87BB43);
        u8 *d = base + (pbase + row) * pitch + bpp * col;
        for (int r = 0; r < 18; r++, d += pitch) {
            u8 *o = d;
            for (int c = 0; c < 18; c++) {
                u32 p = U16(src); src += 2;
                u32 v = ((p & 0x3E0) << 14 >> U32(0x87BB48) & U32(0x87BB54)) | ((p & 0x1F) << 19 >> U32(0x87BB44) & U32(0x87BB50))
                      | ((p & 0x7C00) << 9 >> U32(0x87BB4C) & U32(0x87BB58)) | (p & 0x8000 ? U32(0x87BB5C) : 0);
                if (U8(0x87BB43) == 2) *(u16 *)o = (u16)v; else *(u32 *)o = v;
                o += U8(0x87BB43);
            }
        }
        if ((col += 18) == 0xFC) {
            col = 0;
            if ((row += 18) == 0xFC) { row = 0; page++; pbase += 0x100; }
        }
    }
    Surf_Unlock(U32(ss), 0);
    for (s32 i = 0; i <= page; i++) {
        s32 sr[4] = {0, i << 8, 0x100, (i << 8) + 0x100}, dr[4];
        if (U32(0x865E74) == 0x100) {
            tex_create_surface(0x100, 0x100, 0x8644A0 + 4 * i, 0x8644AC + 4 * i, 0);
            dr[0] = 0; dr[1] = 0;
        } else {
            if (!i) tex_create_surface(0x200, 0x200, 0x8644A0, 0x8644AC, 0);
            dr[0] = (i << 8) % 512; dr[1] = (i / 2) << 8;
        }
        dr[2] = dr[0] + 0x100; dr[3] = dr[1] + 0x100;
        Surf_Blt(U32(U32(0x865E74) == 0x100 ? 0x8644A0 + 4 * i : 0x8644A0), dr, U32(ss), sr, 0x1000000, 0);
    }
    Obj_Release(U32(ss));
    Tmp_Pop(4);
}

/* the tiles sorted, grouped (the vertex count on a group's first tile), their textures made */
void PcDraw_MasksBuild(void)                                         /* 0x4BE6E0 */
{
    Crt_qsort(U32(MK_VERTS), (u32)(S32(MK_N) / 6), 0xC0, 0x4BEA80);
    gptr head = U32(MK_VERTS), next = head + 0xC0;
    for (s32 k = 0; k < S32(MK_N); k += 6, next += 0xC0) {
        U32(head + 0x94) += 6;
        if (k != S32(MK_N) - 6 && U32(head + 0x54) == U32(next + 0x54) && U32(head + 0x74) == U32(next + 0x74)) continue;
        head = next;
    }
    masks_pack();
}

/* after the device was lost: the vertex buffer and textures again, the
   tiles placed for the screen's scale */
static void masks_recreate(void)                                     /* 0x4BEB30 */
{
    masks_release(0);
    masks_vb(U32(MK_N));
    for (s32 k = 0; k < S32(MK_N); k += 6) {
        gptr v = U32(MK_VERTS) + k * 0x20;
        xreal x = (xreal)U32(v + 0x14);
        float y = (float)(xreal)U32(v + 0x34);
        float X = (float)(F32(0x8644BC) * x), Y = (float)(F32(0x8644B8) * (xreal)y);
        F32(v + 0x80) = X; F32(v + 0x40) = X; F32(v) = X;
        F32(v + 0x64) = Y; F32(v + 0x24) = Y; F32(v + 4) = Y;
        float X2 = (float)((x + F32(0x7031B0)) * F32(0x8644BC)), Y2 = (float)(((xreal)y + F32(0x7031B0)) * F32(0x8644B8));
        F32(v + 0xA0) = X2; F32(v + 0x60) = X2; F32(v + 0x20) = X2;
        F32(v + 0xA4) = Y2; F32(v + 0x84) = Y2; F32(v + 0x44) = Y2;
    }
    masks_pack();
}

/* the vertices into the vertex buffer (made again when asked or missing) */
void PcDraw_MasksUpload(u32 again)                                   /* 0x4BEAC0 */
{
    if (!U32(A_DEVICE) || !U32(MK_N)) return;
    if (!U32(MK_VB) || again) masks_recreate();
    void *p = 0;
    VB_Lock(U32(MK_VB), 0x20, &p);
    memcpy(p, MEM(U32(MK_VERTS)), U32(MK_N) * 0x20);
    VB_Unlock(U32(MK_VB));
}

/* the 0x88 packet of the group at vertex i: its vertices and texture; its depth */
u32 PcDraw_MasksMarker(u32 i, gptr p)                                /* 0x4BEC40 */
{
    gptr v = U32(MK_VERTS) + i * 0x20;
    U32(p + 8) = i;
    U16(p + 0xE) = U16(v + 0x54);
    U16(p + 0xC) = U16(v + 0x94);
    return U32(v + 0x74);
}
