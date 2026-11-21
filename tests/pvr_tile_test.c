/* Host test of src/port/dc/pvr_tile.h (the PVR's large textures in pieces):
 *   make pvr-tile-test
 * The split covers each side once, in powers of two, saving VRAM; a
 * triangle cut to every piece covers the same screen area, and at points
 * inside each cut piece the texture coordinate (perspective-correct) is the
 * whole triangle's there, inside that piece. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pvr_tile.h"
#include "pvr_twiddle.h"

static int fails;
#define FAIL(...) do { if (fails++ < 20) printf(__VA_ARGS__); } while (0)

static double frand(void) { return rand() / (double)RAND_MAX; }

static double area(const TileV *p, int n)
{
    double a = 0;
    for (int i = 0; i < n; i++) { const TileV *q = &p[i], *r = &p[(i + 1) % n]; a += (double)q->x * r->y - (double)r->x * q->y; }
    return fabs(a) / 2;
}

/* the whole triangle's uv at screen point (x, y): barycentric on the screen,
   then uw / w, vw / w */
static int tri_uv(const TileV *t, double x, double y, double *u, double *v)
{
    double d = (t[1].y - t[2].y) * (t[0].x - t[2].x) + (t[2].x - t[1].x) * (t[0].y - t[2].y);
    if (fabs(d) < 1e-9) return 0;
    double l0 = ((t[1].y - t[2].y) * (x - t[2].x) + (t[2].x - t[1].x) * (y - t[2].y)) / d;
    double l1 = ((t[2].y - t[0].y) * (x - t[2].x) + (t[0].x - t[2].x) * (y - t[2].y)) / d;
    double l2 = 1 - l0 - l1;
    double w = l0 * t[0].w + l1 * t[1].w + l2 * t[2].w;
    *u = (l0 * t[0].uw + l1 * t[1].uw + l2 * t[2].uw) / w;
    *v = (l0 * t[0].vw + l1 * t[1].vw + l2 * t[2].vw) / w;
    return 1;
}

int main(void)
{
    /* the split */
    unsigned long long full = 0, split = 0;
    for (unsigned n = 1; n <= 1024; n++) {
        unsigned short off[TILE_MAX + 1], size[TILE_MAX];
        int k = tile_split(n, off, size);
        if (k < 1 || k > TILE_MAX || off[0] != 0 || off[k] != n) FAIL("split %u: %d pieces, ends %u\n", n, k, off[k]);
        unsigned sum = 0;
        for (int i = 0; i < k; i++) {
            unsigned len = off[i + 1] - off[i];
            if (size[i] < 8 || (size[i] & (size[i] - 1))) FAIL("split %u: piece %d size %u\n", n, i, size[i]);
            if (len > size[i] || (i < k - 1 && len != size[i])) FAIL("split %u: piece %d %u texels in %u\n", n, i, len, size[i]);
            sum += size[i];
        }
        if (sum > tile_pow2(n)) FAIL("split %u: %u in VRAM, more than whole %u\n", n, sum, tile_pow2(n));
        full += tile_pow2(n); split += sum;
    }
    {
        unsigned short off[4], size[3];
        int k = tile_split(640, off, size);
        if (k != 2 || size[0] != 512 || size[1] != 128) FAIL("640: %d pieces %u %u\n", k, size[0], size[1]);
        k = tile_split(480, off, size);
        if (k != 1 || size[0] != 512) FAIL("480: %d pieces %u\n", k, size[0]);
    }
    printf("split: sides 1..1024 take %.1f%% of the padded VRAM\n", 100.0 * split / full);

    /* the cut */
    srand(7);
    int tris = 0, cut = 0;
    for (int it = 0; it < 20000; it++) {
        TileV t[3];
        int persp = it % 2;
        for (int i = 0; i < 3; i++) {
            double u = frand() * 1.2 - 0.1, v = frand() * 1.2 - 0.1, w = persp ? 0.05 + frand() * 2 : 1.0;
            if (it % 7 == 0) { u = i == 1 ? 1.0 : 0.0; v = i == 2 ? 1.0 : 0.0; }     /* (a screen quad's half) */
            t[i].x = (float)(frand() * 640); t[i].y = (float)(frand() * 480); t[i].w = (float)w;
            t[i].uw = (float)(u * w); t[i].vw = (float)(v * w);
            for (int c = 0; c < 4; c++) t[i].c[c] = (float)frand();
            t[i].fog = (float)frand();
        }
        double whole = area(t, 3);
        if (whole < 1) continue;
        tris++;
        /* pieces of a 640 x 480 texture, the texture's edges at 0 and 1 widened to take uv outside */
        const float ue[4] = {-1e9f, 512.0f / 640, 1e9f}, ve[3] = {-1e9f, 1e9f};
        double sum = 0;
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 1; b++) {
                TileV out[8], tmp[8];
                int n = tile_clip(t, ue[a], ue[a + 1], ve[b], ve[b + 1], out, tmp);
                if (!n) continue;
                if (n > 7) FAIL("cut: %d vertices\n", n);
                cut++;
                sum += area(out, n);
                for (int s = 0; s < 4; s++) {        /* points inside the piece: its uv the triangle's, in the piece */
                    double l[8], tot = 0;
                    for (int i = 0; i < n; i++) tot += l[i] = frand();
                    double x = 0, y = 0, w = 0, uw = 0, vw = 0;
                    for (int i = 0; i < n; i++) { double f = l[i] / tot; x += f * out[i].x; y += f * out[i].y; w += f * out[i].w; uw += f * out[i].uw; vw += f * out[i].vw; }
                    double u, v;
                    if (!tri_uv(t, x, y, &u, &v)) continue;
                    double pu = uw / w, pv = vw / w;
                    double tol = 1e-3 * (1 + fabs(u) + fabs(v));
                    if (fabs(pu - u) > tol || fabs(pv - v) > tol) FAIL("cut %d: uv %f %f, whole %f %f\n", it, pu, pv, u, v);
                    if (pu < ue[a] - tol || pu > ue[a + 1] + tol) FAIL("cut %d: u %f outside piece %d\n", it, pu, a);
                }
            }
        if (fabs(sum - whole) > 1e-3 * whole + 0.05) FAIL("cut %d: pieces' area %f, the triangle's %f\n", it, sum, whole);
    }
    printf("cut: %d triangles into %d pieces\n", tris, cut);
    /* the twiddled order against KallistiOS's formula (pvr_txr_load_ex, 16 bpp) */
    {
#define TWIDTAB(x) ( (x&1)|((x&2)<<1)|((x&4)<<2)|((x&8)<<3)|((x&16)<<4)| \
                     ((x&32)<<5)|((x&64)<<6)|((x&128)<<7)|((x&256)<<8)|((x&512)<<9) )
#define TWIDOUT(x, y) ( TWIDTAB((y)) | (TWIDTAB((x)) << 1) )
        static unsigned short src[1024 * 1024], ours[1024 * 1024], kos[1024 * 1024];
        int sizes = 0;
        for (unsigned w = 8; w <= 1024; w <<= 1)
            for (unsigned h = 8; h <= 1024; h <<= 1) {
                if (w * h > 1024 * 512) continue;
                unsigned pitch = w + (w & 16);                     /* (a pitch wider than the texture too) */
                for (unsigned i = 0; i < pitch * h; i++) src[i] = (unsigned short)(i * 2654435761u >> 7);
                unsigned min = w < h ? w : h, mask = min - 1;
                for (unsigned y = 0; y < h; y++)
                    for (unsigned x = 0; x < w; x++)
                        kos[TWIDOUT(x & mask, y & mask) + (x / min + y / min) * min * min] = src[y * pitch + x];
                pvr_twiddle16(src, pitch, w, h, ours);
                if (memcmp(ours, kos, w * h * 2)) FAIL("twiddle %u x %u (pitch %u) differs from KallistiOS's\n", w, h, pitch);
                sizes++;
            }
        printf("twiddle: %d sizes as KallistiOS's\n", sizes);
    }
    printf(fails ? "%d FAILED\n" : "ok\n", fails);
    return fails != 0;
}
