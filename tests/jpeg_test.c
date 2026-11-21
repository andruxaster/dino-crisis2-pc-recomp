/* The rooms' JPEG backgrounds (src/pc/jpeg.c) on the host: every camera
 * picture of the DBS files, whole, with fill bytes, cut short and with bytes
 * changed, decoded and hashed. The harness checks jpeg.c against libjpeg in
 * the exe (make check); this keeps a faster decoder to the same pixels
 * without wine: write the hashes with the decoder known good, then check.
 *   jpeg_test DIR write FILE | check FILE | time
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include "dc2/types.h"
#include "dc2/gpu.h"

u8 *g_membase;
#define DST 0x6C1500u
#define SRC 0x800000u

static unsigned long long fnv(const u8 *p, size_t n, unsigned long long h)
{
    for (size_t i = 0; i < n; i++) h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}
static unsigned g_seed = 12345;
static unsigned rnd(void) { g_seed = g_seed * 1103515245u + 12345u; return g_seed >> 8; }
static int cmp(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "jpeg_test DIR write FILE | check FILE | time\n"); return 2; }
    g_membase = calloc(1, 0x500000);
    const char *mode = argv[2];
    FILE *ref = NULL;
    if (!strcmp(mode, "write")) ref = fopen(argv[3], "w");
    else if (!strcmp(mode, "check")) ref = fopen(argv[3], "r");
    if (strcmp(mode, "time") && !ref) { perror(argv[3]); return 2; }
    char *names[256]; int nn = 0;
    DIR *d = opendir(argv[1]);
    if (!d) { perror(argv[1]); return 2; }
    for (struct dirent *e; (e = readdir(d)) && nn < 256;) {
        size_t l = strlen(e->d_name);
        if (l > 4 && !strcasecmp(e->d_name + l - 4, ".DBS")) names[nn++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, nn, sizeof *names, cmp);
    int pics = 0, cases = 0, bad = 0;
    double secs = 0;
    u8 *src = MEM(SRC);
    for (int f = 0; f < nn; f++) {
        char path[1024]; snprintf(path, sizeof path, "%s/%s", argv[1], names[f]);
        FILE *fp = fopen(path, "rb"); if (!fp) continue;
        fseek(fp, 0, SEEK_END); long len = ftell(fp); fseek(fp, 0, SEEK_SET);
        u8 *buf = malloc(len); if (fread(buf, 1, len, fp) != (size_t)len) len = 0; fclose(fp);
        for (long o = 0; o + 0x1004 < len; o += 0x800) {
            const u8 *j = buf + o + 0x1000;
            u32 size = (u32)buf[o + 0x24] | buf[o + 0x25] << 8 | buf[o + 0x26] << 16 | (u32)buf[o + 0x27] << 24;
            if (j[0] != 0xFF || j[1] != 0xD8 || j[2] != 0xFF || !size || size > 0x40000 || o + 0x1000 + size > len) continue;
            pics++;
            for (int v = 0; v < (strcmp(mode, "time") ? 5 : 1); v++) {
                memcpy(src, j, size);
                u32 sz = size;
                if (v == 1) {                                   /* fill bytes before a stuffed FF */
                    u32 at = size / 2;
                    while (at + 1 < size && !(src[at] == 0xFF && src[at + 1] == 0)) at++;
                    if (at + 1 >= size) continue;
                    memmove(src + at + 2, src + at, size - at); src[at] = src[at + 1] = 0xFF; sz = size + 2;
                }
                if (v == 2) sz = size / 2 + rnd() % (size / 2); /* cut short */
                if (v == 3) sz = 600 + rnd() % 400;              /* cut in the first MCUs */
                if (v == 4) for (int k = 0; k < 6; k++) {        /* bytes changed (no new markers) */
                    u32 at = size / 3 + rnd() % (size / 2); u8 x = (u8)(src[at] ^ (1 + rnd() % 255));
                    if (src[at] != 0xFF && src[at - 1] != 0xFF && x != 0xFF) src[at] = x;
                }
                memset(MEM(DST), 0, 320 * 240 * 2);
                U16(0x87B728) = 0;
                struct timespec t0, t1;
                clock_gettime(CLOCK_MONOTONIC, &t0);
                PcJpeg_Decode(DST, SRC, sz, 2);
                clock_gettime(CLOCK_MONOTONIC, &t1);
                secs += (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) * 1e-9;
                unsigned long long h = fnv(MEM(DST), 320 * 240 * 2, 0xCBF29CE484222325ull);
                h = fnv(MEM(0x87B728), 2, h);
                cases++;
                if (ref && mode[0] == 'w') fprintf(ref, "%s %lX %d %016llX\n", names[f], o, v, h);
                else if (ref) {
                    char nm[256]; long ro; int rv; unsigned long long rh;
                    if (fscanf(ref, "%255s %lX %d %llX", nm, &ro, &rv, &rh) != 4 || strcmp(nm, names[f]) || ro != o || rv != v) {
                        printf("reference out of step at %s +%lX\n", names[f], o); return 1;
                    }
                    if (rh != h) { if (bad++ < 10) printf("%s +%lX case %d: pixels differ\n", names[f], o, v); }
                }
            }
        }
        free(buf);
    }
    printf("%d pictures, %d cases, %.2f ms a picture%s", pics, cases, secs * 1000 / cases, mode[0] == 'c' ? "" : "\n");
    if (mode[0] == 'c') printf(", %d differ\n", bad);
    if (ref) fclose(ref);
    return bad != 0;
}
