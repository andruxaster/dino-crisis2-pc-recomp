/* The camera backgrounds made ahead (bgvq_format.h, tools/dreamcast/dc2_bgvq.c)
 * on the Dreamcast: Camera_Load asks for a camera's picture by its DBS file
 * and sector (g_cam_bg_hook); when Data\BGVQ\<room>.VQ has it, its two VQ
 * textures go to VRAM, the picture is made from them into the game's
 * memory (for whatever reads it there) and the JPEG is not decoded. The PC
 * layer's background upload (g_pc_bg_hook) then finds the same picture and
 * leaves the surface alone: the PVR draws the VQ textures for it. Any other
 * picture in the background buffer (a menu's, a movie's) goes the usual way. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dc2/types.h"
#include "dc2/pcio.h"
#include "dc2/game.h"
#include "dc2/pcdraw.h"
#include "bgvq_format.h"
#include "bgvq_decode.h"
#include "render_pvr.h"

static struct {
    char base[32];                  /* the room file whose index is held ("" none; "-" no file for it) */
    FILE *f;
    u32 n, flags, *ent;             /* sector, offset pairs */
    int on;                         /* the picture at 0x6C1500 is the VQ one (with sum) */
    u32 sum;
    u32 loaded, missing;            /* (the log) */
} g_v;
static u8 g_blob[BGVQ_BLOB] __attribute__((aligned(32)));

static u32 rd32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }

/* bg_upload's own check of the picture (0x4BE120): 300 words */
static u32 sparse_sum(gptr img)
{
    u32 sum = 0;
    for (u32 r = 0; r < 15; r++) for (u32 k = 0; k < 20; k++) sum += U32(img + r * 0x2800 + k * 0x20);
    return sum;
}

/* the index of Data\BGVQ\<base>.VQ held; 0 when there is none */
static int open_room(const char *base)
{
    if (!strcmp(g_v.base, base)) return g_v.f != NULL;
    if (g_v.f) fclose(g_v.f);
    free(g_v.ent);
    memset(g_v.base, 0, sizeof g_v.base); g_v.f = NULL; g_v.ent = NULL; g_v.n = 0;
    snprintf(g_v.base, sizeof g_v.base, "%s", base);
    char gp[64], path[512];
    snprintf(gp, sizeof gp, "data\\BGVQ\\%s.VQ", base);
    Pc_HostPath(gp, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    u8 h[16];
    if (Host_Read(h, 16, f) != 16 || memcmp(h, BGVQ_MAGIC, 8) || rd32(h + 8) > 4096) { fclose(f); return 0; }
    u32 n = rd32(h + 8);
    u8 *e = malloc(8 * n + 1);
    if (!e || Host_Read(e, 8 * n, f) != 8 * n) { free(e); fclose(f); return 0; }
    g_v.ent = malloc(sizeof(u32) * 2 * n + 1);
    for (u32 i = 0; i < 2 * n; i++) g_v.ent[i] = rd32(e + 4 * i);
    free(e);
    g_v.f = f; g_v.n = n; g_v.flags = rd32(h + 12);
    return 1;
}

/* Camera_Load's question: the picture of the record at sector sec of the
   DBS file dbs (the game's path) put at dst; 1 when done */
static int cam_bg(const char *dbs, u32 sec, gptr dst)
{
    const char *b = dbs;
    for (const char *p = dbs; *p; p++) if (*p == '\\' || *p == '/' || *p == ':') b = p + 1;
    char base[32];
    size_t k = 0;
    for (; b[k] && b[k] != '.' && k < sizeof base - 1; k++) base[k] = (char)toupper((unsigned char)b[k]);
    base[k] = 0;
    g_v.on = 0;
    if (!open_room(base)) { g_v.missing++; return 0; }
    u32 lo = 0, hi = g_v.n;
    while (lo < hi) { u32 m = (lo + hi) / 2; if (g_v.ent[2 * m] < sec) lo = m + 1; else hi = m; }
    if (lo >= g_v.n || g_v.ent[2 * lo] != sec) { g_v.missing++; return 0; }
    if (fseek(g_v.f, (long)g_v.ent[2 * lo + 1], SEEK_SET) || Host_Read(g_blob, BGVQ_BLOB, g_v.f) != BGVQ_BLOB) {
        clearerr(g_v.f); g_v.missing++; return 0;
    }
    if (!RenderPvr_BgVqUpload(g_blob)) { g_v.missing++; return 0; }
    bgvq_decode(g_blob, (unsigned short *)MEM(dst), g_v.flags & 1);
    g_v.sum = sparse_sum(dst);
    g_v.on = 1;
    g_v.loaded++;
    return 1;
}

/* the PC layer's background upload (0x4BE120): the VQ picture is drawn by
   the PVR from its own textures; anything else the usual way */
static int bg_upload_hook(gptr img)
{
    if (!g_v.on || img != 0x6C1500 || U32(0x75CEB4) || U32(0x75CE90) != 512 || U32(0x75CE98) != 256 || sparse_sum(img) != g_v.sum) {
        RenderPvr_BgVqUse(0);
        return 0;
    }
    RenderPvr_BgVqUse(U32(0x75CEA8));
    U32(0x75CEA0) = ~g_v.sum;                       /* (the surface is not this picture: uploaded again when needed) */
    return 1;
}

void BgVq_Init(void)
{
    g_cam_bg_hook = cam_bg;
    g_pc_bg_hook = bg_upload_hook;
    char path[512];
    Pc_HostPath("data\\BGVQ", path, sizeof path);
    printf("backgrounds: made ahead in %s when there\n", path);
}

u32 BgVq_Loaded(void) { return g_v.loaded; }
u32 BgVq_Missing(void) { return g_v.missing; }
