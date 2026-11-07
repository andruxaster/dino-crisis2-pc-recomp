/* smoke -- the reimplemented game code running natively (no Dino2.exe code).
 * Builds the game's memory image from the data sections of Dino2.exe
 * (temporary: the tables will move into C), loads a room the way the file
 * loader does (LZSS record to 0x5E0000), starts the room script like
 * Script_Init and runs Script_Run frame by frame until an opcode that is
 * not reimplemented yet stops the threads.
 *   usage: smoke Dino2.exe DATA_DIR [ROOM] [FRAMES] */
#include <stdio.h>
#include <stdlib.h>
#include "dc2/game.h"

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

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s Dino2.exe DATA_DIR [ROOM] [FRAMES]\n", argv[0]); return 1; }
    const char *room = argc > 3 ? argv[3] : "ST100";
    int frames = argc > 4 ? atoi(argv[4]) : 120;
    if (!load_data_sections(argv[1])) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    U32(A_G_PTR) = A_G_STATIC;

    /* room file: 0x800-byte header of 32-byte records, data sector aligned */
    char path[512]; snprintf(path, sizeof path, "%s/%s.DAT", argv[2], room);
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
    printf("%s: room block unpacked by Lzss_Decode, script section at %08X\n", room, U32(0x5E0000 + 0x14));

    /* runtime tables normally set up by the stage/room loaders (not rewritten
       yet): empty object arrays in a free part of .psxseg, room sections
       from the block header as Room_SetPointers 0x481E10 does */
    memset(MEM(0x690000), 0, 0x30000);
    U32(G() + 0x940) = 0x690000;              /* objects, 0x260 bytes */
    U32(G() + 0x93C) = 0x6A0000;              /* effects, 0x70 bytes */
    U32(G() + 0x944) = 0x6A8000;              /* 0xAC bytes */
    U32(G() + 0x948) = 0x6B0000;              /* 0x34 bytes */
    U32(G() + 0xA5C) = 0x6B4000;              /* AOT, 0x28 bytes */
    U32(G() + 0xA4C) = U32(0x5E0000 + 0x08);  /* collision */
    U32(G() + 0xA44) = U32(0x5E0000 + 0x0C) + 4;  /* camera zones (CamZones_Init) */

    /* Script_Init 0x48BB60: header 0x1C bytes, globals = base + hdr[3] */
    gptr a54 = U32(0x5E0000 + 0x14);
    U32(G() + G_SCRIPT) = a54 + 0x1C;
    U32(A_SCR_GLOBALS) = a54 + 0x1C + U32(a54 + 0xC);
    U32(A_SCR_THREADS) = 0x6E0000;                     /* scratch thread area */
    memset(MEM(0x6E0000), 0, SCR_NTHREADS * SCR_THREAD_SIZE);
    Script_StartThread(0, 0);

    for (int f = 0; f < frames; f++) {
        Script_Run();
        int alive = 0;
        for (int t = 0; t < SCR_NTHREADS; t++) {
            u8 st = U8(0x6E0000 + t * SCR_THREAD_SIZE + T_STATE);
            if (st) alive++;
        }
        if (!alive) { printf("frame %d: no thread left\n", f); break; }
    }
    return 0;
}
