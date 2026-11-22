/* The standalone builds' Direct3D 6 (include/dc2/gpudev.h, GpuDev): DirectDraw
 * surfaces as pixel buffers, textures over them, the render and texture-stage
 * states, the viewport's background colour; draws and clears handed to a
 * Renderer (include/dc2/render.h). Also the PC layer's globals as the port's
 * initialisation (0x4BBA30) leaves them. */
#ifndef DC2_IN_PROCESS
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dc2/types.h"
#include "dc2/gpudev.h"
#include "dc2/render.h"
#include "dc2/pcdraw.h"

enum { K_NONE, K_DDRAW, K_D3D, K_DEVICE, K_SURF, K_TEX, K_VIEWPORT, K_MATERIAL, K_VB };

typedef struct {
    u8 kind, back;                  /* back: the back buffer (the renderer's) */
    u32 w, h, bpp, version, surf;   /* (surf: a texture's surface) */
    u8 *pix;
    RVertex *verts; u32 nverts;     /* a vertex buffer */
} Obj;

#define NOBJ 2048
static Obj g_obj[NOBJ];
/* the port's way to give RAM back when a surface does not fit (the
   Dreamcast drops its camera cache); 1 when it did */
int (*g_low_memory)(void);
static u32 g_surf_bytes, g_surf_peak;     /* the surfaces' pixels now and at most */
static const Renderer *g_r;
static u32 g_rs[0x100], g_tss[2][0x20], g_tex, g_bg_rgb;
static u32 g_screen_w = 640, g_screen_h = 480;

static u32 obj_new(u32 kind)
{
    for (u32 i = 1; i < NOBJ; i++)
        if (!g_obj[i].kind) { memset(&g_obj[i], 0, sizeof g_obj[i]); g_obj[i].kind = (u8)kind; return i; }
    return 0;
}
static Obj *obj(u32 h) { return h && h < NOBJ && g_obj[h].kind ? &g_obj[h] : 0; }
static Obj *surf_of(u32 h) { Obj *o = obj(h); if (o && o->kind == K_TEX) o = obj(o->surf); return o && o->kind == K_SURF ? o : 0; }

static u32 begin_scene(void) { return 0; }
static u32 end_scene(void) { return 0; }
static u32 set_rs(u32 s, u32 v) { if (s < 0x100) g_rs[s] = v; return 0; }
static u32 set_tss(u32 st, u32 ty, u32 v) { if (st < 2 && ty < 0x20) g_tss[st][ty] = v; return 0; }
static u32 set_texture(u32 st, u32 t) { if (!st) g_tex = t; return 0; }

static void state(RState *s)
{
    memset(s, 0, sizeof *s);
    s->blend = g_rs[0x1B] != 0; s->src = (u8)g_rs[0x13]; s->dst = (u8)g_rs[0x14];
    s->alpha_test = g_rs[0xF] != 0; s->alpha_ref = (u8)g_rs[0x18]; s->alpha_func = (u8)g_rs[0x19];
    s->z_test = g_rs[7] != 0; s->z_write = g_rs[0xE] != 0; s->z_func = (u8)g_rs[0x17];
    s->flat = g_rs[9] == 1; s->fog = g_rs[0x1C] != 0; s->fog_table = (u8)g_rs[0x23];
    s->modulate2x = g_tss[0][1] == 5; s->filter = g_tss[0][16] == 2;
    s->fog_color = g_rs[0x22] & 0xFFFFFF;
    memcpy(&s->fog_start, &g_rs[0x24], 4); memcpy(&s->fog_end, &g_rs[0x25], 4);
}

static u8 *pix_of(Obj *o)
{
    if (o->pix || !o->w || !o->h) return o->pix;
    o->pix = calloc((size_t)o->w * o->h, o->bpp);
    if (!o->pix && g_low_memory && g_low_memory()) o->pix = calloc((size_t)o->w * o->h, o->bpp);   /* (RAM given back: once more) */
    if (!o->pix) return NULL;
    g_surf_bytes += o->w * o->h * o->bpp;
    if (g_surf_bytes > g_surf_peak) {
        g_surf_peak = g_surf_bytes;
        if (getenv("DC2_SURFLOG")) {             /* the live surfaces at a new peak */
            fprintf(stderr, "surfaces %u KB:", (unsigned)(g_surf_bytes >> 10));
            for (u32 i = 1; i < sizeof g_obj / sizeof g_obj[0]; i++)
                if (g_obj[i].kind == K_SURF && g_obj[i].pix) fprintf(stderr, " %ux%ux%u", (unsigned)g_obj[i].w, (unsigned)g_obj[i].h, (unsigned)g_obj[i].bpp);
            fprintf(stderr, "\n");
        }
    }
    return o->pix;
}

static void draw_verts(u32 type, const RVertex *v, u32 n)
{
    RState s; RTexture t, *tp = 0;
    state(&s);
    Obj *so = surf_of(g_tex);
    if (so && pix_of(so)) {
        t.id = obj(g_tex)->kind == K_TEX ? obj(g_tex)->surf : g_tex; t.version = so->version;
        t.w = so->w; t.h = so->h; t.bpp = so->bpp; t.pitch = so->w * so->bpp; t.pixels = so->pix;
        tp = &t;
    }
    if (g_r && g_r->draw) g_r->draw((int)type, v, (int)n, &s, tp);
}

static u32 draw(u32 type, u32 fvf, const void *verts, u32 n, u32 flags)
{
    (void)flags;
    if (fvf != 0x1C4) return 0;
    draw_verts(type, verts, n);
    return 0;
}
static u32 draw_vb(u32 type, u32 vb, u32 start, u32 n, u32 flags)
{
    (void)flags;
    Obj *o = obj(vb);
    if (!o || o->kind != K_VB || start + n > o->nverts) return 0;
    draw_verts(type, o->verts + start, n);
    return 0;
}

static u32 vp_clear(u32 n, const void *rects, u32 flags)
{
    (void)n; (void)rects;
    if (g_r && g_r->clear) g_r->clear(flags & 1, (flags & 2) != 0, g_bg_rgb);
    return 0;
}
static u32 vp_set_background(u32 h) { (void)h; return 0; }
static u32 set_material(u32 m, const void *d)
{
    (void)m;
    const float *f = (const float *)((const u8 *)d + 4);
    u32 c = 0;
    for (int k = 0; k < 3; k++) { float x = f[k] < 0 ? 0 : f[k] > 1 ? 1 : f[k]; c = c << 8 | (u32)(x * 255.0f + 0.5f); }
    g_bg_rgb = c;
    return 0;
}

/* a surface's pixels, cleared, made the first time anything locks, draws
   from or into it: a surface the game never touches costs nothing (the
   primary surface, a back buffer the renderer draws itself) */
static u8 *pix_of(Obj *o);
u32 PcSoft_SurfacePeak(void) { return g_surf_peak; }
static u32 create_surface(void *desc, u32 *out)
{
    u8 *d = desc;
    u32 h = obj_new(K_SURF);
    if (!h) { *out = 0; return 1; }
    Obj *o = &g_obj[h];
    o->w = *(u32 *)(d + SD_WIDTH); o->h = *(u32 *)(d + SD_HEIGHT);
    u32 bits = *(u32 *)(d + SD_PIXFMT + 0xC);
    o->bpp = bits > 16 ? 4 : 2;
    o->pix = NULL;                               /* (made on first use: pix_of) */
    *out = h;
    return 0;
}
static u32 query_interface(u32 o, const void *iid, u32 *out)
{
    (void)iid;
    u32 h = obj_new(K_TEX);
    if (!h || !obj(o)) { *out = 0; return 1; }
    g_obj[h].surf = o;
    *out = h;
    return 0;
}
static u32 release(u32 h)
{
    Obj *o = obj(h);
    if (!o) return 0;
    if (o->kind == K_SURF && g_r && g_r->texture_freed) g_r->texture_freed(h);
    if (o->kind == K_SURF && o->pix) g_surf_bytes -= o->w * o->h * o->bpp;
    free(o->pix); free(o->verts);
    memset(o, 0, sizeof *o);
    return 0;
}

static void pix_put(Obj *o, u32 x, u32 y, u32 c) { if (o->bpp == 2) ((u16 *)o->pix)[y * o->w + x] = (u16)c; else ((u32 *)o->pix)[y * o->w + x] = c; }
static u32 pix_get(const Obj *o, u32 x, u32 y) { return o->bpp == 2 ? ((u16 *)o->pix)[y * o->w + x] : ((u32 *)o->pix)[y * o->w + x]; }

/* pixels drawn into the back buffer over [x0,x1) x [y0,y1), stretched (a
   blit to the screen: the movies' frames) */
void PcSoft_DrawImage(u32 id, u32 version, const void *pix, u32 w, u32 h, u32 bpp, s32 x0, s32 y0, s32 x1, s32 y1)
{
    if (!g_r || !g_r->draw || !w || !h) return;
    RTexture t = {id, version, w, h, bpp, w * bpp, pix};
    RState s;
    memset(&s, 0, sizeof s);
    s.filter = 1;
    RVertex v[4];
    memset(v, 0, sizeof v);
    for (int i = 0; i < 4; i++) {
        v[i].x = (float)(i & 1 ? x1 : x0); v[i].y = (float)(i & 2 ? y1 : y0);
        v[i].rhw = 1.0f; v[i].diffuse = 0xFFFFFFFF;
        v[i].u = i & 1 ? 1.0f : 0.0f; v[i].v = i & 2 ? 1.0f : 0.0f;
    }
    g_r->draw(5, v, 4, &s, &t);
}

static u32 blt(u32 dst, const void *dr, u32 src, const void *sr, u32 flags, const void *fx)
{
    Obj *d = obj(dst), *s = obj(src);
    if (!d || d->kind != K_SURF) return 1;
    u32 r[4] = {0, 0, d->w, d->h}, q[4] = {0, 0, s ? s->w : 0, s ? s->h : 0};
    if (dr) memcpy(r, dr, 16);
    if (sr) memcpy(q, sr, 16);
    if (flags & 0x400) {                        /* colour fill */
        u32 c = fx ? *(const u32 *)((const u8 *)fx + 0x50) : 0;
        if (d->back) { if (g_r && g_r->clear) g_r->clear(1, 0, c); return 0; }
        if (!pix_of(d)) return 1;
        for (u32 y = r[1]; y < r[3] && y < d->h; y++) {
            u32 x1 = r[2] < d->w ? r[2] : d->w;
            if (d->bpp == 2) { u16 *p = (u16 *)d->pix + y * d->w; for (u32 x = r[0]; x < x1; x++) p[x] = (u16)c; }
            else { u32 *p = (u32 *)d->pix + y * d->w; for (u32 x = r[0]; x < x1; x++) p[x] = c; }
        }
        d->version++;
        return 0;
    }
    if (!s || s->kind != K_SURF || !pix_of(s)) return 1;
    if (s->back && g_r && g_r->read_back) g_r->read_back((u16 *)s->pix, s->w, s->h, s->w * 2);
    if (d->back) {                              /* to the screen: drawn by the renderer */
        if (!s->back && s->pix && !sr && s->w && s->h)
            PcSoft_DrawImage(src, s->version, s->pix, s->w, s->h, s->bpp, (s32)r[0], (s32)r[1], (s32)r[2], (s32)r[3]);
        return 0;
    }
    u32 dw = r[2] - r[0], dh = r[3] - r[1], sw = q[2] - q[0], sh = q[3] - q[1];
    if (!dw || !dh) return 0;
    if (!pix_of(d)) return 1;
    if (s != d && dw == sw && dh == sh && s->bpp == d->bpp && r[0] + dw <= d->w && r[1] + dh <= d->h && q[0] + sw <= s->w && q[1] + sh <= s->h
        && r[0] + dw >= r[0] && q[0] + sw >= q[0]) {   /* (the usual one: unscaled, inside both; rows as they are) */
        u32 bpp = d->bpp;
        for (u32 y = 0; y < dh; y++)
            memmove(d->pix + ((r[1] + y) * d->w + r[0]) * bpp, s->pix + ((q[1] + y) * s->w + q[0]) * bpp, dw * bpp);
        d->version++;
        return 0;
    }
    if (s != d && s->bpp == d->bpp && dw <= 2048) {    /* scaled, same format: the columns worked out once */
        static u32 xs[2048];
        u32 x0 = 0, x1 = 0;                         /* the columns [x0, x1) inside both */
        for (u32 x = 0; x < dw; x++) {
            u32 sx = q[0] + x * sw / dw;
            xs[x] = sx;
            if (r[0] + x < d->w && sx < s->w) { if (x1 == x0) x0 = x; x1 = x + 1; }
        }
        for (u32 y = 0; y < dh; y++) {
            u32 sy = q[1] + y * sh / dh;
            if (r[1] + y >= d->h || sy >= s->h) continue;
            for (u32 x = x0; x < x1; x++) {
                if (r[0] + x >= d->w || xs[x] >= s->w) continue;
                if (d->bpp == 2) ((u16 *)d->pix)[(r[1] + y) * d->w + r[0] + x] = ((const u16 *)s->pix)[sy * s->w + xs[x]];
                else ((u32 *)d->pix)[(r[1] + y) * d->w + r[0] + x] = ((const u32 *)s->pix)[sy * s->w + xs[x]];
            }
        }
        d->version++;
        return 0;
    }
    for (u32 y = 0; y < dh; y++) {
        u32 sy = q[1] + y * sh / dh;
        if (r[1] + y >= d->h || sy >= s->h) continue;
        for (u32 x = 0; x < dw; x++) {
            u32 sx = q[0] + x * sw / dw;
            if (r[0] + x >= d->w || sx >= s->w) continue;
            u32 c = pix_get(s, sx, sy);
            if (s->bpp != d->bpp) c = s->bpp == 2 ? ((c & 0x8000 ? 0xFF000000u : 0) | (c & 0x7C00) << 9 | (c & 0x3E0) << 6 | (c & 0x1F) << 3)
                                                  : ((c >> 16 & 0x8000) | (c >> 9 & 0x7C00) | (c >> 6 & 0x3E0) | (c >> 3 & 0x1F));
            pix_put(d, r[0] + x, r[1] + y, c);
        }
    }
    d->version++;
    return 0;
}

static u32 flip(u32 s, u32 t, u32 f)
{
    (void)s; (void)t; (void)f;
    if (g_r && g_r->present) g_r->present();
    if (g_r && g_r->begin_frame) g_r->begin_frame();
    return 0;
}
static u32 lock(u32 s, const void *rect, void *desc, u32 flags)
{
    (void)rect; (void)flags;
    Obj *o = obj(s);
    if (!o || o->kind != K_SURF || !pix_of(o)) return 1;
    u8 *d = desc;
    if (o->back && g_r && g_r->read_back) g_r->read_back((u16 *)o->pix, o->w, o->h, o->w * 2);
    *(u32 *)(d + SD_HEIGHT) = o->h; *(u32 *)(d + SD_WIDTH) = o->w; *(u32 *)(d + SD_PITCH) = o->w * o->bpp;
    *(u8 **)(d + SD_SURFACE) = o->pix;
    return 0;
}
static u32 unlock(u32 s, const void *rect) { (void)rect; Obj *o = obj(s); if (o) o->version++; return 0; }
static u32 restore(u32 s) { (void)s; return 0; }
static u32 set_color_key(u32 s, u32 f, const void *k) { (void)s; (void)f; (void)k; return 0; }

static u32 create_vb(const void *desc, u32 *out)
{
    u32 n = ((const u32 *)desc)[3], h = obj_new(K_VB);
    if (!h) { *out = 0; return 1; }
    g_obj[h].verts = calloc(n ? n : 1, sizeof(RVertex)); g_obj[h].nverts = n;
    *out = h;
    return 0;
}
static u32 vb_lock(u32 vb, void **p) { Obj *o = obj(vb); if (!o || o->kind != K_VB) return 1; *p = o->verts; return 0; }
static u32 vb_unlock(u32 vb) { (void)vb; return 0; }

static const GpuDev k_soft = {
    begin_scene, end_scene, set_rs, draw, draw_vb, set_texture, set_tss, vp_clear, vp_set_background, set_material,
    create_surface, query_interface, release, blt, flip, lock, restore, set_color_key, unlock,
    create_vb, vb_lock, vb_unlock,
};

/* ---- the PC layer's globals, as 0x4BBA30 and the device set-up leave them -------------------- */

/* a 16-bit A1R5G5B5 texture format entry (0x40): the converters' shifts and masks, a DDPIXELFORMAT */
static void pixfmt_1555(gptr e)
{
    memset(MEM(e), 0, 0x40);
    U8(e + 2) = 0x10; U8(e + 3) = 2;
    U32(e + 4) = 9; U32(e + 8) = 14; U32(e + 0xC) = 19;
    U32(e + 0x10) = 0x7C00; U32(e + 0x14) = 0x3E0; U32(e + 0x18) = 0x1F; U32(e + 0x1C) = 0x8000;
    U32(e + 0x20) = 0x20; U32(e + 0x24) = 0x41; U32(e + 0x2C) = 16;
    U32(e + 0x30) = 0x7C00; U32(e + 0x34) = 0x3E0; U32(e + 0x38) = 0x1F; U32(e + 0x3C) = 0x8000;
}

static u32 mk(u32 kind) { return obj_new(kind); }
static u32 mk_surface(u32 w, u32 h, int back)
{
    u8 d[SD_SIZE]; u32 s;
    memset(d, 0, sizeof d);
    *(u32 *)(d + SD_WIDTH) = w; *(u32 *)(d + SD_HEIGHT) = h; *(u32 *)(d + SD_PIXFMT + 0xC) = 16;
    create_surface(d, &s);
    g_obj[s].back = (u8)back;
    return s;
}

void PcDraw_InitStandalone(const Renderer *r, u32 w, u32 h)
{
    g_r = r; g_gpudev = &k_soft; g_screen_w = w; g_screen_h = h;
    memset(MEM(0x75E488), 0, 0x6000);                   /* texture cache */
    memset(MEM(0x75CEE8), 0, 0x1000);                   /* converted palettes */
    memset(MEM(0x8644C0), 0, 0x1400);                   /* glyphs: none (0xFFFF) */
    for (gptr a = 0x8644C0; a < 0x8658C0; a += 0x14) U16(a) = 0xFFFF;
    memset(MEM(0x865D48), 0, 0x1C); memset(MEM(0x865D98), 0, 0x28);
    pixfmt_1555(0x87BB40); pixfmt_1555(0x87BB80); pixfmt_1555(0x87BBC0);
    U32(0x75DEF0) = 0; U32(0x75DEEC) = 0;
    for (gptr a = 0x73B680; a <= 0x73B690; a += 4) U32(a) = 0xFFFFFFFF;
    for (u32 i = 0; i < 256; i++) *(float *)MEM(0x865940 + 4 * i) = (float)((double)i * 0.00390625 + 0.002);
    U32(0x865D40) = 16;                                 /* display depth */
    U8(0x865E44) = 2; U32(0x865E6C) = 8; U32(0x865E70) = 8; U32(0x865E74) = 1024; U32(0x865E78) = 1024;   /* powers of 2 */
    U32(0x865EE8) = 2;                                  /* fog by vertex (the game computes it) */
    U8(0x87BAF8) = 1;                                   /* the back buffer cleared each frame */
    *(float *)MEM(0x8644BC) = (float)w / 320.0f; *(float *)MEM(0x8644B8) = (float)h / 240.0f;
    U32(0x73B660) = 0; U32(0x73B664) = 0; U32(0x73B668) = w; U32(0x73B66C) = h;
    U32(0x75BE58) = 0; U32(0x75BE5C) = 0; U32(0x75BE60) = w; U32(0x75BE64) = h;
    U32(A_DDRAW) = mk(K_DDRAW); U32(0x865ED4) = mk(K_D3D); U32(A_DEVICE) = mk(K_DEVICE);
    U32(A_PRIMARY) = mk_surface(w, h, 1); U32(A_BACK) = mk_surface(w, h, 1);
    U32(A_STAGING) = mk_surface(256, 256, 0);
    U32(A_VIEWPORT) = mk(K_VIEWPORT); U32(A_MATERIAL) = mk(K_MATERIAL); U32(0x75DEE8) = 1;
    /* the device's states (0x4BC300) */
    static const u8 tss[][3] = {{0, 2, 2}, {0, 3, 0}, {0, 1, 4}, {0, 5, 2}, {0, 6, 0}, {0, 4, 4}, {1, 1, 1}, {1, 4, 1},
                                {0, 17, 2}, {0, 16, 2}, {0, 12, 3}};
    for (u32 i = 0; i < sizeof tss / 3; i++) set_tss(tss[i][0], tss[i][1], tss[i][2]);
    static const u8 rs[][2] = {{0x1B, 1}, {0xF, 1}, {0x18, 8}, {0x19, 5}, {4, 1}, {0x16, 1}, {0x1D, 0}, {7, 1}, {0xE, 1}, {0x17, 4},
                               {0x1C, 0}, {0x23, 0}};
    for (u32 i = 0; i < sizeof rs / 2; i++) set_rs(rs[i][0], rs[i][1]);
    for (gptr v = 0x8658C0; v < 0x865940; v += 0x20) U32(v + 0x10) = 0xFFFFFFFF;    /* (0x4BCB00) */
    if (r && r->begin_frame) r->begin_frame();
    PcDraw_SetFarColor(U32(0x865EF0), U32(0x73B674), U32(0x73B678));
    PcDraw_SetFogEnable(U32(0x865EEC));
    PcDraw_Clear(1);
}
#endif
