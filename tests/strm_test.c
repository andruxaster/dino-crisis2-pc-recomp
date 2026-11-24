/* The Dreamcast's streams on the AICA (src/port/dc/aica_strm.c), checked on
 * the host: src/pc/audio.c's fill for a file stream (what a KOS stream asks
 * for, frames at the stream's own rate) gives the same samples, to the same
 * end, as its software mix of that stream at that rate. aica_strm and the
 * AICA's sounds are stand-ins here. make build/strm_test && build/strm_test */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "dc2/types.h"
u8 *g_membase;
typedef u32 (*AicaFill)(void *, s16 *, u32);
static AicaFill g_fill; static void *g_ctx; static int g_allow, g_open, g_close;
int AicaStrm_Open(u32 rate, u32 ch, AicaFill f, void *c, float gl, float gr) { if (!g_allow) return -1; g_fill = f; g_ctx = c; g_open++; return 0; }
void AicaStrm_Gains(int s, float gl, float gr) {}
void AicaStrm_Close(int s) { g_close++; g_fill = NULL; }
void *Aica_Load(const void *p, u32 a, u32 b, u32 c, u32 d) { return NULL; }
void Aica_Unload(void *s) {} void Aica_Play(void *s, u32 f, int l, float a, float b) {} void Aica_Gains(void *s, float a, float b) {}
int Aica_IsPlaying(void *s) { return 0; } void Aica_Stop(void *s) {} void Aica_Pause(void *s) {} void Aica_Resume(void *s) {}
size_t Host_Read(void *p, size_t n, FILE *f) { return fread(p, 1, n, f); }
void *Host_OpenStream(const char *path, size_t buf) { (void)buf; return fopen(path, "rb"); }
void Host_CloseStream(void *f) { if (f) fclose(f); }
static char g_root[256];
int Pc_HostPath(const char *g, char *o, unsigned long n) { snprintf(o, n, "%s/%s", g_root, g); for (char *q = o; *q; q++) if (*q == '\\') *q = '/'; return 1; }
u32 Snd_OpenFileVoice(const char *); void Snd_Play(u32, u32, u32); u32 Strm_OpenFile(const char *, u32, u32); void Strm_Play(u32);
void PcAudio_Init(u32, void *); void PcAudio_Mix(s16 *, u32); u32 Snd_IsPlaying(u32); void Strm_Close(u32);

/* the files the test plays, made up: a 16-bit mono PCM "speech" file and
   an IMA ADPCM stereo music file (random blocks, a short last one) */
static u32 g_seed = 1;
static u32 rnd(void) { g_seed = g_seed * 1103515245u + 12345u; return g_seed >> 8; }
static void put16(FILE *f, u32 v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }
static void put32(FILE *f, u32 v) { put16(f, v & 0xFFFF); put16(f, v >> 16); }
static void wav(const char *name, u32 fmt, u32 ch, u32 align, u32 bits, const u8 *data, u32 n, u32 spb)
{
    char p[512]; snprintf(p, sizeof p, "%s/%s", g_root, name);
    FILE *f = fopen(p, "wb");
    u32 fl = fmt == 0x11 ? 20 : 16;
    fwrite("RIFF", 1, 4, f); put32(f, 4 + 8 + fl + 8 + n); fwrite("WAVEfmt ", 1, 8, f); put32(f, fl);
    put16(f, fmt); put16(f, ch); put32(f, 22050); put32(f, 22050 * align); put16(f, align); put16(f, bits);
    if (fmt == 0x11) { put16(f, 2); put16(f, spb); }
    fwrite("data", 1, 4, f); put32(f, n); fwrite(data, 1, n, f); fclose(f);
}
static void make_files(void)
{
    static u8 buf[64 << 10];
    char p[512];
    snprintf(g_root, sizeof g_root, "/tmp/dc2_strm_test.%d", (int)getpid());
    snprintf(p, sizeof p, "mkdir -p %s/data/BGM", g_root);
    if (system(p)) exit(2);
    for (u32 i = 0; i < 30000; i++) { s16 v = (s16)((s32)(rnd() % 40001) - 20000); buf[2 * i] = (u8)v; buf[2 * i + 1] = (u8)((u16)v >> 8); }
    wav("SP0001.DAT", 1, 1, 2, 16, buf, 60000, 0);
    u32 n = 0;
    for (u32 b = 0; b < 37; b++)
        for (u32 i = 0; i < 1024; i++) buf[n++] = i < 8 ? (i % 4 == 2 ? rnd() % 61 : i % 4 == 3 ? 0 : (u8)rnd()) : (u8)rnd();
    wav("data/BGM/AB.WAV", 0x11, 2, 1024, 4, buf, n - 300, (1024 / 2 - 4) * 2 + 1);
    snprintf(p, sizeof p, "%s/data/BGM/INDEX.TXT", g_root);
    FILE *f = fopen(p, "w"); fputs("MS_0001 3 AB\n", f); fclose(f);
}
static s16 ref[2][2 * 400 * 512], got[2 * 400 * 512];
static u32 run_ref(int strm, u32 ch) {   /* the software mix, at the stream's own rate */
    g_allow = 0; u32 h;
    if (strm) { h = Strm_OpenFile("MS_0001.DAT", 3, 0); Strm_Play(h); } else { Snd_OpenFileVoice("SP0001.DAT"); Snd_Play(0, 0, 0); }
    u32 n = 0; s16 lr[2 * 512];
    for (int i = 0; i < 400; i++) { PcAudio_Mix(lr, 512); for (int k = 0; k < 512; k++) for (u32 c = 0; c < ch; c++) ref[strm][(n + k) * ch + c] = lr[2 * k + c]; n += 512; }
    if (strm) Strm_Close(h);
    return n;
}
int main(void)
{
    make_files();
    PcAudio_Init(22050, NULL);
    int fails = 0;
    for (int strm = 0; strm < 2; strm++) {
        u32 ch = strm ? 2 : 1;
        run_ref(strm, ch);
        g_allow = 1; u32 h = 0;
        if (strm) { h = Strm_OpenFile("MS_0001.DAT", 3, 0); Strm_Play(h); } else { Snd_OpenFileVoice("SP0001.DAT"); Snd_Play(0, 0, 0); }
        u32 n = 0, k; int calls = 0;
        while (g_fill && (k = g_fill(g_ctx, got + n * ch, 300 + (calls++ * 37) % 900))) n += k;
        /* the reference's samples up to where it fell silent */
        u32 m = 0; for (u32 i = 0; i < 400 * 512; i++) { int nz = 0; for (u32 c = 0; c < ch; c++) nz |= ref[strm][i * ch + c] != 0; if (nz) m = i + 1; }
        int diff = 0; for (u32 i = 0; i < n * ch; i++) if (got[i] != ref[strm][i]) { if (!diff) printf("  first difference at %u\n", i); diff++; }
        printf("%s: streamed %u frames (the mix: %u), %d differences, playing %u, opened %d closed %d\n", strm ? "IMA stream" : "PCM voice", n, m, diff,
               strm ? 0 : Snd_IsPlaying(0), g_open, g_close);
        fails += diff || n != m;
        if (strm) Strm_Close(h);
    }
    { char p[300]; snprintf(p, sizeof p, "rm -rf %s", g_root); if (system(p)) {} }
    printf(fails ? "FAILED\n" : "ok\n");
    return fails != 0;
}
