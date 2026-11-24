/* The camera backgrounds made ahead of time for the Dreamcast
 * (src/port/dc/bgvq_format.h): every camera picture in the room DBS files of
 * a Data folder decoded by the game's own JPEG decoder (src/pc/jpeg.c, the
 * same pixels the console would make) and turned into the PVR's VQ
 * textures, written to OUT/BGVQ/<NAME>.VQ. Copy that BGVQ folder into the
 * Data folder of the disc (CDI or GDI): the Dreamcast build then reads a
 * camera's picture from there instead of decoding its JPEG (and falls back
 * to the JPEG for any camera not found). The game's files stay yours: this
 * runs on your copy and nothing of it goes into the repository.
 *
 *   make build/dc2_bgvq
 *   build/dc2_bgvq path/to/Data OUT [threads]
 *
 * The VQ: per 256 x 256 piece, 256 codes of 2 x 2 texels found by k-means
 * on the piece's picture blocks (the padding past the picture left out),
 * the codes rounded to RGB565 before the last assignment. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <sys/stat.h>
#include "dc2/types.h"
#include "dc2/gpu.h"
#include "../../src/port/dc/bgvq_format.h"
#include "../../src/port/dc/bgvq_decode.h"

void (*g_crumb)(int c);                   /* (the breadcrumbs: none here) */
u8 *g_membase;
#define DST 0x6C1500u
#define SRC 0x800000u
#define NCODE 256
#define ITERS 12

/* ---- one piece: 256 x 256 texels of rgb (8 bits each), the picture in
   its first valid_w x valid_h; out: codebook + twiddled indices ---- */
typedef struct {
    const u8 *rgb;
    int valid_w, valid_h;
    u8 *out;
    double sq_err;                        /* summed over the picture's texels */
    long texels;
} Piece;

static unsigned spread(unsigned i) { unsigned r = 0; for (int b = 0; b < 8; b++) r |= ((i >> b) & 1u) << (2 * b); return r; }

/* the 2 x 2 block at (bx, by) as 12 values: top left, bottom left, top right, bottom right */
static void block(const u8 *rgb, int bx, int by, float *v)
{
    static const int dx[4] = {0, 0, 1, 1}, dy[4] = {0, 1, 0, 1};
    for (int t = 0; t < 4; t++) {
        const u8 *p = rgb + ((2 * by + dy[t]) * 256 + 2 * bx + dx[t]) * 3;
        v[3 * t] = p[0]; v[3 * t + 1] = p[1]; v[3 * t + 2] = p[2];
    }
}

static float dist(const float *a, const float *b, float best)
{
    float d = 0;
    for (int k = 0; k < 12; k++) { float e = a[k] - b[k]; d += e * e; if (d >= best) return d; }
    return d;
}

static int nearest(const float *v, const float *cb, float *dbest)
{
    int bi = 0; float bd = 3.4e38f;
    for (int c = 0; c < NCODE; c++) { float d = dist(v, cb + 12 * c, bd); if (d < bd) { bd = d; bi = c; } }
    if (dbest) *dbest = bd;
    return bi;
}

static int cmp_luma(const void *a, const void *b)
{
    const float *x = *(const float *const *)a, *y = *(const float *const *)b;
    float lx = 0, ly = 0;
    for (int k = 0; k < 12; k++) { lx += x[k]; ly += y[k]; }
    return lx < ly ? -1 : lx > ly;
}

static u16 to565(const float *p)
{
    int r = (int)(p[0] + 0.5f), g = (int)(p[1] + 0.5f), b = (int)(p[2] + 0.5f);
    r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (u16)(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | (b * 31 + 127) / 255);
}
static void from565(u16 c, float *p)
{
    int r = c >> 11, g = c >> 5 & 63, b = c & 31;
    p[0] = (float)(r << 3 | r >> 2); p[1] = (float)(g << 2 | g >> 4); p[2] = (float)(b << 3 | b >> 2);
}

static void vq_piece(Piece *pc)
{
    int nbw = (pc->valid_w + 1) / 2, nbh = (pc->valid_h + 1) / 2, n = nbw * nbh;
    float *vec = malloc(sizeof(float) * 12 * n), *cb = malloc(sizeof(float) * 12 * NCODE);
    double *acc = malloc(sizeof(double) * 12 * NCODE);
    int *cnt = malloc(sizeof(int) * NCODE), *who = malloc(sizeof(int) * n);
    float *dd = malloc(sizeof(float) * n);
    for (int by = 0, i = 0; by < nbh; by++) for (int bx = 0; bx < nbw; bx++, i++) block(pc->rgb, bx, by, vec + 12 * i);
    /* the start: codes spread over the blocks in order of brightness */
    const float **ord = malloc(sizeof(float *) * n);
    for (int i = 0; i < n; i++) ord[i] = vec + 12 * i;
    qsort(ord, n, sizeof *ord, cmp_luma);
    for (int c = 0; c < NCODE; c++) memcpy(cb + 12 * c, ord[(long)c * n / NCODE + n / (2 * NCODE)], sizeof(float) * 12);
    free(ord);
    for (int it = 0; it < ITERS; it++) {
        memset(acc, 0, sizeof(double) * 12 * NCODE); memset(cnt, 0, sizeof(int) * NCODE);
        for (int i = 0; i < n; i++) {
            int c = nearest(vec + 12 * i, cb, &dd[i]);
            who[i] = c; cnt[c]++;
            for (int k = 0; k < 12; k++) acc[12 * c + k] += vec[12 * i + k];
        }
        for (int c = 0; c < NCODE; c++) {
            if (cnt[c]) { for (int k = 0; k < 12; k++) cb[12 * c + k] = (float)(acc[12 * c + k] / cnt[c]); continue; }
            int w = 0;                                   /* (an empty code: the block worst served) */
            for (int i = 1; i < n; i++) if (dd[i] > dd[w]) w = i;
            memcpy(cb + 12 * c, vec + 12 * w, sizeof(float) * 12);
            dd[w] = 0;
        }
    }
    /* the codes as the PVR has them, then every block of the piece to its nearest */
    u16 *book = (u16 *)pc->out;
    for (int c = 0; c < NCODE; c++)
        for (int t = 0; t < 4; t++) { book[4 * c + t] = to565(cb + 12 * c + 3 * t); from565(book[4 * c + t], cb + 12 * c + 3 * t); }
    u8 *idx = pc->out + BGVQ_CODEBOOK;
    pc->sq_err = 0; pc->texels = 0;
    for (int by = 0; by < 128; by++)
        for (int bx = 0; bx < 128; bx++) {
            float v[12], d;
            block(pc->rgb, bx, by, v);
            int c = nearest(v, cb, &d);
            idx[spread((unsigned)by) | spread((unsigned)bx) << 1] = (u8)c;
            if (2 * bx < pc->valid_w && 2 * by < pc->valid_h) { pc->sq_err += d; pc->texels += 4; }
        }
    free(vec); free(cb); free(acc); free(cnt); free(who); free(dd);
}

/* ---- the pieces of a file shared out to threads ---- */
static Piece *g_work;
static int g_nwork, g_next;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static void *worker(void *a)
{
    (void)a;
    for (;;) {
        pthread_mutex_lock(&g_lock);
        int i = g_next < g_nwork ? g_next++ : -1;
        pthread_mutex_unlock(&g_lock);
        if (i < 0) return NULL;
        vq_piece(&g_work[i]);
    }
}

static void put32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static int cmp_name(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "dc2_bgvq DATA_DIR OUT_DIR [threads]\n  writes OUT_DIR/BGVQ/<room>.VQ: copy the BGVQ folder into the disc's Data folder\n"); return 2; }
    int nthreads = argc > 3 ? atoi(argv[3]) : 4;
    if (nthreads < 1) nthreads = 1;
    g_membase = calloc(1, 0x900000);
    char outdir[1024];
    snprintf(outdir, sizeof outdir, "%s/BGVQ", argv[2]);
    mkdir(argv[2], 0755); mkdir(outdir, 0755);
    char *names[512]; int nn = 0;
    DIR *d = opendir(argv[1]);
    if (!d) { perror(argv[1]); return 2; }
    for (struct dirent *e; (e = readdir(d)) && nn < 512;) {
        size_t l = strlen(e->d_name);
        if (l > 4 && !strcasecmp(e->d_name + l - 4, ".DBS")) names[nn++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, nn, sizeof *names, cmp_name);
    long pics = 0, files = 0, bytes = 0, stp = 0, px = 0;
    double err = 0; long texels = 0;
    double back_err = 0, worst = 99; long back_px = 0;
    char worst_at[300] = "";
    for (int f = 0; f < nn; f++) {
        char path[1400]; snprintf(path, sizeof path, "%s/%s", argv[1], names[f]);
        FILE *fp = fopen(path, "rb"); if (!fp) continue;
        fseek(fp, 0, SEEK_END); long len = ftell(fp); fseek(fp, 0, SEEK_SET);
        u8 *buf = malloc(len > 0 ? len : 1);
        if (fread(buf, 1, len, fp) != (size_t)len) len = 0;
        fclose(fp);
        /* the camera records: a JPEG at +0x1000 of a sector, its size at +0x24 */
        int cap = 64, n = 0;
        u32 *secs = malloc(sizeof(u32) * cap);
        u8 **rgb = malloc(sizeof(u8 *) * cap);
        for (long o = 0; o + 0x1004 < len; o += 0x800) {
            const u8 *j = buf + o + 0x1000;
            u32 size = (u32)buf[o + 0x24] | buf[o + 0x25] << 8 | buf[o + 0x26] << 16 | (u32)buf[o + 0x27] << 24;
            if (j[0] != 0xFF || j[1] != 0xD8 || j[2] != 0xFF || !size || size > 0x40000 || o + 0x1000 + size > len) continue;
            memcpy(MEM(SRC), j, size);
            memset(MEM(DST), 0, 320 * 240 * 2);
            U16(0x87B728) = 0;
            PcJpeg_Decode(DST, SRC, size, 2);
            if (n == cap) { cap *= 2; secs = realloc(secs, sizeof(u32) * cap); rgb = realloc(rgb, sizeof(u8 *) * cap); }
            /* the picture, 512 x 256, its right and bottom edges repeated */
            u8 *img = malloc(512 * 256 * 3);
            for (int y = 0; y < 256; y++)
                for (int x = 0; x < 512; x++) {
                    int sx = x < 320 ? x : 319, sy = y < 240 ? y : 239;
                    u16 p = U16(DST + (sy * 320 + sx) * 2);
                    if (x < 320 && y < 240) { stp += p >> 15; px++; }
                    int r = p & 31, g = p >> 5 & 31, b = p >> 10 & 31;
                    u8 *q = img + (y * 512 + x) * 3;
                    q[0] = (u8)(r << 3 | r >> 2); q[1] = (u8)(g << 3 | g >> 2); q[2] = (u8)(b << 3 | b >> 2);
                }
            secs[n] = (u32)(o / 0x800); rgb[n] = img; n++;
        }
        free(buf);
        if (!n) { free(secs); free(rgb); continue; }
        /* two pieces a picture: columns 0-255 and 256-319 (each in a 256 x 256 texture) */
        u8 **halves = malloc(sizeof(u8 *) * 2 * n);
        g_work = calloc(2 * n, sizeof(Piece));
        u8 *blobs = malloc((size_t)n * BGVQ_BLOB);
        for (int i = 0; i < n; i++)
            for (int h = 0; h < 2; h++) {
                u8 *t = malloc(256 * 256 * 3);
                for (int y = 0; y < 256; y++) memcpy(t + y * 256 * 3, rgb[i] + (y * 512 + 256 * h) * 3, 256 * 3);
                if (h) for (int y = 0; y < 256; y++) for (int x = 64; x < 256; x++) memcpy(t + (y * 256 + x) * 3, t + (y * 256 + 63) * 3, 3);
                halves[2 * i + h] = t;
                g_work[2 * i + h] = (Piece){t, h ? 64 : 256, 240, blobs + (size_t)i * BGVQ_BLOB + h * BGVQ_PIECE, 0, 0};
            }
        g_nwork = 2 * n; g_next = 0;
        pthread_t th[64];
        int nt = nthreads > 64 ? 64 : nthreads;
        for (int t = 0; t < nt; t++) pthread_create(&th[t], NULL, worker, NULL);
        for (int t = 0; t < nt; t++) pthread_join(th[t], NULL);
        for (int i = 0; i < 2 * n; i++) { err += g_work[i].sq_err; texels += g_work[i].texels; free(halves[i]); }
        /* the file */
        char base[256]; size_t l = strlen(names[f]) - 4;
        for (size_t k = 0; k < l && k < 255; k++) base[k] = (char)toupper((unsigned char)names[f][k]);
        base[l < 255 ? l : 255] = 0;
        snprintf(path, sizeof path, "%s/%s.VQ", outdir, base);
        FILE *o = fopen(path, "wb");
        if (!o) { perror(path); return 1; }
        u32 head = 16 + 8 * (u32)n, at = (head + 31) & ~31u;
        u8 *hdr = calloc(1, at);
        memcpy(hdr, BGVQ_MAGIC, 8); put32(hdr + 8, (u32)n); put32(hdr + 12, 0);
        for (int i = 0; i < n; i++) { put32(hdr + 16 + 8 * i, secs[i]); put32(hdr + 20 + 8 * i, at + (u32)i * BGVQ_BLOB); }
        fwrite(hdr, 1, at, o);
        fwrite(blobs, BGVQ_BLOB, (size_t)n, o);
        fclose(o);
        /* read back as the console will (bgvq_decode.h) and measured against the JPEG's pixels */
        o = fopen(path, "rb");
        u8 *back = malloc(BGVQ_BLOB);
        static u16 pic[320 * 240];
        for (int i = 0; i < n; i++) {
            u8 e[8];
            fseek(o, 16 + 8 * i, SEEK_SET);
            if (fread(e, 1, 8, o) != 8) { fprintf(stderr, "%s: cannot be read back\n", path); return 1; }
            u32 sec = e[0] | e[1] << 8 | e[2] << 16 | (u32)e[3] << 24, off = e[4] | e[5] << 8 | e[6] << 16 | (u32)e[7] << 24;
            fseek(o, (long)off, SEEK_SET);
            if (sec != secs[i] || fread(back, 1, BGVQ_BLOB, o) != BGVQ_BLOB) { fprintf(stderr, "%s: cannot be read back\n", path); return 1; }
            bgvq_decode(back, pic, 0);
            double se = 0;
            for (int y = 0; y < 240; y++)
                for (int x = 0; x < 320; x++) {
                    u16 p = pic[y * 320 + x];
                    const u8 *q = rgb[i] + (y * 512 + x) * 3;
                    int c[3] = {p & 31, p >> 5 & 31, p >> 10 & 31};
                    for (int k = 0; k < 3; k++) { double dv = (double)((c[k] << 3 | c[k] >> 2) - q[k]); se += dv * dv; }
                }
            double ps = se > 0 ? 10 * log10(255.0 * 255.0 / (se / (320 * 240 * 3))) : 99;
            back_err += se; back_px += 320 * 240 * 3;
            if (ps < worst) { worst = ps; snprintf(worst_at, sizeof worst_at, "%s sector %u", base, (unsigned)sec); }
        }
        free(back);
        fclose(o);
        bytes += at + (long)n * BGVQ_BLOB; pics += n; files++;
        for (int i = 0; i < n; i++) free(rgb[i]);
        free(hdr); free(blobs); free(g_work); free(halves); free(secs); free(rgb);
        printf("%s: %d pictures\n", base, n);
        fflush(stdout);
    }
    if (stp * 2 > px) {                      /* (bit 15 of the pictures: the flag in each file) */
        for (int f = 0; f < nn; f++) {
            char path[1400], base[256]; size_t l = strlen(names[f]) - 4;
            for (size_t k = 0; k < l && k < 255; k++) base[k] = (char)toupper((unsigned char)names[f][k]);
            base[l < 255 ? l : 255] = 0;
            snprintf(path, sizeof path, "%s/%s.VQ", outdir, base);
            FILE *o = fopen(path, "r+b");
            if (!o) continue;
            u8 fl[4]; put32(fl, 1); fseek(o, 12, SEEK_SET); fwrite(fl, 1, 4, o); fclose(o);
        }
    }
    double mse = texels ? err / texels / 3 : 0, bmse = back_px ? back_err / back_px : 0;
    printf("read back: %.1f dB on average, the worst %.1f dB (%s); bit 15 set in %ld of %ld pixels\n",
           bmse > 0 ? 10 * log10(255.0 * 255.0 / bmse) : 99.0, worst, worst_at, stp, px);
    if (worst < 25) { fprintf(stderr, "a picture reads back wrong: the files are not right\n"); return 1; }
    printf("%ld pictures in %ld files, %.1f MB, %.1f dB PSNR against the decoded JPEGs; copy %s into the disc's Data folder\n",
           pics, files, bytes / 1048576.0, mse > 0 ? 10 * log10(255.0 * 255.0 / mse) : 99.0, outdir);
    return 0;
}
