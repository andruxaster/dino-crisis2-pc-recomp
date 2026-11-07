/*
 * dc2view -- minimal room viewer for Dino Crisis 2 PC data (2002 release).
 *
 * Loads a room (STxyy.DAT/.DBS) and shows one camera: JPEG background,
 * masks with their OT depth, an animated skeletal model (WEP_P*.DAT etc.)
 * an optional floor grid and the room collision (walls, walkable areas,
 * camera-switch zones); the floor height comes from the collision levels
 * (level = -y/1000). Formats follow
 * the dc2 tools (dc2dat/dc2room/dc2model/dc2anim). Projection is done on the
 * CPU like the GTE: sx = 160 + H*x/z, sy = 120 + H*y/z, screen 320x240.
 *
 * build:  cc -O2 -o dc2view dc2view.c $(sdl2-config --cflags --libs) -lGL -lm
 *         Windows (mingw-w64): see Makefile target win (opengl32, SDL2.dll)
 * JPEG decoding: stb_image.h (public domain / MIT, bundled)
 * run:    dc2view DATA_DIR ST100 [--cam N] [--model WEP_P002] [--anim N] [--vram FILE]
 *                 [--grid] [--no-model] [--no-masks] [--no-bg] [--debug]
 * WEP_P000/WEP_P100 carry no body texture: add --vram WEP_P002 (or WEP_P101).
 *                 [--pos X Z] [--yaw A] [--floor Y] [--collision] [--no-follow] [--door N]
 *                 [--screenshot out.ppm] [--frames N]
 * keys:   Left/Right camera (model follows the camera target until moved),
 *         C put model under the camera target, Up/Down animation, Q/E turn,
 *         WASD move model (stops at walls, switches camera by zone as the game does),
 *         N dinosaurs placed by the room script on/off (obj_spawn positions and
 *         the spawn points of the random spawner), Enter go through a door (doors come from the room script),
 *         F walls/zones on/off, K show collision (doors magenta), PgUp/PgDn floor height,
 *         G grid, M masks, B background,
 *         O model, Space pause, Esc quit
 */
#include <SDL.h>
#include <SDL_opengl.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32;

#define SECTOR 0x800
static const char *g_dir;

/* ------------------------------------------------------------------ files */
typedef struct { u8 *p; u32 n; } Buf;

static Buf read_file(const char *name)
{
    Buf b = {0, 0};
    char path[1024], alt[256];
    for (int pass = 0; pass < 3 && !b.p; pass++) {
        size_t i;
        for (i = 0; name[i] && i < sizeof alt - 1; i++)
            alt[i] = pass == 1 ? toupper((u8)name[i]) : pass == 2 ? tolower((u8)name[i]) : name[i];
        alt[i] = 0;
        snprintf(path, sizeof path, "%s/%s", g_dir, alt);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END); b.n = (u32)ftell(f); fseek(f, 0, SEEK_SET);
        b.p = malloc(b.n ? b.n : 1);
        if (fread(b.p, 1, b.n, f) != b.n) b.n = 0;
        fclose(f);
    }
    return b;
}

static u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static s16 rds16(const u8 *p) { return (s16)rd16(p); }
static u32 rd32(const u8 *p) { return (u32)p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }

/* LZSS (Unpack 0x4041F0): flag bits LSB first, 1 = literal, 0 = 2-byte match
 * offset = b0 | (b1 & 15) << 8, length = (b1 >> 4) + 2 */
static Buf lzss(const u8 *s, u32 n)
{
    Buf o; u32 cap = n * 4 + 1024, i = 0;
    o.p = malloc(cap); o.n = 0;
    while (i < n) {
        u8 fl = s[i++];
        for (int b = 0; b < 8 && i < n; b++, fl >>= 1) {
            if (o.n + 18 > cap) { cap *= 2; o.p = realloc(o.p, cap); }
            if (fl & 1) { o.p[o.n++] = s[i++]; continue; }
            if (i + 1 >= n) { i = n; break; }
            u32 off = s[i] | (s[i + 1] & 15) << 8, len = (s[i + 1] >> 4) + 2;
            i += 2;
            for (u32 k = 0; k < len; k++, o.n++)
                o.p[o.n] = (off && off <= o.n) ? o.p[o.n - off] : 0;
        }
    }
    return o;
}

/* container: 0x800 header of 32-byte records, payloads sector-aligned */
typedef struct { u32 type, size, arg; u16 x, y, w, h; const u8 *data; } Rec;

static int parse_container(const u8 *d, u32 n, u32 base, Rec *out, int max)
{
    int cnt = 0; u32 off = base + SECTOR;
    if (base + SECTOR > n) return 0;
    for (int i = 0; i < SECTOR / 32 && cnt < max; i++) {
        const u8 *r = d + base + i * 32;
        int zero = 1;
        for (int k = 0; k < 32; k++) if (r[k]) { zero = 0; break; }
        if (zero || !memcmp(r, "dummy", 5)) continue;
        Rec *o = &out[cnt++];
        o->type = rd32(r); o->size = rd32(r + 4); o->arg = rd32(r + 8);
        o->x = rd16(r + 8); o->y = rd16(r + 10); o->w = rd16(r + 12); o->h = rd16(r + 14);
        o->data = off + o->size <= n ? d + off : NULL;
        off += (o->size + SECTOR - 1) & ~(SECTOR - 1);
    }
    return cnt;
}

/* ------------------------------------------------------------------- VRAM */
static u16 g_vram[512][1024];

static void vram_put(int x, int y, int w, int h, const u8 *src, u32 n)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            u32 k = (u32)(j * w + i) * 2;
            if (k + 1 >= n) return;
            if (y + j < 512 && x + i < 1024) g_vram[y + j][x + i] = rd16(src + k);
        }
}

static void load_vram_records(Rec *r, int n)
{
    for (int i = 0; i < n; i++) {
        if (!r[i].data) continue;
        if (r[i].type == 1 || r[i].type == 2) vram_put(r[i].x, r[i].y, r[i].w, r[i].h, r[i].data, r[i].size);
        else if (r[i].type == 6 || r[i].type == 8) {
            Buf u = lzss(r[i].data, r[i].size);
            if (r[i].type == 8) vram_put(r[i].x, r[i].y, r[i].w, r[i].h, u.p, u.n);
            else {
                int tx = (r[i].w + 31) / 32;
                for (u32 k = 0; k * SECTOR < u.n; k++)
                    vram_put(r[i].x + (k % tx) * 32, r[i].y + (k / tx) * 32, 32, 32,
                             u.p + k * SECTOR, u.n - k * SECTOR);
            }
            free(u.p);
        }
    }
}

static void bgr555(u16 v, u8 *rgba)
{
    rgba[0] = (u8)((v & 31) << 3); rgba[1] = (u8)(((v >> 5) & 31) << 3);
    rgba[2] = (u8)(((v >> 10) & 31) << 3); rgba[3] = v ? 255 : 0;  /* 0x0000 = transparent */
}

/* 256x256 texture from a texture page + CLUT (tpage/clut words as in the game) */
static GLuint texture_from_vram(u16 tpage, u16 clut)
{
    static u8 px[256 * 256 * 4];
    int px0 = (tpage & 15) * 64, py0 = (tpage & 16) ? 256 : 0, mode = (tpage >> 7) & 3;
    int cx = (clut & 63) * 16, cy = (clut >> 6) & 511;
    for (int v = 0; v < 256; v++)
        for (int u = 0; u < 256; u++) {
            u16 c;
            int y = py0 + v;
            if (y > 511) y = 511;
            if (mode == 0) { u16 w = g_vram[y][(px0 + u / 4) & 1023]; c = g_vram[cy][(cx + ((w >> ((u & 3) * 4)) & 15)) & 1023]; }
            else if (mode == 1) { u16 w = g_vram[y][(px0 + u / 2) & 1023]; c = g_vram[cy][(cx + ((w >> ((u & 1) * 8)) & 255)) & 1023]; }
            else c = g_vram[y][(px0 + u) & 1023];
            bgr555(c, &px[(v * 256 + u) * 4]);
        }
    GLuint t;
    glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    return t;
}

/* ------------------------------------------------------------------- room */
typedef struct { s16 eye[3]; u16 h; s16 target[3]; s16 roll; u32 dbs_off, dbs_size; } Camera;
typedef struct { u8 x, y; s16 z; } Mask;

static Camera g_cams[64]; static int g_ncam;
static Buf g_dbs;
static GLuint g_bg_tex, g_mask_tex;
static Mask g_masks[512]; static int g_nmask;

/* collision (G+0xA4C) and camera-switch zones (G+0xA44), see dc2_game_flow.h */
typedef struct { u8 flag, n, idx, ign; u16 levels; u32 edge[64]; s16 pt[65][2]; } CPoly;
typedef struct { u8 flag, cam, lvl, range; s16 q[4][2]; } CZone;
static CPoly g_poly[64]; static int g_npoly;
static CZone g_zone[128]; static int g_nzone;

/* doors = aot_door (opcode 0x30) calls found in the room script (G+0xA54) */
typedef struct { u8 level; s16 q[4][2]; s16 dest[3], dir; u16 room; u8 key_flag; } Door;
static Door g_door[32]; static int g_ndoor;

/* dinosaurs the script can place: obj_spawn (0x1A) and enemy_set spawn points (0x4F) */
typedef struct { u8 type, variant, from_set; s16 pos[3], dir; } Spawn;
static Spawn g_spawn[64]; static int g_nspawn;

static void add_spawn(int type, int variant, int from_set, const s16 *pos, s16 dir)
{
    if (type < 2 || type > 14 || g_nspawn >= 64) return;
    for (int i = 0; i < g_nspawn; i++)
        if (g_spawn[i].type == type && !memcmp(g_spawn[i].pos, pos, 6)) return;
    Spawn *e = &g_spawn[g_nspawn++];
    e->type = (u8)type; e->variant = (u8)variant; e->from_set = (u8)from_set;
    memcpy(e->pos, pos, 6); e->dir = dir;
}

/* ENEMY SET: header 16 bytes, zones of 32 bytes, 16-byte spawn points (dc2objects.py) */
static void scan_enemy_set(const u8 *blk, u32 n, u32 off)
{
    if (off + 16 > n) return;
    const u8 *h = blk + off;
    for (int z = 0; z < h[0]; z++) {
        const u8 *zn = h + 16 + 32 * z;
        if (zn + 32 > blk + n) return;
        u32 po = off + rd16(zn + 16);
        for (int i = 0; i < zn[18] && po + 16 * i + 8 <= n; i++) {
            const u8 *p = blk + po + 16 * i;
            s16 pos[3] = {rds16(p), rds16(p + 2), rds16(p + 4)};
            add_spawn(h[1], h[4], 1, pos, rds16(p + 6));
        }
    }
}

#define BLK_BASE 0x5E0000u
static u32 end_of_block;

static void scan_doors(const u8 *blk, u32 a54, u32 end)
{
    static const u8 len[8] = {2, 6, 6, 6, 2, 4, 4, 4};
    u32 base = a54 + 0x1C, gtab = base + rd32(blk + a54 + 12);
    s32 st[64]; int sp = 0;
    g_ndoor = 0; g_nspawn = 0;
    for (u32 pc = base + rd32(blk + base); pc + 2 <= end && pc < gtab; ) {
        u8 op = blk[pc];
        if (op > 0x5F) break;                       /* reached the data area */
        u32 n = op < 8 ? len[op] : op == 8 ? 4 : 2;
        if (op == 5) {
            u8 t = blk[pc + 1]; s16 v = rds16(blk + pc + 2); s32 x = v;
            if (t == 6) { x = (s32)rd32(blk + gtab + 4 * v); if (x >= 0x8000 && x < 0x10000) x -= 0x10000; }
            else if (t != 0 && t != 4) x = 0x7FFFFFFF;
            if (sp < 64) st[sp++] = x;
        } else if (op == 3 && blk[pc + 2] == 0xFF && blk[pc + 1] == 0) {
            int k = rds16(blk + pc + 4); sp = sp > k ? sp - k : 0;
        } else if (op == 7 || op == 4) sp = 0;
        else if (op == 0x1A && sp >= 10) {
            s32 *a = st + sp - 10;
            if (a[1] != 0x7FFFFFFF && a[2] != 0x7FFFFFFF && a[3] != 0x7FFFFFFF && a[4] != 0x7FFFFFFF) {
                s16 pos[3] = {(s16)a[2], (s16)a[3], (s16)a[4]};
                add_spawn(a[1], a[7] == 0x7FFFFFFF ? 0 : a[7] & 15, 0, pos, (s16)a[5]);
            }
        } else if (op == 0x4F && sp >= 1 && st[sp - 1] != 0x7FFFFFFF && (u32)st[sp - 1] >= BLK_BASE) {
            scan_enemy_set(blk, end_of_block, (u32)st[sp - 1] - BLK_BASE);
        } else if (op == 0x30 && sp >= 19 && g_ndoor < 32) {
            s32 *a = st + sp - 19; int ok = 1;
            for (int i = 2; i < 17; i++) if (a[i] == 0x7FFFFFFF) ok = 0;
            if (ok) {
                Door *d = &g_door[g_ndoor++];
                d->level = (u8)a[2];
                for (int i = 0; i < 4; i++) { d->q[i][0] = (s16)a[3 + 2 * i]; d->q[i][1] = (s16)a[4 + 2 * i]; }
                for (int i = 0; i < 3; i++) d->dest[i] = (s16)a[11 + i];
                d->dir = (s16)a[14]; d->room = (u16)a[15]; d->key_flag = (u8)a[18];
            }
        }
        pc += n;
    }
}

static int load_room(const char *room)
{
    char name[64];
    snprintf(name, sizeof name, "%s.DAT", room);
    Buf dat = read_file(name);
    if (!dat.p) { fprintf(stderr, "cannot read %s\n", name); return 0; }
    Rec r[64];
    int n = parse_container(dat.p, dat.n, 0, r, 64);
    load_vram_records(r, n);
    for (int i = 0; i < n; i++) {
        if (r[i].type != 5 || r[i].arg != 0x5E0000 || !r[i].data) continue;
        Buf b = lzss(r[i].data, r[i].size);
        u32 base = 0x5E0000, cams = rd32(b.p + 0x24) - base, dbs = rd32(b.p + 0x28) - base;
        g_ncam = rd16(b.p + cams + 4);
        if (g_ncam > 64) g_ncam = 64;
        for (int c = 0; c < g_ncam; c++) {
            const u8 *e = b.p + cams + 0x10 + c * 16;
            Camera *k = &g_cams[c];
            for (int a = 0; a < 3; a++) { k->eye[a] = rds16(e + a * 2); k->target[a] = rds16(e + 8 + a * 2); }
            k->h = rd16(e + 6); k->roll = rds16(e + 14);
            k->dbs_off = rd32(b.p + dbs + c * 8) * SECTOR; k->dbs_size = rd32(b.p + dbs + c * 8 + 4);
        }
        u32 col = rd32(b.p + 0x08) - base, zs = rd32(b.p + 0x0C) - base;
        g_npoly = b.p[col]; if (g_npoly > 64) g_npoly = 64;
        for (int k = 0; k < g_npoly; k++) {
            const u8 *q = b.p + col + rd32(b.p + col + 8 + 4 * k);
            CPoly *c = &g_poly[k];
            c->flag = q[0]; c->n = q[1] > 64 ? 64 : q[1]; c->idx = q[2]; c->ign = q[3]; c->levels = rd16(q + 4);
            for (int e = 0; e < c->n; e++) c->edge[e] = rd32(q + 8 + 4 * e);
            for (int e = 0; e <= c->n; e++) { c->pt[e][0] = rds16(q + 8 + 4 * q[1] + 4 * e); c->pt[e][1] = rds16(q + 10 + 4 * q[1] + 4 * e); }
        }
        u32 a54 = rd32(b.p + 0x14) - base, sec_end = b.n;
        for (int k = 0; k < 12; k++) { u32 q = rd32(b.p + 4 * k) - base; if (q > a54 && q < sec_end) sec_end = q; }
        end_of_block = b.n;
        scan_doors(b.p, a54, sec_end);
        g_nzone = (int)rd32(b.p + zs); if (g_nzone > 128) g_nzone = 128;
        for (int k = 0; k < g_nzone; k++) {
            const u8 *q = b.p + zs + 4 + 0x2C * k;
            CZone *z = &g_zone[k];
            z->flag = q[0]; z->cam = q[4]; z->lvl = q[6]; z->range = q[7];
            for (int e = 0; e < 4; e++) { z->q[e][0] = rds16(q + 12 + 4 * e); z->q[e][1] = rds16(q + 14 + 4 * e); }
        }
        free(b.p);
    }
    free(dat.p);
    snprintf(name, sizeof name, "%s.DBS", room);
    free(g_dbs.p);
    g_dbs = read_file(name);
    printf("room %s: %d cameras, %d collision polygons, %d camera zones, %d doors, DBS %u bytes\n",
           room, g_ncam, g_npoly, g_nzone, g_ndoor, g_dbs.n);
    for (int k = 0; k < g_nspawn; k++)
        printf("  dinosaur type %d at (%d, %d, %d)%s\n", g_spawn[k].type, g_spawn[k].pos[0], g_spawn[k].pos[1],
               g_spawn[k].pos[2], g_spawn[k].from_set ? " (random spawner point)" : "");
    for (int k = 0; k < g_ndoor; k++)
        printf("  door %d (level %d) -> ST%X%02X at (%d, %d, %d) dir %d%s\n", k, g_door[k].level,
               g_door[k].room >> 8, g_door[k].room & 0xFF, g_door[k].dest[0], g_door[k].dest[1],
               g_door[k].dest[2], g_door[k].dir, g_door[k].key_flag ? " (locked until key flag)" : "");
    return g_ncam > 0 && g_dbs.p;
}

static void load_camera(int c)
{
    Rec r[16];
    int n = parse_container(g_dbs.p, g_dbs.n, g_cams[c].dbs_off, r, 16);
    const u8 *maskrec = NULL; Buf pix = {0, 0};
    g_nmask = 0;
    for (int i = 0; i < n; i++) {
        if (!r[i].data) continue;
        if (r[i].type == 0 && r[i].arg == 0x6B9500) maskrec = r[i].data;
        else if (r[i].type == 0 && r[i].arg == 0x6C1500) {
            int w = 0, h = 0, comp = 0;
            u8 *img = stbi_load_from_memory(r[i].data, (int)r[i].size, &w, &h, &comp, 3);
            u8 *rgb = calloc(512 * 256 * 3, 1);
            if (img) {
                for (int y = 0; y < h && y < 256; y++)
                    memcpy(rgb + y * 512 * 3, img + (size_t)y * w * 3, (size_t)(w < 512 ? w : 512) * 3);
                stbi_image_free(img);
            } else fprintf(stderr, "JPEG decode failed: %s\n", stbi_failure_reason());
            if (!g_bg_tex) glGenTextures(1, &g_bg_tex);
            glBindTexture(GL_TEXTURE_2D, g_bg_tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
            free(rgb);
            if (w != 320 || h != 240) fprintf(stderr, "unexpected JPEG size %dx%d\n", w, h);
        } else if (i == 3 && r[i].type == 0 && r[i].arg > r[i].size) pix = lzss(r[i].data, r[i].size);
        else if (r[i].type == 2 || r[i].type == 8) load_vram_records(&r[i], 1);
    }
    if (maskrec) {
        int cnt = rd16(maskrec);
        for (int k = 0; k < cnt && k < 512; k++) {
            g_masks[k].x = maskrec[4 + k * 4]; g_masks[k].y = maskrec[5 + k * 4] & 15;
            g_masks[k].z = rds16(maskrec + 6 + k * 4);
        }
        g_nmask = cnt < 512 ? cnt : 512;
    }
    if (g_nmask && pix.n >= (u32)g_nmask * 648) {
        /* atlas of 18x18 tiles, 56 per row */
        u8 *at = calloc(1024 * 256 * 4, 1);
        for (int k = 0; k < g_nmask; k++) {
            int ax = (k % 56) * 18, ay = (k / 56) * 18;
            for (int j = 0; j < 18; j++)
                for (int i = 0; i < 18; i++)
                    bgr555(rd16(pix.p + k * 648 + (j * 18 + i) * 2), &at[((ay + j) * 1024 + ax + i) * 4]);
        }
        if (!g_mask_tex) glGenTextures(1, &g_mask_tex);
        glBindTexture(GL_TEXTURE_2D, g_mask_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, at);
        free(at);
    } else g_nmask = 0;
    free(pix.p);
    printf("camera %d: eye (%d,%d,%d) target (%d,%d,%d) H %d roll %d, %d masks\n", c,
           g_cams[c].eye[0], g_cams[c].eye[1], g_cams[c].eye[2], g_cams[c].target[0],
           g_cams[c].target[1], g_cams[c].target[2], g_cams[c].h, g_cams[c].roll, g_nmask);
}

/* ------------------------------------------------------------------ model */
typedef struct { s16 joint[3]; s8 parent; u32 tris, quads; u16 ntri, nquad; } Part;
typedef struct { const u8 *p; u16 frames; } Seq;

typedef struct {
    char name[16], tex_name[16]; Buf blk; u32 base, verts; int nparts; Part parts[64];
    u16 tpage, clut; GLuint tex; Seq seqs[256]; int nseq;
} Model;

/* an object placed in the room: the hero (actor 0) or a dinosaur from the script */
typedef struct { Model *m; float pos[3], yaw, time; int seq; int random_alt; } Actor;

static Model g_models[16]; static int g_nmodels;
static Model *M;                 /* model being loaded / drawn */

static void upload_file(const char *name)
{
    Buf b = read_file(name); Rec r[64];
    if (b.p) { load_vram_records(r, parse_container(b.p, b.n, 0, r, 64)); free(b.p); }
    else fprintf(stderr, "cannot read %s\n", name);
}

/* like Enemy_LoadModel: the .DAT (with its own VRAM records), then the texture file */
static int load_model_into(Model *m, const char *file, const char *tex)
{
    M = m; memset(m, 0, sizeof *m); snprintf(m->name, sizeof m->name, "%s", file);
    if (tex) snprintf(m->tex_name, sizeof m->tex_name, "%s", tex);
    char name[64];
    snprintf(name, sizeof name, "%s.DAT", file);
    Buf d = read_file(name);
    if (!d.p) { fprintf(stderr, "cannot read %s\n", name); return 0; }
    Rec r[64];
    int n = parse_container(d.p, d.n, 0, r, 64);
    load_vram_records(r, n);
    if (tex) upload_file(tex);
    for (int i = 0; i < n && !M->blk.p; i++) {
        if (r[i].type != 5 || !r[i].data) continue;
        Buf b = lzss(r[i].data, r[i].size);
        u32 base = r[i].arg, vp = rd32(b.p), np = rd32(b.p + 4), tp = rd32(b.p + 8), qp = rd32(b.p + 12);
        u32 parts = rd32(b.p + 0x14);
        if (vp < base || vp >= base + b.n || np <= vp || tp <= np || qp < tp || parts == 0 || parts > 64) { free(b.p); continue; }
        M->blk = b; M->base = base; M->verts = vp - base; M->nparts = (int)parts;
        M->tpage = rd16(b.p + 0x18); M->clut = rd16(b.p + 0x1A);
        for (int k = 0; k < M->nparts; k++) {
            const u8 *e = b.p + 0x1C + k * 20;
            Part *P = &M->parts[k];
            for (int a = 0; a < 3; a++) P->joint[a] = rds16(e + a * 2);
            P->parent = (s8)e[6];
            P->tris = rd32(e + 8) - base; P->quads = rd32(e + 12) - base;
            P->ntri = rd16(e + 16); P->nquad = rd16(e + 18);
        }
        /* animation table after the quad list (0xCD padding skipped) */
        u32 o = qp - base + 16u * rd16(b.p + 0x12);
        while (o < b.n && b.p[o] == 0xCD) o++;
        o = (o + 3) & ~3u;
        u32 lim = b.n, stride = (16 + 6 * parts + 3) & ~3u;
        for (; o + 4 <= lim && M->nseq < 256; o += 4) {
            u32 v = rd32(b.p + o);
            if (!v) continue;
            if (v < base || v >= base + b.n) break;
            u32 a = v - base;
            if (a < lim) lim = a;
            u16 nf = rd16(b.p + a);
            if (nf && a + 0x14 + nf * stride <= b.n) { M->seqs[M->nseq].p = b.p + a; M->seqs[M->nseq].frames = nf; M->nseq++; }
        }
    }
    free(d.p);
    if (!M->blk.p) { fprintf(stderr, "%s: no model block\n", file); return 0; }
    M->tex = texture_from_vram(M->tpage, M->clut);
    printf("model %s: %d parts, tpage %#x clut %#x, %d sequences\n", file, M->nparts, M->tpage, M->clut, M->nseq);
    return 1;
}

/* load (once) a model, after uploading an optional texture file to VRAM */
static Model *get_model(const char *file, const char *tex)
{
    for (int i = 0; i < g_nmodels; i++)
        if (!strcmp(g_models[i].name, file) && !strcmp(g_models[i].tex_name, tex ? tex : "")) return &g_models[i];
    if (g_nmodels >= 16) return NULL;
    if (!load_model_into(&g_models[g_nmodels], file, tex)) return NULL;
    return &g_models[g_nmodels++];
}

/* ------------------------------------------------------------------- math */
typedef struct { float m[3][3]; float t[3]; } Xf;

static void rot_yxz(const float a[3], float m[3][3])  /* RotMatrixYXZ: M = Ry * Rx * Rz */
{
    float k = 6.2831853f / 4096.f;
    float cx = cosf(a[0] * k), sx = sinf(a[0] * k), cy = cosf(a[1] * k), sy = sinf(a[1] * k);
    float cz = cosf(a[2] * k), sz = sinf(a[2] * k);
    float rx[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};
    float ry[3][3] = {{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}};
    float rz[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
    float t[3][3];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        t[i][j] = 0; for (int k2 = 0; k2 < 3; k2++) t[i][j] += rx[i][k2] * rz[k2][j];
    }
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        m[i][j] = 0; for (int k2 = 0; k2 < 3; k2++) m[i][j] += ry[i][k2] * t[k2][j];
    }
}

static void xf_mul(const Xf *a, const Xf *b, Xf *o)  /* o = a * b */
{
    Xf r;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            r.m[i][j] = 0; for (int k = 0; k < 3; k++) r.m[i][j] += a->m[i][k] * b->m[k][j];
        }
        r.t[i] = a->t[i];
        for (int k = 0; k < 3; k++) r.t[i] += a->m[i][k] * b->t[k];
    }
    *o = r;
}

static void xf_apply(const Xf *a, const float v[3], float o[3])
{
    for (int i = 0; i < 3; i++) o[i] = a->m[i][0] * v[0] + a->m[i][1] * v[1] + a->m[i][2] * v[2] + a->t[i];
}

/* camera: z forward, x right, y down (world y is down as on the PS1) */
static Xf g_view; static float g_H;

static void setup_view(const Camera *c)
{
    float e[3] = {c->eye[0], c->eye[1], c->eye[2]};
    float f[3] = {c->target[0] - e[0], c->target[1] - e[1], c->target[2] - e[2]};
    float l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    for (int i = 0; i < 3; i++) f[i] /= l;
    float dn[3] = {0, 1, 0};
    float x[3] = {dn[1] * f[2] - dn[2] * f[1], dn[2] * f[0] - dn[0] * f[2], dn[0] * f[1] - dn[1] * f[0]};
    l = sqrtf(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    for (int i = 0; i < 3; i++) x[i] /= l;
    float y[3] = {f[1] * x[2] - f[2] * x[1], f[2] * x[0] - f[0] * x[2], f[0] * x[1] - f[1] * x[0]};
    float r = c->roll * 6.2831853f / 4096.f, cr = cosf(r), sr = sinf(r);
    for (int i = 0; i < 3; i++) {
        float xi = x[i] * cr + y[i] * sr, yi = -x[i] * sr + y[i] * cr;
        g_view.m[0][i] = xi; g_view.m[1][i] = yi; g_view.m[2][i] = f[i];
    }
    for (int i = 0; i < 3; i++) g_view.t[i] = -(g_view.m[i][0] * e[0] + g_view.m[i][1] * e[1] + g_view.m[i][2] * e[2]);
    g_H = c->h;
}

#define ZMAX 16384.f
static int project(const float w[3], float s[3])  /* -> screen x, y, depth 0..1 */
{
    float c[3];
    xf_apply(&g_view, w, c);
    if (c[2] < 32.f) return 0;
    s[0] = 160.f + g_H * c[0] / c[2];
    s[1] = 120.f + g_H * c[1] / c[2];
    s[2] = c[2] / ZMAX; if (s[2] > 1.f) s[2] = 1.f;
    return 1;
}

/* ------------------------------------------------------------------ drawing */
static void draw_background(void)
{
    glDisable(GL_DEPTH_TEST); glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_bg_tex);
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(0, 0, -0.999f);
    glTexCoord2f(320.f / 512, 0); glVertex3f(320, 0, -0.999f);
    glTexCoord2f(320.f / 512, 240.f / 256); glVertex3f(320, 240, -0.999f);
    glTexCoord2f(0, 240.f / 256); glVertex3f(0, 240, -0.999f);
    glEnd();
}

static void draw_masks(void)
{
    if (!g_nmask) return;
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.5f);
    glBindTexture(GL_TEXTURE_2D, g_mask_tex);
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    for (int k = 0; k < g_nmask; k++) {
        float x = g_masks[k].x * 16.f, y = g_masks[k].y * 16.f;
        float d = -(g_masks[k].z * 16.f) / ZMAX;          /* OT slot -> camera z */
        float u = ((k % 56) * 18 + 1) / 1024.f, v = ((k / 56) * 18 + 1) / 256.f;
        float du = 16 / 1024.f, dv = 16 / 256.f;
        glTexCoord2f(u, v); glVertex3f(x, y, d);
        glTexCoord2f(u + du, v); glVertex3f(x + 16, y, d);
        glTexCoord2f(u + du, v + dv); glVertex3f(x + 16, y + 16, d);
        glTexCoord2f(u, v + dv); glVertex3f(x, y + 16, d);
    }
    glEnd();
    glDisable(GL_ALPHA_TEST);
}

static float g_floor;

static void draw_grid(void)
{
    glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST);
    glBegin(GL_LINES);
    for (int axis = 0; axis < 2; axis++)
        for (int k = -80; k <= 80; k++) {
            int major = k % 4 == 0;
            glColor3f(major ? 1.f : 0.f, major ? 1.f : 0.8f, major ? 0.f : 1.f);
            for (int t = -80; t < 80; t++) {
                float a[3], b[3], sa[3], sb[3];
                a[1] = b[1] = g_floor;
                if (axis == 0) { a[0] = b[0] = k * 500.f; a[2] = t * 500.f; b[2] = (t + 1) * 500.f; }
                else { a[2] = b[2] = k * 500.f; a[0] = t * 500.f; b[0] = (t + 1) * 500.f; }
                if (project(a, sa) && project(b, sb)) { glVertex3f(sa[0], sa[1], 0); glVertex3f(sb[0], sb[1], 0); }
            }
        }
    glEnd();
}

/* level = trunc(-y / 1000) as in the game (obj+0x5A); polygons apply to the levels in their mask */
static int level_of(float y) { return (int)(-y / 1000.f); }

static int inside(const s16 (*p)[2], int n, float x, float z)
{
    int c = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((p[i][1] > z) != (p[j][1] > z) &&
            x < p[j][0] + (z - p[j][1]) * (float)(p[i][0] - p[j][0]) / (float)(p[i][1] - p[j][1])) c = !c;
    return c;
}

/* floor level for a point: lowest level of a walkable (flag 0x40) polygon containing it */
static int floor_level_at(float x, float z, int prefer)
{
    int best = -1;
    for (int k = 0; k < g_npoly; k++) {
        CPoly *c = &g_poly[k];
        if (!c->flag || !(c->flag & 0x40) || !c->levels || !inside((const s16 (*)[2])c->pt, c->n, x, z)) continue;
        if (prefer >= 0 && (c->levels >> prefer & 1)) return prefer;
        int lo = 0; while (!(c->levels >> lo & 1)) lo++;
        if (best < 0 || lo < best) best = lo;
    }
    return best;
}

/* camera zone for a position, like 0x4035F0 */
static int zone_camera(float x, float z, int lvl)
{
    for (int k = 0; k < g_nzone; k++) {
        CZone *q = &g_zone[k];
        if (q->flag & 0x80 || lvl < q->lvl || lvl > q->lvl + q->range) continue;
        if (inside((const s16 (*)[2])q->q, 4, x, z) && q->cam < g_ncam) return q->cam;
    }
    return -1;
}

/* level for a camera: level of its first zone (or of the floor under its target) */
static int camera_level(int cam)
{
    for (int k = 0; k < g_nzone; k++) if (g_zone[k].cam == cam && !(g_zone[k].flag & 0x80)) return g_zone[k].lvl;
    int l = floor_level_at(g_cams[cam].target[0], g_cams[cam].target[2], -1);
    return l < 0 ? 0 : l;
}

static int seg_cross(float ax, float az, float bx, float bz, float cx, float cz, float dx, float dz)
{
    float d1 = (bx - ax) * (cz - az) - (bz - az) * (cx - ax), d2 = (bx - ax) * (dz - az) - (bz - az) * (dx - ax);
    float d3 = (dx - cx) * (az - cz) - (dz - cz) * (ax - cx), d4 = (dx - cx) * (bz - cz) - (dz - cz) * (bx - cx);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

/* wall test of 0x486FB0 for the player (kind 0x80): edges of active polygons at this level */
static int blocked(float ax, float az, float bx, float bz, int lvl)
{
    const u8 kind = 0x80;
    for (int k = 0; k < g_npoly; k++) {
        CPoly *c = &g_poly[k];
        if (!c->flag || !(c->levels >> lvl & 1) || (c->ign & kind)) continue;
        for (int e = 0; e < c->n; e++) {
            u8 t = c->edge[e] & 0xFF, ig = c->edge[e] >> 8 & 0xFF;
            if (!t || (ig & kind) || (t & 2)) continue;
            if (seg_cross(ax, az, bx, bz, c->pt[e][0], c->pt[e][1], c->pt[e + 1][0], c->pt[e + 1][1])) return 1;
        }
    }
    return 0;
}

static int door_at(float x, float z, int lvl)
{
    for (int k = 0; k < g_ndoor; k++)
        if (g_door[k].level == lvl && inside((const s16 (*)[2])g_door[k].q, 4, x, z)) return k;
    return -1;
}

static void line3(float y, float x0, float z0, float x1, float z1)
{
    const int N = 16;   /* split so that perspective stays right near the camera */
    for (int i = 0; i < N; i++) {
        float a[3] = {x0 + (x1 - x0) * i / N, y, z0 + (z1 - z0) * i / N};
        float b[3] = {x0 + (x1 - x0) * (i + 1) / N, y, z0 + (z1 - z0) * (i + 1) / N};
        float sa[3], sb[3];
        if (project(a, sa) && project(b, sb)) { glVertex3f(sa[0], sa[1], 0); glVertex3f(sb[0], sb[1], 0); }
    }
}

/* K: walls red, passable edges green, outline of walkable areas yellow, zone of this camera cyan */
static void draw_collision(int cam, int lvl)
{
    float y = -1000.f * lvl;
    glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST); glLineWidth(2);
    glBegin(GL_LINES);
    for (int k = 0; k < g_npoly; k++) {
        CPoly *c = &g_poly[k];
        if (!c->flag || !(c->levels >> lvl & 1)) continue;
        for (int e = 0; e < c->n; e++) {
            u8 t = c->edge[e] & 0xFF;
            if (!t || (t & 2)) glColor3f(0.2f, 1, 0.2f);
            else if (c->flag & 0x40) glColor3f(1, 1, 0);
            else glColor3f(1, 0.2f, 0.2f);
            line3(y, c->pt[e][0], c->pt[e][1], c->pt[e + 1][0], c->pt[e + 1][1]);
        }
    }
    glColor3f(1, 0.3f, 1);
    for (int k = 0; k < g_ndoor; k++) {
        Door *d = &g_door[k];
        if (d->level != lvl) continue;
        for (int e = 0; e < 4; e++) line3(y, d->q[e][0], d->q[e][1], d->q[(e + 1) & 3][0], d->q[(e + 1) & 3][1]);
    }
    glColor3f(0, 1, 1);
    for (int k = 0; k < g_nzone; k++) {
        CZone *q = &g_zone[k];
        if (q->cam != cam) continue;
        for (int e = 0; e < 4; e++) line3(y, q->q[e][0], q->q[e][1], q->q[(e + 1) & 3][0], q->q[(e + 1) & 3][1]);
    }
    glEnd(); glLineWidth(1);
}

static float g_pos[3], g_yaw, g_time; static int g_seq, g_paused;
static const Actor *A;           /* actor being drawn */

static void pose(Xf *out)
{
    /* frame lookup with linear interpolation (the game uses cubic over 4 keys) */
    float rot[64][3] = {{0}}, root[3] = {0}, orot[3] = {0};
    if (M->nseq) {
        const Seq *s = &M->seqs[A->seq % M->nseq];
        u32 stride = (16 + 6 * M->nparts + 3) & ~3u;
        float total = 0;
        for (int f = 0; f < s->frames; f++) { s16 d = rds16(s->p + 0x14 + f * stride + 2); total += d > 0 ? d : 1; }
        float t = fmodf(A->time, total);
        int f = 0; float dur = 1;
        for (; f < s->frames; f++) {
            s16 d = rds16(s->p + 0x14 + f * stride + 2); dur = d > 0 ? d : 1;
            if (t < dur) break;
            t -= dur;
        }
        if (f >= s->frames) f = s->frames - 1;
        int g = (f + 1) % s->frames;
        float a = t / dur;
        const u8 *A = s->p + 0x14 + f * stride, *B = s->p + 0x14 + g * stride;
        for (int i = 0; i < 3; i++) {
            root[i] = rds16(A + 4 + i * 2) * (1 - a) + rds16(B + 4 + i * 2) * a;
            orot[i] = rds16(A + 10 + i * 2) * (1 - a) + rds16(B + 10 + i * 2) * a;
        }
        for (int p = 0; p < M->nparts; p++)
            for (int i = 0; i < 3; i++) {
                int x = rds16(A + 16 + p * 6 + i * 2) & 4095, y = rds16(B + 16 + p * 6 + i * 2) & 4095;
                if (y - x > 2048) y -= 4096; else if (x - y > 2048) y += 4096;
                rot[p][i] = x * (1 - a) + y * a;
            }
    } else {
        for (int i = 0; i < 3; i++) root[i] = M->parts[0].joint[i];
    }
    Xf obj;
    float yaw[3] = {orot[0], orot[1] + A->yaw, orot[2]};
    rot_yxz(yaw, obj.m);
    for (int i = 0; i < 3; i++) obj.t[i] = A->pos[i];
    for (int p = 0; p < M->nparts; p++) {
        Xf local;
        rot_yxz(rot[p], local.m);
        for (int i = 0; i < 3; i++) local.t[i] = p == 0 ? root[i] : M->parts[p].joint[i];
        int par = M->parts[p].parent;
        xf_mul(par >= 0 && par < p ? &out[par] : &obj, &local, &out[p]);
    }
}

static const char *ENEMY_FILE[15] = {0, 0, "E00", "E10", "E20", "E30", "E40", "E90", "E60", "E70", "E80", "E31", "E32", "EA0", "E50"};
static const char *TEX2[10] = {"E00.TEX", "E00.TEX", "E00.TEX", "E02.TEX", "E00_02.TEX", "E02.TEX", "E03.TEX", "E00_03.TEX", "E03.TEX", "E01.TEX"};
static const char *TEX7[9] = {"E90.TEX", "E90_91.TEX", "E91.TEX", "E90.TEX", "E90_91.TEX", "E91.TEX", "E90.TEX", "E90_91.TEX", "E91.TEX"};
static const char *TEX6[2] = {"E40.TEX", "E41.TEX"};

static Actor g_enemy[64]; static int g_nenemy;

static void build_enemies(void)
{
    g_nenemy = 0;
    for (int k = 0; k < g_nspawn; k++) {
        Spawn *s = &g_spawn[k];
        const char *tex = s->type == 2 ? TEX2[s->variant % 10] : s->type == 7 ? TEX7[s->variant % 9] :
                          s->type == 6 ? TEX6[s->variant & 1] : NULL;
        char mname[16]; snprintf(mname, sizeof mname, "%s", ENEMY_FILE[s->type]);
        Model *m = get_model(mname, tex);
        if (!m) continue;
        Actor *a = &g_enemy[g_nenemy++];
        memset(a, 0, sizeof *a);
        a->m = m; a->pos[0] = s->pos[0]; a->pos[1] = s->pos[1]; a->pos[2] = s->pos[2];
        a->yaw = s->dir; a->seq = 0; a->time = (float)(k * 37);
    }
}

static void emit(const float s[3], u8 u, u8 v)
{
    glTexCoord2f((u + 0.5f) / 256.f, (v + 0.5f) / 256.f);
    glVertex3f(s[0], s[1], -s[2]);
}

static int nclip(const float *a, const float *b, const float *c)
{
    return (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]) >= 0;
}

static int g_debug;

static void draw_actor(const Actor *a)
{
    A = a; M = a->m;
    if (!M || !M->blk.p) return;
    Xf xf[64];
    pose(xf);
    int st_all = 0, st_proj = 0, st_vis = 0; float bb[4] = {1e9f, 1e9f, -1e9f, -1e9f};
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST); glAlphaFunc(GL_GREATER, 0.5f);
    glBindTexture(GL_TEXTURE_2D, M->tex);
    glColor3f(1, 1, 1);
    glBegin(GL_TRIANGLES);
    for (int p = 0; p < M->nparts; p++) {
        const Part *P = &M->parts[p];
        for (int k = 0; k < P->ntri + P->nquad; k++) {
            int quad = k >= P->ntri;
            const u8 *e = M->blk.p + (quad ? P->quads + (k - P->ntri) * 16 : P->tris + k * 12);
            int nv = quad ? 4 : 3, ok = 1;
            float s[4][3]; u8 uv[4][2];
            for (int i = 0; i < nv; i++) {
                const u8 *v = M->blk.p + M->verts + rd16(e + i * 2) * 8;
                float lv[3] = {rds16(v), rds16(v + 2), rds16(v + 4)}, w[3];
                xf_apply(&xf[p], lv, w);
                ok &= project(w, s[i]);
            }
            st_all++;
            if (!ok) continue;
            st_proj++;
            for (int i = 0; i < nv; i++) { if (s[i][0] < bb[0]) bb[0] = s[i][0]; if (s[i][1] < bb[1]) bb[1] = s[i][1]; if (s[i][0] > bb[2]) bb[2] = s[i][0]; if (s[i][1] > bb[3]) bb[3] = s[i][1]; }
            if (quad) for (int i = 0; i < 4; i++) { uv[i][0] = e[8 + i * 2]; uv[i][1] = e[9 + i * 2]; }
            else {  /* TRI: u2 v2 u0 v0 u1 v1 */
                uv[2][0] = e[6]; uv[2][1] = e[7]; uv[0][0] = e[8]; uv[0][1] = e[9]; uv[1][0] = e[10]; uv[1][1] = e[11];
            }
            if (!nclip(s[0], s[1], s[2])) continue;
            st_vis++;
            emit(s[0], uv[0][0], uv[0][1]); emit(s[1], uv[1][0], uv[1][1]); emit(s[2], uv[2][0], uv[2][1]);
            if (quad) { emit(s[1], uv[1][0], uv[1][1]); emit(s[3], uv[3][0], uv[3][1]); emit(s[2], uv[2][0], uv[2][1]); }
        }
    }
    glEnd();
    glDisable(GL_ALPHA_TEST);
    if (g_debug) {
        printf("%s: polys %d projected %d front-facing %d, screen bbox (%.0f,%.0f)-(%.0f,%.0f), root world (%.0f,%.0f,%.0f)\n",
               M->name, st_all, st_proj, st_vis, bb[0], bb[1], bb[2], bb[3], xf[0].t[0], xf[0].t[1], xf[0].t[2]);
    }
}

static void save_ppm(const char *path, int w, int h)
{
    u8 *px = malloc((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    FILE *f = fopen(path, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; y--) fwrite(px + (size_t)y * w * 3, 1, (size_t)w * 3, f);
        fclose(f);
    }
    free(px);
}

/* -------------------------------------------------------------------- main */
int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s DATA_DIR STxyy [--cam N] [--model WEP_P002] [--anim N] [--vram FILE] [--grid] [--collision] [--no-follow] [--door N] [--no-enemies]\n"
                        "       [--no-model] [--no-masks] [--no-bg] [--pos X Z] [--yaw A]\n"
                        "       [--screenshot out.ppm] [--frames N]\n", argv[0]);
        return 1;
    }
    g_dir = argv[1];
    const char *room = argv[2], *model = "WEP_P002", *shot = NULL, *extra[8]; int nextra = 0;
    int cam = 0, grid = 0, show_model = 1, show_masks = 1, show_bg = 1, frames = 0, posset = 0;
    int floorset = 0, show_col = 0, follow = 1, lvl = 0, door_req = -1, camset = 0, show_enemies = 1;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--cam") && i + 1 < argc) { cam = atoi(argv[++i]); camset = 1; }
        else if (!strcmp(argv[i], "--model") && i + 1 < argc) model = argv[++i];
        else if (!strcmp(argv[i], "--anim") && i + 1 < argc) g_seq = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--grid")) grid = 1;
        else if (!strcmp(argv[i], "--debug")) g_debug = 1;
        else if (!strcmp(argv[i], "--vram") && i + 1 < argc && nextra < 8) extra[nextra++] = argv[++i];
        else if (!strcmp(argv[i], "--no-model")) show_model = 0;
        else if (!strcmp(argv[i], "--no-masks")) show_masks = 0;
        else if (!strcmp(argv[i], "--no-bg")) show_bg = 0;
        else if (!strcmp(argv[i], "--collision")) show_col = 1;
        else if (!strcmp(argv[i], "--no-enemies")) show_enemies = 0;
        else if (!strcmp(argv[i], "--door") && i + 1 < argc) door_req = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--no-follow")) follow = 0;
        else if (!strcmp(argv[i], "--pos") && i + 2 < argc) { g_pos[0] = atof(argv[++i]); g_pos[2] = atof(argv[++i]); posset = 1; }
        else if (!strcmp(argv[i], "--yaw") && i + 1 < argc) g_yaw = atof(argv[++i]);
        else if (!strcmp(argv[i], "--floor") && i + 1 < argc) { g_floor = atof(argv[++i]); floorset = 1; }
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
    }
    if (SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    const int W = 960, Hh = 720;
    SDL_Window *win = SDL_CreateWindow("dc2view", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, Hh, SDL_WINDOW_OPENGL);
    if (!win || !SDL_GL_CreateContext(win)) { fprintf(stderr, "GL: %s\n", SDL_GetError()); return 1; }
    if (!load_room(room)) return 1;
    if (cam >= g_ncam) cam = 0;
    for (int i = 0; i < nextra; i++) {          /* extra VRAM uploads (e.g. body textures) */
        char nm[64]; snprintf(nm, sizeof nm, strchr(extra[i], '.') ? "%s" : "%s.DAT", extra[i]);
        Buf b = read_file(nm); Rec r[64];
        if (b.p) { load_vram_records(r, parse_container(b.p, b.n, 0, r, 64)); free(b.p); }
        else fprintf(stderr, "cannot read %s\n", nm);
    }
    Model *hero = show_model ? get_model(model, NULL) : NULL;
    if (!hero) show_model = 0;
    if (show_enemies) build_enemies();
    if (posset && !camset) {            /* --pos without --cam: camera of the zone, as in the game */
        int l = floor_level_at(g_pos[0], g_pos[2], -1);
        int c = zone_camera(g_pos[0], g_pos[2], floorset ? level_of(g_floor) : l);
        if (c >= 0) cam = c;
    }
    load_camera(cam);
    setup_view(&g_cams[cam]);
    if (!posset) { g_pos[0] = g_cams[cam].target[0]; g_pos[2] = g_cams[cam].target[2]; }
    if (floorset) lvl = level_of(g_floor);
    else {
        int l = posset ? floor_level_at(g_pos[0], g_pos[2], camera_level(cam)) : -1;
        lvl = l >= 0 ? l : camera_level(cam);
        g_floor = -1000.f * lvl;
    }
    g_pos[1] = g_floor;
    printf("camera %d, floor level %d (y = %.0f)\n", cam, lvl, g_floor);
    if (door_req >= 0 && door_req < g_ndoor) {   /* start by walking through door N */
        Door *d = &g_door[door_req];
        g_pos[0] = (d->q[0][0] + d->q[1][0] + d->q[2][0] + d->q[3][0]) / 4.f;
        g_pos[2] = (d->q[0][1] + d->q[1][1] + d->q[2][1] + d->q[3][1]) / 4.f;
        lvl = d->level;
        SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; SDL_PushEvent(&e);
    }

    for (int frame = 0, run = 1; run; frame++) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) run = 0;
            if (ev.type != SDL_KEYDOWN) continue;
            switch (ev.key.keysym.sym) {
            case SDLK_ESCAPE: run = 0; break;
            case SDLK_RIGHT: case SDLK_LEFT:
                cam = (cam + (ev.key.keysym.sym == SDLK_RIGHT ? 1 : g_ncam - 1)) % g_ncam;
                load_camera(cam); setup_view(&g_cams[cam]);
                if (!posset) {
                    g_pos[0] = g_cams[cam].target[0]; g_pos[2] = g_cams[cam].target[2];
                    if (!floorset) { lvl = camera_level(cam); g_floor = g_pos[1] = -1000.f * lvl; }
                }
                printf("camera %d, floor level %d (y = %.0f)\n", cam, lvl, g_floor);
                break;
            case SDLK_c:   /* put the model back under the camera target */
                posset = 0; g_pos[0] = g_cams[cam].target[0]; g_pos[2] = g_cams[cam].target[2];
                printf("model at (%.0f, %.0f, %.0f)\n", g_pos[0], g_pos[1], g_pos[2]); break;
            case SDLK_UP: if (!hero) break; g_seq = hero->nseq ? (g_seq + 1) % hero->nseq : 0; g_time = 0; printf("anim %d\n", g_seq); break;
            case SDLK_DOWN: if (!hero) break; g_seq = hero->nseq ? (g_seq + hero->nseq - 1) % hero->nseq : 0; g_time = 0; printf("anim %d\n", g_seq); break;
            case SDLK_g: grid ^= 1; break;
            case SDLK_m: show_masks ^= 1; break;
            case SDLK_b: show_bg ^= 1; break;
            case SDLK_o: show_model ^= 1; break;
            case SDLK_SPACE: g_paused ^= 1; break;
            case SDLK_w: case SDLK_s: case SDLK_a: case SDLK_d: {
                SDL_Keycode k = ev.key.keysym.sym;
                float nx = g_pos[0] + (k == SDLK_d ? 250.f : k == SDLK_a ? -250.f : 0.f);
                float nz = g_pos[2] + (k == SDLK_w ? 250.f : k == SDLK_s ? -250.f : 0.f);
                posset = 1;
                if (follow && blocked(g_pos[0], g_pos[2], nx, nz, lvl)) { printf("wall\n"); break; }
                g_pos[0] = nx; g_pos[2] = nz;
                if (follow) {
                    int l = floor_level_at(nx, nz, lvl);
                    if (l >= 0 && l != lvl) { lvl = l; g_floor = g_pos[1] = -1000.f * lvl; printf("floor level %d\n", lvl); }
                    int c = zone_camera(nx, nz, lvl);
                    if (c >= 0 && c != cam) { cam = c; load_camera(cam); setup_view(&g_cams[cam]); printf("camera %d\n", cam); }
                    int dk = door_at(nx, nz, lvl);
                    if (dk >= 0) printf("door -> ST%X%02X (Enter)\n", g_door[dk].room >> 8, g_door[dk].room & 0xFF);
                }
                break;
            }
            case SDLK_RETURN: {      /* go through the door the model stands in */
                int k = door_at(g_pos[0], g_pos[2], lvl);
                if (k < 0) { printf("no door here\n"); break; }
                Door d = g_door[k];
                char nm[16]; snprintf(nm, sizeof nm, "ST%X%02X", d.room >> 8, d.room & 0xFF);
                if (!load_room(nm)) { printf("cannot load %s\n", nm); return 1; }
                g_pos[0] = d.dest[0]; g_pos[1] = d.dest[1]; g_pos[2] = d.dest[2];
                g_yaw = d.dir; lvl = level_of(d.dest[1]); g_floor = -1000.f * lvl; g_pos[1] = g_floor;
                int c = zone_camera(g_pos[0], g_pos[2], lvl);
                cam = c >= 0 ? c : 0; posset = 1;
                load_camera(cam); setup_view(&g_cams[cam]);
                printf("entered %s, camera %d, level %d\n", nm, cam, lvl);
                if (show_enemies) build_enemies();
                break;
            }
            case SDLK_n: show_enemies ^= 1; if (show_enemies && !g_nenemy) build_enemies(); break;
            case SDLK_f: follow ^= 1; printf("walls/camera zones %s\n", follow ? "on" : "off"); break;
            case SDLK_k: show_col ^= 1; break;
            case SDLK_PAGEUP: g_floor -= 100; g_pos[1] = g_floor; lvl = level_of(g_floor); printf("floor %g (level %d)\n", g_floor, lvl); break;
            case SDLK_PAGEDOWN: g_floor += 100; g_pos[1] = g_floor; lvl = level_of(g_floor); printf("floor %g (level %d)\n", g_floor, lvl); break;
            case SDLK_q: g_yaw -= 256; break;
            case SDLK_e: g_yaw += 256; break;
            default: break;
            }
        }
        glViewport(0, 0, W, Hh);
        glClearColor(0.1f, 0.1f, 0.1f, 1); glClearDepth(1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, 320, 240, 0, 0, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        if (show_bg) draw_background();
        if (show_model) {
            Actor h = {hero, {g_pos[0], g_pos[1], g_pos[2]}, g_yaw, g_time, g_seq, 0};
            draw_actor(&h);
        }
        if (show_enemies)
            for (int k = 0; k < g_nenemy; k++) {
                if (!g_paused) g_enemy[k].time += 1.f;
                draw_actor(&g_enemy[k]);
            }
        if (show_masks) draw_masks();
        g_debug = 0;
        if (grid) draw_grid();
        if (show_col) draw_collision(cam, lvl);
        if (!g_paused) g_time += 1.f;            /* 1 tick per displayed frame at 60 Hz */
        if (shot && frame + 1 >= (frames > 0 ? frames : 1)) { save_ppm(shot, W, Hh); run = 0; }
        SDL_GL_SwapWindow(win);
        if (!shot) SDL_Delay(16);
    }
    SDL_Quit();
    return 0;
}
