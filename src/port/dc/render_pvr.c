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
#include <stddef.h>
#include <stdint.h>
#include <dc/pvr.h>
#include <kos/dbglog.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "dc2/render.h"
#include "render_pvr.h"

#define VERTEX_BUF   (512 * 1024)
#define NTEX         1024
#define NPENDING     256

typedef struct { u32 id, version, w, h, pw, ph; float su, sv; pvr_ptr_t mem; u32 used; } Tex;   /* used: the scene it was last drawn in */   /* pw x ph: the power-of-two size in VRAM */
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

static void scene_begin(void)
{
    if (g_in_scene) return;
    pvr_wait_ready();
    g_cur ^= 1;                                  /* the scene before last is done now */
    for (int i = 0; i < g_npending[g_cur]; i++) pvr_mem_free(g_pending[g_cur][i]);
    g_npending[g_cur] = 0;
    g_scene++;
    pvr_set_bg_color((g_bg >> 16 & 0xFF) / 255.0f, (g_bg >> 8 & 0xFF) / 255.0f, (g_bg & 0xFF) / 255.0f);
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
        if (e->mem && e->used + 1 < g_scene && (!old || e->used < old->used)) old = e;
    }
    if (!old) return 0;
    pvr_mem_free(old->mem);
    old->mem = 0; old->id = 0;
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

/* a texture in VRAM, uploaded again on a new version: padded to powers of
   two (the twiddled formats need them; uv scaled by su, sv), 32-bit pixels
   made 1555, through a temporary of the padded size */
static Tex *tex_get(const RTexture *t)
{
    if (!t || (t->bpp != 2 && t->bpp != 4) || !t->w || !t->h || t->w > 1024 || t->h > 1024) return 0;
    Tex *e = &g_tex[t->id % NTEX];
    if (e->id != t->id || !e->mem || e->version != t->version || e->w != t->w || e->h != t->h) {
        u32 pw = pow2(t->w), ph = pow2(t->h);
        defer_free(e->mem);
        e->mem = 0;
        e->mem = vram_alloc(pw * ph * 2);
        if (!e->mem) { e->id = 0; return 0; }
        e->id = t->id; e->version = t->version; e->w = t->w; e->h = t->h; e->pw = pw; e->ph = ph;
        e->su = (float)t->w / (float)pw; e->sv = (float)t->h / (float)ph;
        if (t->bpp == 2 && pw == t->w && ph == t->h && t->pitch == t->w * 2) {
            pvr_txr_load_ex((void *)t->pixels, e->mem, pw, ph, PVR_TXRLOAD_16BPP);
        } else {
            u16 *tmp = memalign(32, pw * ph * 2);
            if (!tmp) { pvr_mem_free(e->mem); e->mem = 0; e->id = 0; return 0; }
            memset(tmp, 0, pw * ph * 2);
            for (u32 y = 0; y < t->h; y++) {
                const u8 *src = (const u8 *)t->pixels + y * t->pitch;
                u16 *row = tmp + y * pw;
                if (t->bpp == 2) memcpy(row, src, t->w * 2);
                else for (u32 x = 0; x < t->w; x++) row[x] = to1555(src + 4 * x);
                for (u32 x = t->w; x < pw; x++) row[x] = row[t->w - 1];     /* edges repeated (filtering) */
            }
            for (u32 y = t->h; y < ph; y++) memcpy(tmp + y * pw, tmp + (t->h - 1) * pw, pw * 2);
            pvr_txr_load_ex(tmp, e->mem, pw, ph, PVR_TXRLOAD_16BPP);
            free(tmp);
        }
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

static void header(const RState *s, Tex *t)
{
    pvr_poly_cxt_t c;
    if (t) {
        pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_TWIDDLED, (int)t->pw, (int)t->ph, t->mem,
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

static void pvr_draw(int prim, const RVertex *v, int n, const RState *s, const RTexture *t)
{
    if (n <= 0) return;
    scene_begin();
    Tex *tx = tex_get(t);
    header(s, tx);
    if (s->fog && s->fog_color != g_fog_rgb) {
        g_fog_rgb = s->fog_color;
        pvr_fog_table_color(1.0f, (g_fog_rgb >> 16 & 0xFF) / 255.0f, (g_fog_rgb >> 8 & 0xFF) / 255.0f, (g_fog_rgb & 0xFF) / 255.0f);
    }
    g_double = s->modulate2x && tx;
    g_su = tx ? tx->su : 1.0f; g_sv = tx ? tx->sv : 1.0f;
    int fl = s->flat;
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
    if (e->id == id && e->mem) { defer_free(e->mem); e->mem = 0; e->id = 0; }
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
