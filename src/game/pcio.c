/* See include/dc2/pcio.h. */
#include "dc2/pcio.h"

#ifdef DC2_IN_PROCESS
#define ORIG(addr, type) ((type)(uintptr_t)(addr))
s32  DC2CALL Pc_Open(gptr p, u32 a, u32 b)      { return ORIG(0x4C03F0, s32 (DC2CALL *)(gptr, u32, u32))(p, a, b); }
s32  DC2CALL Pc_Read(s32 fd, gptr buf, u32 n)   { return ORIG(0x4C04E0, s32 (DC2CALL *)(s32, gptr, u32))(fd, buf, n); }
u32  DC2CALL Pc_Size(s32 fd)                    { return ORIG(0x4C0540, u32 (DC2CALL *)(s32))(fd); }
s32  DC2CALL Pc_Seek(s32 fd, s32 off, u32 w)    { return ORIG(0x4C0580, s32 (DC2CALL *)(s32, s32, u32))(fd, off, w); }
s32  DC2CALL Pc_SeekSet(s32 fd, s32 off)         { return ORIG(0x4C0560, s32 (DC2CALL *)(s32, s32))(fd, off); }
void DC2CALL Pc_Close(s32 fd)                   { ORIG(0x4C05A0, void (DC2CALL *)(s32))(fd); }
gptr DC2CALL Crt_malloc(u32 n)                  { return ORIG(0x4DD711, gptr (DC2CALL *)(u32))(n); }
void DC2CALL Crt_free(gptr p)                   { ORIG(0x4DD628, void (DC2CALL *)(gptr))(p); }
void DC2CALL Task_Sleep(u32 n)                  { ORIG(0x4C29A9, void (DC2CALL *)(u32))(n); }
#else
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <dirent.h>

const char *g_pc_root = ".";

static FILE *g_files[16];

/* "data\NAME" -> root/data/NAME, each component matched without case */
static int resolve(const char *game_path, char *out, size_t n)
{
    char comp[256];
    snprintf(out, n, "%s", g_pc_root);
    for (const char *s = game_path; *s;) {
        size_t k = 0;
        while (*s && *s != '\\' && *s != '/' && k < sizeof comp - 1) comp[k++] = *s++;
        comp[k] = 0;
        while (*s == '\\' || *s == '/') s++;
        DIR *d = opendir(out);
        const char *found = comp;
        struct dirent *e;
        if (d) {
            while ((e = readdir(d))) if (!strcasecmp(e->d_name, comp)) { found = e->d_name; break; }
        }
        size_t l = strlen(out);
        snprintf(out + l, n - l, "/%s", found);
        if (d) closedir(d);
    }
    return 1;
}

s32 DC2CALL Pc_Open(gptr path, u32 a, u32 b)
{
    (void)a; (void)b;
    int fd = 0;
    while (fd < 16 && g_files[fd]) fd++;
    if (fd == 16) return -1;
    char host[1024];
    resolve((const char *)MEM(path), host, sizeof host);
    if (!(g_files[fd] = fopen(host, "rb"))) return -1;
    return fd;
}
s32 DC2CALL Pc_Read(s32 fd, gptr buf, u32 n) { return (s32)fread(MEM(buf), 1, n, g_files[fd]); }
u32 DC2CALL Pc_Size(s32 fd)
{
    long cur = ftell(g_files[fd]); fseek(g_files[fd], 0, SEEK_END);
    long n = ftell(g_files[fd]); fseek(g_files[fd], cur, SEEK_SET);
    return (u32)n;
}
s32 DC2CALL Pc_Seek(s32 fd, s32 off, u32 w) { return fseek(g_files[fd], off, (int)w); }
s32 DC2CALL Pc_SeekSet(s32 fd, s32 off) { return fseek(g_files[fd], off, SEEK_SET); }
void DC2CALL Pc_Close(s32 fd) { fclose(g_files[fd]); g_files[fd] = NULL; }

/* first-fit heap in the emulated image: blocks of {u32 size | used bit, data} */
#define HEAP_BASE (IMAGE_BASE + IMAGE_SIZE + TMP_SIZE)
static int g_heap_init;
gptr DC2CALL Crt_malloc(u32 n)
{
    if (!g_heap_init) { U32(HEAP_BASE) = HEAP_SIZE - 4; g_heap_init = 1; }
    n = (n + 15) & ~15u;
    for (gptr b = HEAP_BASE; b < HEAP_BASE + HEAP_SIZE; b += 4 + (U32(b) & ~1u)) {
        u32 sz = U32(b);
        if (sz & 1) continue;
        while (b + 4 + sz < HEAP_BASE + HEAP_SIZE && !(U32(b + 4 + sz) & 1))   /* merge free */
            sz += 4 + U32(b + 4 + sz);
        U32(b) = sz;
        if (sz < n) continue;
        if (sz >= n + 32) { U32(b + 4 + n) = sz - n - 4; sz = n; }
        U32(b) = sz | 1;
        return b + 4;
    }
    fprintf(stderr, "Crt_malloc: out of heap (%u bytes)\n", n);
    return 0;
}
void DC2CALL Crt_free(gptr p) { if (p) U32(p - 4) &= ~1u; }
#endif
