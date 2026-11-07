/* smoke -- the reimplemented game code running natively (no Dino2.exe code).
 * Builds the game's memory image from the data sections of Dino2.exe
 * (temporary: the tables will move into C), loads a room the way the file
 * loader does (LZSS record to 0x5E0000), starts the room script like
 * Script_Init and runs Script_Run frame by frame until an opcode that is
 * not reimplemented yet stops the threads.
 * Functions that are not rewritten yet are reported once and skipped.
 *   usage: smoke Dino2.exe DATA_DIR [ROOM|all] [FRAMES] */
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include "dc2/game.h"
#include "dc2/orig.h"

u8 *g_membase;

static u8 *read_all(const char *path, long *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(*n);
    if (fread(d, 1, *n, f) != (size_t)*n) { fclose(f); free(d); return NULL; }
    fclose(f);
    return d;
}

static int load_data_sections(const char *exe)
{
    long n; u8 *d = read_all(exe, &n);
    if (!d) return 0;
    u32 pe = *(u32 *)(d + 0x3C); u16 nsec = *(u16 *)(d + pe + 6), opt = *(u16 *)(d + pe + 20);
    g_membase = calloc(1, IMAGE_SIZE + TMP_SIZE);
    for (int i = 0; i < nsec; i++) {
        u8 *s = d + pe + 24 + opt + 40 * i;
        u32 va = *(u32 *)(s + 12), rs = *(u32 *)(s + 16), ro = *(u32 *)(s + 20), ch = *(u32 *)(s + 36);
        if (ch & 0x20000000) continue;                  /* executable: not needed */
        memcpy(g_membase + va, d + ro, rs);
    }
    free(d);
    return 1;
}

static const char *g_exe;

/* stand-in for the object type handlers (not rewritten yet): they build the
   object's model parts; give each object a chain of 32 empty parts so that
   code which walks the parts has something to work on */
#define TYPE_TABLE 0x6B8000u
#define PARTS      0x6C0000u      /* 32 objects x 32 parts x 0x80 */
static void type_handler(gptr fn, u32 obj)
{
    (void)fn;
    if (obj < 0x690000 || obj >= 0x690000 + 32 * 0x260 || U32(obj + 0x64)) return;
    gptr p = PARTS + (obj - 0x690000) / 0x260 * 0x1000, parent = obj;
    memset(MEM(p), 0, 0x1000);
    U32(obj + 0x64) = p;
    for (int i = 0; i < 32; i++, p += 0x80) {
        U32(p + 0x58) = parent;
        if (i < 31) U32(p + 0x54) = p + 0x80;
        parent = p;
    }
}

static int run_room(const char *data, const char *room, int frames)
{
    free(g_membase);
    if (!load_data_sections(g_exe)) { fprintf(stderr, "cannot read %s\n", g_exe); return 1; }
    U32(A_G_PTR) = A_G_STATIC;

    /* room file: 0x800-byte header of 32-byte records, data sector aligned */
    char path[512]; snprintf(path, sizeof path, "%s/%s.DAT", data, room);
    long n; u8 *d = read_all(path, &n);
    if (!d) { fprintf(stderr, "cannot read %s\n", path); return 1; }
    u32 off = 0x800; int got = 0;
    for (int r = 0; r < 64; r++) {
        u32 type = *(u32 *)(d + 32 * r), size = *(u32 *)(d + 32 * r + 4), arg = *(u32 *)(d + 32 * r + 8);
        if (!memcmp(d + 32 * r, "dummy", 5) || (!type && !size)) continue;
        if (type == 5 && arg == 0x5E0000) {
            memcpy(MEM(0x610000), d + off, size);
            Lzss_Decode(0x5E0000, 0x610000, size);
            got = 1;
        }
        off += (size + 0x7FF) & ~0x7FFu;
    }
    free(d);
    if (!got) { fprintf(stderr, "%s: no room block\n", room); return 1; }

    /* runtime tables normally set up by the stage/room loaders (not rewritten
       yet): empty object arrays in a free part of .psxseg, room sections
       from the block header as Room_SetPointers 0x481E10 does */
    memset(MEM(0x690000), 0, 0x50000);
    for (int i = 0; i < 32; i++) type_handler(0, 0x690000 + i * 0x260);
    U32(G() + 0x940) = 0x690000;              /* objects, 0x260 bytes */
    U32(G() + 0x93C) = 0x6A0000;              /* effects, 0x70 bytes */
    U32(G() + 0x944) = 0x6A8000;              /* 0xAC bytes */
    U32(G() + 0x948) = 0x6B0000;              /* 0x34 bytes */
    U32(G() + 0xA5C) = 0x6B4000;              /* AOT, 0x28 bytes */
    U32(0x87DCA0) = TYPE_TABLE;               /* object type handlers: stand-in */
    g_orig_call1 = type_handler;
    U32(0x87F940) = 32;                       /* object slots */
    U32(G() + 0xA4C) = U32(0x5E0000 + 0x08);  /* collision */
    U32(G() + 0xA44) = U32(0x5E0000 + 0x0C) + 4;  /* camera zones (CamZones_Init) */

    /* Script_Init 0x48BB60: header 0x1C bytes, globals = base + hdr[3] */
    gptr a54 = U32(0x5E0000 + 0x14);
    U32(G() + G_SCRIPT) = a54 + 0x1C;
    U32(A_SCR_GLOBALS) = a54 + 0x1C + U32(a54 + 0xC);
    U32(A_SCR_THREADS) = 0x6E0000;                     /* scratch thread area */
    memset(MEM(0x6E0000), 0, SCR_NTHREADS * SCR_THREAD_SIZE);
    Script_StartThread(0, 0);

    int f, maxalive = 0, alive = 0;
    for (f = 0; f < frames; f++) {
        Script_Run();
        alive = 0;
        for (int t = 0; t < SCR_NTHREADS; t++)
            if (U8(0x6E0000 + t * SCR_THREAD_SIZE + T_STATE)) alive++;
        if (alive > maxalive) maxalive = alive;
        if (!alive) break;
    }
    printf("%s: %d frames, up to %d threads, %d running at the end\n", room, f, maxalive, alive);
    return 0;
}

static int cmp(const void *a, const void *b) { return strcmp(*(char **)a, *(char **)b); }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s Dino2.exe DATA_DIR [ROOM|all] [FRAMES]\n", argv[0]); return 1; }
    g_exe = argv[1];
    const char *room = argc > 3 ? argv[3] : "ST100";
    int frames = argc > 4 ? atoi(argv[4]) : 120;
    if (strcmp(room, "all")) return run_room(argv[2], room, frames);
    DIR *dir = opendir(argv[2]);
    if (!dir) { fprintf(stderr, "cannot open %s\n", argv[2]); return 1; }
    char *names[256]; int n = 0; struct dirent *e;
    while ((e = readdir(dir)) && n < 256) {
        size_t l = strlen(e->d_name);
        if (l == 9 && !strncmp(e->d_name, "ST", 2) && !strcmp(e->d_name + 5, ".DAT")) names[n++] = strndup(e->d_name, 5);
    }
    closedir(dir);
    qsort(names, n, sizeof *names, cmp);
    int bad = 0;
    for (int i = 0; i < n; i++) bad |= run_room(argv[2], names[i], frames);
    return bad;
}
