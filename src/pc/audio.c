/* The PC port's sound (0x4BF720..0x4C0090: DirectSound buffers, MP3
 * streams) for the standalone builds: a mixer of the game's sound buffers
 * and streams into stereo s16. The port calls PcAudio_Frame after each frame
 * of play; the samples of that frame go to its sink (none: they are mixed
 * and dropped, so sounds still take their time and end).
 *
 * Buffers (Snd_*): RIFF WAVE PCM copied at creation (the bank's memory is
 * the game's); handle 0 is the voice's. Volume as the PC layer sets it:
 * v / 127 * 3000 - 3000 hundredths of a dB (streams: * 10000 - 10000), pan
 * p / 64 * 10000 - 10000; a frequency other than 0 replays at that rate.
 * Streams (Strm_*): MPEG-1 layer III data (the room's music) decoded whole
 * at Strm_Open (minimp3), looping when opened so. */
#ifndef DC2_IN_PROCESS
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "dc2/types.h"
#include "dc2/pcio.h"
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "../../third_party/minimp3/minimp3.h"
#include <stdio.h>

static int g_log = -1;   /* DC2_SNDLOG: the calls on stderr */
#define SLOG(...) do { if (g_log < 0) g_log = getenv("DC2_SNDLOG") != NULL; if (g_log) fprintf(stderr, __VA_ARGS__); } while (0)

#define NBUF 0x400
#define NSTRM 16

typedef struct {
    s16 *pcm;           /* frames x channels */
    u32 frames, ch, rate;
    u32 freq;           /* playing rate (0: its own) */
    double pos;         /* in frames */
    u8 used, playing, loop, paused;
    float gl, gr;       /* volume and pan */
    s32 vol, pan;       /* hundredths of a dB */
} Voice;

static Voice g_buf[NBUF], g_strm[NSTRM];
static u32 g_rate = 44100;
static void (*g_sink)(const s16 *lr, u32 frames);
static double g_carry;

static float db_gain(s32 cb) { return cb <= -10000 ? 0.0f : (float)pow(10.0, cb / 2000.0); }
static void gains(Voice *v)
{
    float g = db_gain(v->vol);
    v->gl = g * (v->pan > 0 ? db_gain(-v->pan) : 1.0f);
    v->gr = g * (v->pan < 0 ? db_gain(v->pan) : 1.0f);
}
static s32 clamp_cb(s32 v, s32 lo, s32 hi) { return v < lo ? lo : v > hi ? hi : v; }
/* v / 127 * range - range, as the PC layer's float to int (towards 0) */
static s32 cb_of(s32 v, float range) { return (s32)((float)v * 0.007874016f * range - range); }

static void voice_free(Voice *v) { free(v->pcm); memset(v, 0, sizeof *v); }

/* ---- buffers ---- */

u32 DC2CALL Snd_CreateBuffer(gptr w, u32 voice)
{
    u32 h = 0;
    if (!voice) {
        for (h = 1; h < NBUF && g_buf[h].used; h++) {}
        if (h == NBUF) return 0xFFFFFFFF;
    }
    Voice *v = &g_buf[h];
    voice_free(v);
    u32 ch = U16(w + 0x16), rate = U32(w + 0x18), bits = U16(w + 0x22);
    u32 c = w + 0x14 + U32(w + 0x10), size = 0, data = 0;
    for (int k = 0; k < 16; k++, c += 8 + U32(c + 4))
        if (!memcmp(MEM(c), "data", 4)) { size = U32(c + 4); data = c + 8; break; }
    if (!ch) ch = 1;
    u32 bytes = bits == 8 ? 1 : 2, n = size / (bytes * ch);
    v->pcm = malloc((size_t)(n ? n : 1) * ch * sizeof(s16));
    for (u32 i = 0; i < n * ch; i++)
        v->pcm[i] = bytes == 1 ? (s16)((U8(data + i) - 0x80) << 8) : S16(data + 2 * i);
    v->frames = n; v->ch = ch; v->rate = rate ? rate : 22050;
    v->used = 1; v->vol = 0; v->pan = 0; gains(v);
    return h;
}

void DC2CALL Snd_Play(u32 h, u32 freq, u32 loop)
{
    if (h >= NBUF || !g_buf[h].used) return;
    Voice *v = &g_buf[h];
    if (freq) v->freq = freq;
    v->pos = 0; v->playing = 1; v->paused = 0; v->loop = loop != 0;
    SLOG("snd %u play freq %u loop %u (%u frames at %u)\n", h, freq, loop, v->frames, v->rate);
}
void DC2CALL Snd_SetVolume(u32 h, s32 vol)
{
    if (h >= NBUF || !g_buf[h].used) return;
    g_buf[h].vol = clamp_cb(cb_of(vol, 3000.0f), -10000, 0); gains(&g_buf[h]);
    SLOG("snd %u volume %d (%d)\n", h, vol, g_buf[h].vol);
}
void DC2CALL Snd_SetPan(u32 h, u32 pan)
{
    if (h >= NBUF || !g_buf[h].used) return;
    g_buf[h].pan = clamp_cb((s32)((float)(s32)pan * 0.015625f * 10000.0f - 10000.0f), -10000, 10000); gains(&g_buf[h]);
}
u32 DC2CALL Snd_IsPlaying(u32 h) { return h < NBUF && g_buf[h].used && g_buf[h].playing; }
void DC2CALL Snd_Stop(u32 h) { if (h < NBUF) g_buf[h].playing = 0; }
void DC2CALL Snd_Free(u32 h) { if (h < NBUF) voice_free(&g_buf[h]); }
void DC2CALL Snd_Pause(u32 h) { if (h < NBUF && g_buf[h].playing) { g_buf[h].playing = 0; g_buf[h].paused = 1; } }
void DC2CALL Snd_Resume(u32 h) { if (h < NBUF && g_buf[h].paused) { g_buf[h].paused = 0; g_buf[h].playing = 1; } }

/* ---- streams ---- */

u32 DC2CALL Strm_Open(gptr mp3, u32 size, u32 loop)
{
    u32 h = 0;
    while (h < NSTRM && g_strm[h].used) h++;
    if (h == NSTRM) return 0xFFFFFFFF;
    Voice *v = &g_strm[h];
    mp3dec_t dec;
    mp3dec_init(&dec);
    s16 frame[MINIMP3_MAX_SAMPLES_PER_FRAME];
    const u8 *p = MEM(mp3);
    size_t cap = 1 << 20, n = 0;
    s16 *pcm = malloc(cap * 2 * sizeof(s16));
    u32 rate = 44100;
    for (int left = (int)size; left > 0;) {
        mp3dec_frame_info_t fi;
        int got = mp3dec_decode_frame(&dec, p, left, frame, &fi);
        if (!fi.frame_bytes) break;
        p += fi.frame_bytes; left -= fi.frame_bytes;
        if (!got) continue;
        if (fi.hz) rate = (u32)fi.hz;
        if (n + (size_t)got > cap) { cap = (n + got) * 2; pcm = realloc(pcm, cap * 2 * sizeof(s16)); }
        for (int i = 0; i < got; i++) {             /* as stereo */
            pcm[2 * (n + i)] = frame[fi.channels == 2 ? 2 * i : i];
            pcm[2 * (n + i) + 1] = frame[fi.channels == 2 ? 2 * i + 1 : i];
        }
        n += (size_t)got;
    }
    v->pcm = pcm; v->frames = (u32)n; v->ch = 2; v->rate = rate;
    v->used = 1; v->loop = loop != 0; v->vol = 0; v->pan = 0; gains(v);
    SLOG("strm %u open %u bytes: %u frames at %u loop %u\n", h, size, v->frames, rate, loop);
    return h;
}
void DC2CALL Strm_Play(u32 h) { SLOG("strm %u play\n", h); if (h < NSTRM && g_strm[h].used) { g_strm[h].playing = 1; g_strm[h].paused = 0; } }
void DC2CALL Strm_Stop(u32 h) { if (h < NSTRM) g_strm[h].playing = 0; }
void DC2CALL Strm_SetVolume(u32 h, s32 vol)
{
    if (h >= NSTRM || !g_strm[h].used) return;
    g_strm[h].vol = clamp_cb(cb_of(vol, 10000.0f), -10000, 0); gains(&g_strm[h]);
    SLOG("strm %u volume %d (%d)\n", h, vol, g_strm[h].vol);
}
void DC2CALL Strm_Close(u32 h) { if (h < NSTRM) voice_free(&g_strm[h]); }
void DC2CALL Strm_Pause(u32 h) { if (h < NSTRM && g_strm[h].playing) { g_strm[h].playing = 0; g_strm[h].paused = 1; } }
void DC2CALL Strm_Resume(u32 h) { if (h < NSTRM && g_strm[h].paused) { g_strm[h].paused = 0; g_strm[h].playing = 1; } }

/* ---- the mix ---- */

static void mix_voice(Voice *v, float *acc, u32 frames)
{
    if (!v->playing || !v->frames) { if (v->playing) v->playing = 0; return; }
    double step = (double)(v->freq ? v->freq : v->rate) / g_rate;
    for (u32 i = 0; i < frames; i++) {
        if (v->pos >= v->frames) {
            if (!v->loop) { v->playing = 0; return; }
            v->pos -= v->frames;
        }
        u32 k = (u32)v->pos;
        float l, r;
        if (v->ch == 2) { l = v->pcm[2 * k]; r = v->pcm[2 * k + 1]; }
        else l = r = v->pcm[k];
        acc[2 * i] += l * v->gl; acc[2 * i + 1] += r * v->gr;
        v->pos += step;
    }
}

void PcAudio_Mix(s16 *lr, u32 frames)
{
    static float acc[2 * 8192];
    while (frames) {
        u32 n = frames > 8192 ? 8192 : frames;
        memset(acc, 0, 2 * n * sizeof(float));
        for (u32 h = 0; h < NBUF; h++) if (g_buf[h].used) mix_voice(&g_buf[h], acc, n);
        for (u32 h = 0; h < NSTRM; h++) if (g_strm[h].used) mix_voice(&g_strm[h], acc, n);
        for (u32 i = 0; i < 2 * n; i++) {
            float s = acc[i];
            lr[i] = (s16)(s > 32767.0f ? 32767 : s < -32768.0f ? -32768 : (s32)s);
        }
        lr += 2 * n; frames -= n;
    }
}

void PcAudio_Init(u32 rate, void (*sink)(const s16 *lr, u32 frames)) { g_rate = rate ? rate : 44100; g_sink = sink; }

void PcAudio_Frame(u32 sixtieths)
{
    static s16 out[2 * 8192];
    g_carry += (double)g_rate * (sixtieths ? sixtieths : 1) / 60.0;
    u32 n = (u32)g_carry;
    g_carry -= n;
    if (n > 8192) n = 8192;
    PcAudio_Mix(out, n);
    if (g_sink) g_sink(out, n);
}
#endif
