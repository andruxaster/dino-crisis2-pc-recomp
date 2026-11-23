/* The Dreamcast renderer (include/dc2/render.h) on the PowerVR2 through KOS's
 * pvr_* API, the way the Resident Evil DC port draws:
 *   one translucent list, autosort off: polygons come out in submission order
 *   (the order table's); no blending is ONE/ZERO;
 *   depth on 1/w: D3D's z test (less-equal on z) becomes greater-equal on rhw;
 *   textures twiddled ARGB1555 (the PC layer's format already), re-uploaded on
 *   a new version, old VRAM freed only once the PVR is done with the scene;
 *   when VRAM runs short the textures unused longest go first (none the PVR
 *   may still draw from: the current and the last scene's);
 *   vertex fog: the specular's alpha (D3D: 255 no fog) as the offset colour's
 *   alpha (PVR: 255 all fog);
 *   flat shading: the first vertex's colour on all (the PVR takes the last);
 *   triangle lists and fans as 3-vertex strips, lines as 1-pixel quads.
 * Not done: alpha test (no punch-through list: transparent texels still write
 * depth), MODULATE2X (the colour doubled instead), read-back (the frame copy
 * sees black). */
#include "dc2/crumb.h"
#include <stddef.h>
#include <stdint.h>
#include <dc/pvr.h>
#include <kos/dbglog.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "dc2/render.h"
#include "render_pvr.h"
#include "pvr_tile.h"
#include "pvr_twiddle.h"
#include "dc2/stall.h"

#define VERTEX_BUF   (512 * 1024)
#define NTEX         1024
#define NPENDING     256

/* a texture in VRAM: nx x ny pieces (pvr_tile.h), piece (i, j) at texels
   xo[i], yo[j] of pw[i] x ph[j] (powers of two) in mem[j * nx + i]; used: the
   scene it was last drawn in; linear: stored in rows, not twiddled (one
   that has changed once, as the camera backgrounds do, likely changes
   again: rows are a plain copy, twiddling costs a pass over every pixel) */
typedef struct {
    u32 id, version, w, h, used;
    u8 nx, ny, linear;
    u16 xo[TILE_MAX + 1], yo[TILE_MAX + 1], pw[TILE_MAX], ph[TILE_MAX];
    pvr_ptr_t mem[TILE_MAX * TILE_MAX];
} Tex;
static Tex g_tex[NTEX];
static pvr_ptr_t g_pending[2][NPENDING];        /* freed once the scene that may use them is drawn */
static int g_npending[2], g_cur;
static int g_in_scene;
static u32 g_scene = 2;                         /* scenes begun (from 2: "used < g_scene - 1" means not in the last two) */
static u32 g_evicted;                           /* textures dropped for room (in the log) */
static u32 g_frag;                              /* allocations that failed with enough free (fragments), then retried */
static u32 g_bg;

/* headers and vertices staged, sent 8 KB at a time */
static u8 g_ta[8192] __attribute__((aligned(32)));
static u32 g_ta_used;
static void ta_flush(void) { if (g_ta_used) { pvr_prim(g_ta, g_ta_used); g_ta_used = 0; } }
static void *ta_alloc(void) { if (g_ta_used + 32 > sizeof g_ta) ta_flush(); void *r = g_ta + g_ta_used; g_ta_used += 32; return r; }

static void defer_free(pvr_ptr_t m)
{
    if (!m) return;
    if (g_npending[g_cur] == NPENDING) { pvr_wait_ready(); for (int i = 0; i < g_npending[g_cur]; i++) pvr_mem_free(g_pending[g_cur][i]); g_npending[g_cur] = 0; }
    g_pending[g_cur][g_npending[g_cur]++] = m;
}

/* all of a texture's pieces given back (after the scenes that may draw them) */
static void tex_release(Tex *e)
{
    for (int i = 0; i < e->nx * e->ny; i++) { defer_free(e->mem[i]); e->mem[i] = 0; }
    e->nx = e->ny = 0;
    e->id = 0;
}

static void scene_begin(void)
{
    if (g_in_scene) return;
    pvr_wait_ready();
    g_cur ^= 1;                                  /* the scene before last is done now */
    for (int i = 0; i < g_npending[g_cur]; i++) pvr_mem_free(g_pending[g_cur][i]);
    g_npending[g_cur] = 0;
    g_scene++;
    pvr_set_bg_color((g_bg >> 16 & 0xFF) / 255.0f, (g_bg >> 8 & 0xFF) / 255.0f, (g_bg & 0xFF) / 255.0f);
    CRUMB('B');
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_TR_POLY);
    g_in_scene = 1;
}

/* the texture unused longest, the PVR done with it, out of VRAM; 0: none */
static int evict_one(void)
{
    Tex *old = 0;
    for (int i = 0; i < NTEX; i++) {
        Tex *e = &g_tex[i];
        if (e->nx && e->used + 1 < g_scene && (!old || e->used < old->used)) old = e;
    }
    if (!old) return 0;
    for (int i = 0; i < old->nx * old->ny; i++) { pvr_mem_free(old->mem[i]); old->mem[i] = 0; }
    old->nx = old->ny = 0; old->id = 0;
    g_evicted++;
    return 1;
}

/* VRAM for a texture: room made first (some to spare, against the heap's
   fragments), then textures dropped one by one while it does not fit */
#define VRAM_SPARE (256 * 1024)
static pvr_ptr_t vram_alloc(u32 n)
{
    while (pvr_mem_available() < n + VRAM_SPARE && evict_one()) {}
    for (;;) {
        if (pvr_mem_available() >= n) {
            /* a failure here is expected (the free VRAM in pieces) and
               handled below: KOS's "out of PVR memory" message kept quiet */
            int lvl = dbglog_level;
            dbglog_set_level(DBG_CRITICAL);
            pvr_ptr_t m = pvr_mem_malloc(n);
            dbglog_set_level(lvl);
            if (m) return m;
            g_frag++;
        }
        if (!evict_one()) return 0;
    }
}

u32 RenderPvr_Evicted(void) { return g_evicted; }
u32 RenderPvr_Fragmented(void) { return g_frag; }

static u32 pow2(u32 n) { u32 p = 8; while (p < n) p <<= 1; return p; }

/* 32-bit B, G, R, A to ARGB1555 (alpha from 128 up opaque) */
static u16 to1555(const u8 *p) { return (u16)((p[3] >= 128 ? 0x8000 : 0) | (p[2] >> 3) << 10 | (p[1] >> 3) << 5 | p[0] >> 3); }

/* texels x0.. x0+tw, y0.. y0+th of t into a pw x ph piece at mem (32-bit
   pixels made 1555; the padding repeats the last row and column, for the
   filtering), through a temporary unless the texels fit as they are */
static int tex_upload1(const RTexture *t, pvr_ptr_t mem, u32 x0, u32 y0, u32 tw, u32 th, u32 pw, u32 ph);
static int tex_upload_rows(const RTexture *t, pvr_ptr_t mem, u32 x0, u32 y0, u32 tw, u32 th, u32 pw, u32 ph);
static int tex_upload(const RTexture *t, pvr_ptr_t mem, u32 x0, u32 y0, u32 tw, u32 th, u32 pw, u32 ph, int linear)
{
    STALL_BEGIN();
    int r = linear ? tex_upload_rows(t, mem, x0, y0, tw, th, pw, ph) : tex_upload1(t, mem, x0, y0, tw, th, pw, ph);
    STALL_END(STALL_TEX);
    return r;
}
/* the piece's pixels a tile at a time (pvr_twiddle_tiles): no buffer of the
   texture's size, only 2 KB tiles, each sent by store queues */
typedef struct { const RTexture *t; pvr_ptr_t mem; u32 x0, y0, tw, th; } Upload;

static void up_fetch(void *c, unsigned x, unsigned y, unsigned n, unsigned short *out)
{
    const Upload *u = c;
    if (y >= u->th) y = u->th - 1;                  /* (past the picture: its last row and column repeated) */
    const u8 *src = (const u8 *)u->t->pixels + (u->y0 + y) * u->t->pitch + u->x0 * u->t->bpp;
    unsigned in = x < u->tw ? (u->tw - x < n ? u->tw - x : n) : 0;
    if (u->t->bpp == 2) memcpy(out, src + x * 2, in * 2);
    else for (unsigned i = 0; i < in; i++) out[i] = to1555(src + 4 * (x + i));
    if (in < n) {
        u16 last = u->t->bpp == 2 ? ((const u16 *)src)[u->tw - 1] : to1555(src + 4 * (u->tw - 1));
        for (unsigned i = in; i < n; i++) out[i] = last;
    }
}

static void up_emit(void *c, unsigned at, const unsigned short *tile, unsigned n)
{
    const Upload *u = c;
    if (n * 2 >= 32) pvr_txr_load((void *)tile, (u8 *)u->mem + at * 2, n * 2);
    else memcpy((u8 *)u->mem + at * 2, tile, n * 2);   /* (an 8 x 8 texture's 128 bytes go by store queues too; smaller: none) */
}

static int tex_upload1(const RTexture *t, pvr_ptr_t mem, u32 x0, u32 y0, u32 tw, u32 th, u32 pw, u32 ph)
{
    Upload u = {t, mem, x0, y0, tw, th};
    pvr_twiddle_tiles(pw, ph, up_fetch, up_emit, &u);
    return 1;
}

/* the same in rows (a non-twiddled texture: pw a power of two, no stride) */
static int tex_upload_rows(const RTexture *t, pvr_ptr_t mem, u32 x0, u32 y0, u32 tw, u32 th, u32 pw, u32 ph)
{
    static u16 row[1024] __attribute__((aligned(32)));
    Upload u = {t, mem, x0, y0, tw, th};
    for (u32 y = 0; y < ph; y++) {
        up_fetch(&u, 0, y, pw, row);
        up_emit(&u, y * pw, row, pw);
    }
    return 1;
}

/* a texture in VRAM, uploaded again on a new version: in pieces of powers of
   two (the twiddled formats need them), a large one in several (pvr_tile.h) */
static Tex *tex_get(const RTexture *t)
{
    if (!t || (t->bpp != 2 && t->bpp != 4) || !t->w || !t->h || t->w > 1024 || t->h > 1024) return 0;
    Tex *e = &g_tex[t->id % NTEX];
    if (e->id != t->id || !e->nx || e->version != t->version || e->w != t->w || e->h != t->h) {
        int linear = e->id == t->id && e->nx && e->w == t->w && e->h == t->h;   /* (a new version of the same one) */
        tex_release(e);
        e->linear = (u8)linear;
        e->used = g_scene;                      /* (not taken for room by its own pieces' vram_alloc) */
        e->nx = (u8)tile_split(t->w, e->xo, e->pw);
        e->ny = (u8)tile_split(t->h, e->yo, e->ph);
        for (int j = 0; j < e->ny; j++)
            for (int i = 0; i < e->nx; i++) {
                pvr_ptr_t m = vram_alloc((u32)e->pw[i] * e->ph[j] * 2);
                e->mem[j * e->nx + i] = m;
                if (!m || !tex_upload(t, m, e->xo[i], e->yo[j], e->xo[i + 1] - e->xo[i], e->yo[j + 1] - e->yo[j], e->pw[i], e->ph[j], e->linear)) {
                    tex_release(e);         /* (those made: freed after the scenes, as any) */
                    return 0;
                }
            }
        e->id = t->id; e->version = t->version; e->w = t->w; e->h = t->h;
    }
    e->used = g_scene;
    return e;
}

/* D3DBLEND as the PVR's source or destination factor (2/3: the other side's colour) */
static int blend_factor(u8 b, int dst)
{
    switch (b) {
    case 1: return PVR_BLEND_ZERO;
    case 2: return PVR_BLEND_ONE;
    case 3: return dst ? PVR_BLEND_DESTCOLOR : PVR_BLEND_ONE;          /* SRCCOLOR (as a source: not on the PVR) */
    case 4: return dst ? PVR_BLEND_INVDESTCOLOR : PVR_BLEND_ZERO;      /* INVSRCCOLOR */
    case 5: return PVR_BLEND_SRCALPHA;
    case 6: return PVR_BLEND_INVSRCALPHA;
    case 7: return PVR_BLEND_DESTALPHA;
    case 8: return PVR_BLEND_INVDESTALPHA;
    case 9: return dst ? PVR_BLEND_ONE : PVR_BLEND_DESTCOLOR;          /* DESTCOLOR */
    case 10: return dst ? PVR_BLEND_ZERO : PVR_BLEND_INVDESTCOLOR;     /* INVDESTCOLOR */
    }
    return PVR_BLEND_ONE;
}

/* D3D's comparison on z as the PVR's on 1/w */
static int depth_cmp(u8 f)
{
    static const u8 k[9] = {PVR_DEPTHCMP_ALWAYS, PVR_DEPTHCMP_NEVER, PVR_DEPTHCMP_GREATER, PVR_DEPTHCMP_EQUAL, PVR_DEPTHCMP_GEQUAL,
                            PVR_DEPTHCMP_LESS, PVR_DEPTHCMP_NOTEQUAL, PVR_DEPTHCMP_LEQUAL, PVR_DEPTHCMP_ALWAYS};
    return f <= 8 ? k[f] : PVR_DEPTHCMP_ALWAYS;
}

/* the polygon header: piece k of texture t (or no texture) */
static void header(const RState *s, const Tex *t, int k)
{
    pvr_poly_cxt_t c;
    if (t) {
        int i = k % t->nx, j = k / t->nx;
        pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | (t->linear ? PVR_TXRFMT_NONTWIDDLED : PVR_TXRFMT_TWIDDLED),
                         t->pw[i], t->ph[j], t->mem[k],
                         s->filter ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE);
        c.txr.env = PVR_TXRENV_MODULATEALPHA;
        c.txr.uv_clamp = PVR_UVCLAMP_UV;
    } else pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = s->flat ? PVR_SHADE_FLAT : PVR_SHADE_GOURAUD;
    if (s->blend) { c.blend.src = blend_factor(s->src, 0); c.blend.dst = blend_factor(s->dst, 1); }
    else { c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ZERO; }
    c.depth.comparison = s->z_test ? depth_cmp(s->z_func) : PVR_DEPTHCMP_ALWAYS;
    c.depth.write = s->z_write ? PVR_DEPTHWRITE_ENABLE : PVR_DEPTHWRITE_DISABLE;
    if (s->fog && s->fog_table == 0) { c.gen.fog_type = PVR_FOG_VERTEX; c.gen.specular = PVR_SPECULAR_ENABLE; }
    else c.gen.fog_type = PVR_FOG_DISABLE;
    pvr_poly_hdr_t *h = ta_alloc();
    pvr_poly_compile(h, &c);
}

static u32 g_fog_rgb = 0xFFFFFFFF;
static int g_double;
static float g_su = 1.0f, g_sv = 1.0f;    /* the drawn texture's uv scale */

static void vertex(const RVertex *v, u32 argb, int eol)
{
    pvr_vertex_t *p = ta_alloc();
    p->flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    p->x = v->x; p->y = v->y; p->z = v->rhw > 0 ? v->rhw : 0.0001f;
    p->u = v->u * g_su; p->v = v->v * g_sv;
    if (g_double) {
        u32 r = (argb >> 16 & 0xFF) * 2, g = (argb >> 8 & 0xFF) * 2, b = (argb & 0xFF) * 2;
        argb = (argb & 0xFF000000) | (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b);
    }
    p->argb = argb;
    p->oargb = (255 - (v->specular >> 24)) << 24;
}

/* a line from a to b as a quad one pixel wide */
static void line(const RVertex *a, const RVertex *b, int flat)
{
    float dx = b->x - a->x, dy = b->y - a->y;
    RVertex q[4] = {*a, *a, *b, *b};
    if (dx * dx > dy * dy) { q[1].y += 1; q[3].y += 1; } else { q[1].x += 1; q[3].x += 1; }
    u32 cb = flat ? a->diffuse : b->diffuse;
    vertex(&q[0], a->diffuse, 0); vertex(&q[1], a->diffuse, 0); vertex(&q[2], cb, 0); vertex(&q[3], cb, 1);
}

static TileV tilev(const RVertex *v, u32 argb)
{
    TileV o;
    float w = v->rhw > 0 ? v->rhw : 0.0001f;
    o.x = v->x; o.y = v->y; o.w = w; o.uw = v->u * w; o.vw = v->v * w;
    for (int i = 0; i < 4; i++) o.c[i] = (float)(argb >> (24 - 8 * i) & 0xFF);
    o.fog = (float)(v->specular >> 24);
    return o;
}

static u32 chan(float f) { return f <= 0 ? 0 : f >= 255 ? 255 : (u32)(f + 0.5f); }

/* triangles over a texture in pieces: each cut to every piece (in texture
   space, perspective-correct) and drawn with that piece's header; the
   pieces' uv made the piece's own */
static void draw_tiled(int prim, const RVertex *v, int n, const RState *s, const Tex *tx, int fl)
{
    int ntri = prim == 4 ? n / 3 : n - 2;
    if (ntri <= 0) return;
    for (int k = 0; k < tx->nx * tx->ny; k++) {
        int i = k % tx->nx, j = k / tx->nx, hdr = 0;
        /* the piece in the texture's 0..1, its outer sides open (uv clamped there) */
        float u0 = i ? (float)tx->xo[i] / tx->w : -1e30f, u1 = i + 1 < tx->nx ? (float)tx->xo[i + 1] / tx->w : 1e30f;
        float v0 = j ? (float)tx->yo[j] / tx->h : -1e30f, v1 = j + 1 < tx->ny ? (float)tx->yo[j + 1] / tx->h : 1e30f;
        float su = (float)tx->w / tx->pw[i], sv = (float)tx->h / tx->ph[j];
        float ou = (float)tx->xo[i] / tx->pw[i], ov = (float)tx->yo[j] / tx->ph[j];
        for (int t = 0; t < ntri; t++) {
            int a, b, c;
            u32 flat;
            if (prim == 4) { a = 3 * t; b = a + 1; c = a + 2; flat = v[a].diffuse; }
            else if (prim == 5) { a = t; b = t + 1; c = t + 2; flat = v[0].diffuse; }
            else { a = 0; b = t + 1; c = t + 2; flat = v[0].diffuse; }
            TileV tri[3] = {tilev(&v[a], fl ? flat : v[a].diffuse), tilev(&v[b], fl ? flat : v[b].diffuse), tilev(&v[c], fl ? flat : v[c].diffuse)};
            TileV out[8], tmp[8];
            int m = tile_clip(tri, u0, u1, v0, v1, out, tmp);
            if (!m) continue;
            if (!hdr) { header(s, tx, k); hdr = 1; }
            for (int q = 1; q + 1 < m; q++) {
                const TileV *p3[3] = {&out[0], &out[q], &out[q + 1]};
                for (int r = 0; r < 3; r++) {
                    const TileV *p = p3[r];
                    RVertex rv;
                    rv.x = p->x; rv.y = p->y; rv.rhw = p->w;
                    rv.u = p->uw / p->w * su - ou; rv.v = p->vw / p->w * sv - ov;
                    rv.specular = chan(p->fog) << 24;
                    u32 argb = chan(p->c[0]) << 24 | chan(p->c[1]) << 16 | chan(p->c[2]) << 8 | chan(p->c[3]);
                    vertex(&rv, argb, r == 2);
                }
            }
        }
    }
}

static void pvr_draw(int prim, const RVertex *v, int n, const RState *s, const RTexture *t)
{
    if (n <= 0) return;
    scene_begin();
    Tex *tx = tex_get(t);
    if (s->fog && s->fog_color != g_fog_rgb) {
        g_fog_rgb = s->fog_color;
        pvr_fog_table_color(1.0f, (g_fog_rgb >> 16 & 0xFF) / 255.0f, (g_fog_rgb >> 8 & 0xFF) / 255.0f, (g_fog_rgb & 0xFF) / 255.0f);
    }
    g_double = s->modulate2x && tx;
    int fl = s->flat;
    if (tx && tx->nx * tx->ny > 1 && prim >= 4) {       /* (lines and points over pieces: the first piece's) */
        g_su = g_sv = 1.0f;
        draw_tiled(prim, v, n, s, tx, fl);
        return;
    }
    header(s, tx, 0);
    g_su = tx ? (float)tx->w / tx->pw[0] : 1.0f; g_sv = tx ? (float)tx->h / tx->ph[0] : 1.0f;
    switch (prim) {
    case 5:                                       /* strip */
        for (int i = 0; i < n; i++) vertex(&v[i], fl ? v[0].diffuse : v[i].diffuse, i == n - 1);
        break;
    case 4:                                       /* list */
        for (int i = 0; i + 2 < n; i += 3)
            for (int k = 0; k < 3; k++) vertex(&v[i + k], fl ? v[i].diffuse : v[i + k].diffuse, k == 2);
        break;
    case 6:                                       /* fan: 0, i, i+1 */
        for (int i = 1; i + 1 < n; i++) {
            vertex(&v[0], v[0].diffuse, 0);
            vertex(&v[i], fl ? v[0].diffuse : v[i].diffuse, 0);
            vertex(&v[i + 1], fl ? v[0].diffuse : v[i + 1].diffuse, 1);
        }
        break;
    case 2: for (int i = 0; i + 1 < n; i += 2) line(&v[i], &v[i + 1], fl); break;
    case 3: for (int i = 0; i + 1 < n; i++) line(&v[i], &v[i + 1], fl); break;
    case 1: for (int i = 0; i < n; i++) line(&v[i], &v[i], 1); break;
    }
}

static void pvr_clear(int target, int zbuffer, u32 rgb)
{
    (void)zbuffer;                                /* (the depth buffer starts each scene cleared) */
    if (target) g_bg = rgb;
}

static void pvr_begin_frame(void) {}

static void pvr_present(void)
{
    scene_begin();                                /* (an empty frame still shows the background colour) */
    ta_flush();
    pvr_list_finish();
    CRUMB('E');
    pvr_scene_finish();
    g_in_scene = 0;
}

static void pvr_read_back(u16 *dst, u32 w, u32 h, u32 pitch)
{
    for (u32 y = 0; y < h; y++) memset((u8 *)dst + y * pitch, 0, w * 2);
}

static void pvr_texture_freed(u32 id)
{
    Tex *e = &g_tex[id % NTEX];
    if (e->id == id && e->nx) tex_release(e);
}

static const Renderer k_pvr = {pvr_begin_frame, pvr_clear, pvr_draw, pvr_present, pvr_read_back, pvr_texture_freed};

const Renderer *RenderPvr_Open(void)
{
    pvr_init_params_t p;
    memset(&p, 0, sizeof p);
    p.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_0;
    p.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    p.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_32;
    p.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    p.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_0;
    p.vertex_buf_size = VERTEX_BUF;
    p.autosort_disabled = 1;                      /* submission order: the order table's */
    p.opb_overflow_count = 8;
    if (pvr_init(&p) < 0) return 0;
    pvr_fog_table_linear(0.0f, 1.0f);             /* (vertex fog takes the offset alpha; the table only for its colour) */
    return &k_pvr;
}
