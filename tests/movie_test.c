/* movie_test -- the standalone movies (src/pc/movie.c) checked in numbers:
 * a movie opened, run and polled frame by frame like Movie_StepPlay does,
 * with a renderer that records what is drawn. Prints per polled frame the
 * quad drawn (screen rectangle) and the texture's version, and writes the
 * frames shown (raw BGRA, w x h each) and the sound mixed (s16 stereo
 * 44.1 kHz) to files, to be compared with another decoder's output.
 *   usage: movie_test ROOT NAME RATE X Y FRAMES OUT.bgra OUT.pcm
 *   (NAME as in the game's table, e.g. OPEN_30; RATE: 0x87DE00, 2 or 4) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dc2/types.h"
#include "dc2/pcio.h"
#include "dc2/render.h"
#include "dc2/data.h"

u8 *g_membase;
static FILE *g_vout;
static u32 g_last_version, g_draws;
static float g_rect[4];

static void r_draw(int prim, const RVertex *v, int n, const RState *s, const RTexture *t)
{
    (void)prim; (void)s;
    if (!t || n != 4) return;
    g_draws++;
    g_rect[0] = v[0].x; g_rect[1] = v[0].y; g_rect[2] = v[3].x; g_rect[3] = v[3].y;
    if (t->version != g_last_version) {
        g_last_version = t->version;
        fwrite(t->pixels, 1, (size_t)t->w * t->h * 4, g_vout);
    }
}
static void r_nop(void) {}
static void r_clear(int a, int b, u32 c) { (void)a; (void)b; (void)c; }
static void r_read_back(u16 *d, u32 w, u32 h, u32 p) { (void)w; memset(d, 0, (size_t)h * p); }
static void r_freed(u32 id) { (void)id; }
static const Renderer k_rec = {r_nop, r_clear, r_draw, r_nop, r_read_back, r_freed};

static FILE *g_aout;
static void sink(const s16 *lr, u32 frames) { fwrite(lr, 4, frames, g_aout); }

int main(int argc, char **argv)
{
    if (argc < 9) { fprintf(stderr, "usage: %s ROOT NAME RATE X Y FRAMES OUT.bgra OUT.pcm\n", argv[0]); return 1; }
    g_membase = calloc(1, IMAGE_SIZE + TMP_SIZE + HEAP_SIZE);
    Data_Init();
    g_pc_root = argv[1];
    PcDraw_InitStandalone(&k_rec, 640, 480);
    g_vout = fopen(argv[7], "wb"); g_aout = fopen(argv[8], "wb");
    PcAudio_Init(44100, sink);
    U32(0x87DE00) = (u32)atoi(argv[3]);
    gptr name = IMAGE_BASE + IMAGE_SIZE;            /* (the TMP area) */
    snprintf((char *)MEM(name), 64, "movie\\%s.dat", argv[2]);
    u32 r = Movie_Open(name, (u32)atoi(argv[4]), (u32)atoi(argv[5]));
    printf("open %u\n", r);
    if (r) return 2;
    Movie_Ctl1();
    int frames = atoi(argv[6]), f;
    u32 shown = 0;
    for (f = 0; f < frames; f++) {
        u32 v0 = g_last_version;
        g_draws = 0;
        u32 end = Movie_Poll();
        PcAudio_Frame(U32(0x87DE00));
        if (g_last_version != v0) shown++;
        if (f < 3 || end) printf("poll %d: end %u draws %u rect %.0f,%.0f %.0f,%.0f version %u\n", f, end, g_draws,
                                 g_rect[0], g_rect[1], g_rect[2], g_rect[3], g_last_version);
        if (end) break;
    }
    printf("polls %d, frames shown %u\n", f, shown);
    fclose(g_vout); fclose(g_aout);
    return 0;
}
