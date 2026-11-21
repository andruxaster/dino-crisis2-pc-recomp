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
 * at Strm_Open (minimp3), looping when opened so. A source (the movie's
 * sound, src/pc/movie.c) is added in at full volume. */
#ifndef DC2_IN_PROCESS
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "mix_tap.h"
#include "dc2/types.h"
#include "dc2/pcio.h"
/* the mixer's positions: double on the PC, float on the SH-4 (its doubles are slow) */
#if defined(__SH4__) || defined(__sh__)
typedef float xreal_pos;
#else
typedef double xreal_pos;
#endif
/* the Dreamcast mixes in its sound thread (pulled by the stream): every
   call into this file holds a recursive lock there; none on the PC */
#if defined(__DREAMCAST__)
#include <kos/mutex.h>
static mutex_t g_amx = RECURSIVE_MUTEX_INITIALIZER;
#define ALOCK()   mutex_lock(&g_amx)
#define AUNLOCK() mutex_unlock(&g_amx)
#else
#define ALOCK()   ((void)0)
#define AUNLOCK() ((void)0)
#endif
#if defined(DC2_AICA)
#include "../port/dc/aica_sfx.h"
#endif
#define MINIMP3_IMPLEMENTATION
#include "../../third_party/minimp3/minimp3.h"
#include <stdio.h>

static int g_log = -1;   /* DC2_SNDLOG: the calls on stderr */
#define SLOG(...) do { if (g_log < 0) g_log = getenv("DC2_SNDLOG") != NULL; if (g_log) fprintf(stderr, __VA_ARGS__); } while (0)

#define NBUF 0x400
#define NSTRM 16

typedef struct {
    FILE *f;            /* a stream played from a file (Strm_OpenFile): IMA ADPCM WAV */
    u32 data_off, data_len, data_read, align, spb, fmt, bits;   /* fmt 0x11 IMA ADPCM, 1 PCM (spb: frames a block) */
    u8 *blk;            /* one block of the file */
    s16 *pcm;           /* frames x channels (a file stream: the block decoded) */
    u32 frames, ch, rate;
    u32 freq;           /* playing rate (0: its own) */
    xreal_pos pos;         /* in frames */
    u8 used, playing, loop, paused;
    u8 borrowed;        /* pcm points into game memory (a sound bank), not owned */
    float gl, gr;       /* volume and pan */
    s32 vol, pan;       /* hundredths of a dB */
#if defined(DC2_AICA)
    AicaSnd *hw;        /* played by the AICA's channels (a bank's sound), not mixed here */
#endif
} Voice;

static Voice g_buf[NBUF], g_strm[NSTRM];
static u32 g_rate = 44100;
static void (*g_sink)(const s16 *lr, u32 frames);
static xreal_pos g_carry;
static u32 (*g_source)(float *lr, u32 frames);

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

static void voice_free(Voice *v)
{
#if defined(DC2_AICA)
    Aica_Unload(v->hw);
#endif
    if (v->f) fclose(v->f);
    free(v->blk);
    if (!v->borrowed) free(v->pcm);
    memset(v, 0, sizeof *v);
}

/* ---- IMA ADPCM (WAV format 0x11): blocks of {s16 sample, u8 index, 0} per
   channel, then groups of 4 bytes per channel, 8 samples each, low nibble
   first ---- */
static const s16 k_ima_step[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060,
    1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const s8 k_ima_index[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
static s16 ima_nibble(s32 *pred, s32 *idx, u32 nib)
{
    s32 step = k_ima_step[*idx], d = step >> 3;
    if (nib & 4) d += step;
    if (nib & 2) d += step >> 1;
    if (nib & 1) d += step >> 2;
    *pred += nib & 8 ? -d : d;
    if (*pred > 32767) *pred = 32767; else if (*pred < -32768) *pred = -32768;
    *idx += k_ima_index[nib & 15];
    if (*idx < 0) *idx = 0; else if (*idx > 88) *idx = 88;
    return (s16)*pred;
}
/* a block of n bytes into frames (interleaved by ch); the frames decoded */
u32 PcAudio_ImaBlock(const u8 *b, u32 n, u32 ch, s16 *out, u32 spb)
{
    s32 pred[2], idx[2];
    if (n < 4 * ch) return 0;
    for (u32 c = 0; c < ch; c++) {
        pred[c] = (s16)(b[4 * c] | b[4 * c + 1] << 8);
        idx[c] = b[4 * c + 2] > 88 ? 88 : b[4 * c + 2];
        out[c] = (s16)pred[c];
    }
    u32 frames = 1;
    const u8 *p = b + 4 * ch, *end = b + n;
    while (p + 4 * ch <= end && frames + 8 <= spb) {
        for (u32 c = 0; c < ch; c++, p += 4)
            for (u32 k = 0; k < 8; k++)
                out[(frames + k) * ch + c] = ima_nibble(&pred[c], &idx[c], (p[k >> 1] >> ((k & 1) * 4)) & 15);
        frames += 8;
    }
    return frames;
}
/* the next block of a file stream into v->pcm; 0 at the end of the data */
static int file_next_block(Voice *v)
{
    if (v->data_read >= v->data_len) {
        if (!v->loop) return 0;
        fseek(v->f, (long)v->data_off, SEEK_SET);
        v->data_read = 0;
    }
    u32 blk = v->fmt == 1 ? v->spb * v->align : v->align;
    u32 n = v->data_len - v->data_read < blk ? v->data_len - v->data_read : blk;
    if (Host_Read(v->blk, n, v->f) != n) {      /* (the next try from this block's start, not mid-block) */
        clearerr(v->f);
        fseek(v->f, (long)(v->data_off + v->data_read), SEEK_SET);
        return 0;
    }
    v->data_read += n;
    if (v->fmt == 1) {                          /* PCM: 16-bit little-endian or 8-bit unsigned */
        u32 k = n / v->align * v->ch;
        for (u32 i = 0; i < k; i++)
            v->pcm[i] = v->bits == 8 ? (s16)((v->blk[i] - 0x80) << 8) : (s16)(v->blk[2 * i] | v->blk[2 * i + 1] << 8);
        v->frames = n / v->align;
    } else v->frames = PcAudio_ImaBlock(v->blk, n, v->ch, v->pcm, v->spb);
    return v->frames != 0;
}

/* a WAV file (IMA ADPCM, or PCM when pcm_ok) set up in v to be played from
   the file a block at a time; 0 when it is not one */
static int file_voice(Voice *v, FILE *f, u32 loop, int pcm_ok)
{
    u8 hd[12];
    u32 ch = 0, rate = 0, align = 0, fmt = 0, bits = 0, data_off = 0, data_len = 0;
    if (Host_Read(hd, 12, f) == 12 && !memcmp(hd, "RIFF", 4) && !memcmp(hd + 8, "WAVE", 4)) {
        u8 c[8];
        while (Host_Read(c, 8, f) == 8) {
            u32 n = (u32)(c[4] | c[5] << 8 | c[6] << 16 | (u32)c[7] << 24);
            if (!memcmp(c, "fmt ", 4)) {
                u8 fm[16];
                if (n < 16 || Host_Read(fm, 16, f) != 16) break;
                fmt = (u32)(fm[0] | fm[1] << 8); ch = (u32)(fm[2] | fm[3] << 8);
                rate = (u32)(fm[4] | fm[5] << 8 | fm[6] << 16 | (u32)fm[7] << 24); align = (u32)(fm[12] | fm[13] << 8);
                bits = (u32)(fm[14] | fm[15] << 8);
                fseek(f, (long)(n - 16 + (n & 1)), SEEK_CUR);
            } else if (!memcmp(c, "data", 4)) { data_off = (u32)ftell(f); data_len = n; break; }
            else fseek(f, (long)(n + (n & 1)), SEEK_CUR);
        }
    }
    if ((ch != 1 && ch != 2) || !rate || !data_off) return 0;
    if (fmt == 0x11 && align >= 4 * ch) {
        v->spb = (align / ch - 4) * 2 + 1;
        v->blk = malloc(align);
    } else if (fmt == 1 && pcm_ok && (bits == 8 || bits == 16) && align == ch * bits / 8) {
        v->spb = 2048;                          /* frames read at a time */
        v->blk = malloc(v->spb * align);
    } else return 0;
    v->pcm = malloc((size_t)v->spb * ch * sizeof(s16));
    v->f = f; v->fmt = fmt; v->bits = bits; v->data_off = data_off; v->data_len = data_len; v->align = align;
    v->ch = ch; v->rate = rate; v->frames = 0; v->pos = 0; v->data_read = 0;
    v->used = 1; v->loop = loop != 0; v->vol = 0; v->pan = 0; gains(v);
    return 1;
}

/* small-memory builds: the voice (Speech\NNNN.DAT, a PCM wave of up to
   1.5 MB) played from the disc as the voice buffer (handle 0) rather than
   read whole into the game's heap and copied; 0, or 0xFFFFFFFF when the
   file is missing or not a wave */
static u32 locked_Snd_OpenFileVoice(const char *game_path)
{
    char path[512];
    Pc_HostPath(game_path, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) return 0xFFFFFFFF;
    Voice *v = &g_buf[0];
    voice_free(v);
    if (!file_voice(v, f, 0, 1)) { fclose(f); memset(v, 0, sizeof *v); return 0xFFFFFFFF; }
    SLOG("voice %s: %u Hz, %u ch, %u bits, %u bytes\n", path, (unsigned)v->rate, (unsigned)v->ch, (unsigned)v->bits, (unsigned)v->data_len);
    return 0;
}

/* ---- buffers ---- */

static u32 locked_Snd_CreateBuffer(gptr w, u32 voice)
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
#if defined(DC2_AICA)
    /* the Dreamcast: into sound RAM, played by the AICA (none of it here) */
    if (n && (v->hw = Aica_Load(MEM(data), n, ch, bytes * 8, rate))) {
        v->pcm = NULL; v->borrowed = 1;
        v->frames = n; v->ch = ch; v->rate = rate ? rate : 22050;
        v->used = 1; v->vol = 0; v->pan = 0; gains(v);
        return h;
    }
#endif
#if defined(DC2_LOWMEM)
    /* small-memory builds: 16-bit sounds play from the bank in game memory
       (freed after its buffers, Snd_FreeBank) rather than from a copy */
    if (bytes == 2 && !(data & 1) && n) {
        v->pcm = (s16 *)MEM(data); v->borrowed = 1;
    } else
#endif
    {
        v->pcm = malloc((size_t)(n ? n : 1) * ch * sizeof(s16));
        for (u32 i = 0; i < n * ch; i++)
            v->pcm[i] = bytes == 1 ? (s16)((U8(data + i) - 0x80) << 8) : S16(data + 2 * i);
    }
    v->frames = n; v->ch = ch; v->rate = rate ? rate : 22050;
    v->used = 1; v->vol = 0; v->pan = 0; gains(v);
    return h;
}

static void locked_Snd_Play(u32 h, u32 freq, u32 loop)
{
    if (h >= NBUF || !g_buf[h].used) return;
    Voice *v = &g_buf[h];
    if (freq) v->freq = freq;
#if defined(DC2_AICA)
    if (v->hw) { v->loop = loop != 0; Aica_Play(v->hw, v->freq, loop, v->gl, v->gr); SLOG("snd %u play on the AICA freq %u loop %u\n", (unsigned)h, (unsigned)freq, (unsigned)loop); return; }
#endif
    v->pos = 0; v->playing = 1; v->paused = 0; v->loop = loop != 0;
    if (v->f) { fseek(v->f, (long)v->data_off, SEEK_SET); v->data_read = 0; v->frames = 0; }
    SLOG("snd %u play freq %u loop %u (%u frames at %u)\n", (unsigned)h, (unsigned)freq, (unsigned)loop, (unsigned)v->frames, (unsigned)v->rate);
}
static void locked_Snd_SetVolume(u32 h, s32 vol)
{
    if (h >= NBUF || !g_buf[h].used) return;
    g_buf[h].vol = clamp_cb(cb_of(vol, 3000.0f), -10000, 0); gains(&g_buf[h]);
#if defined(DC2_AICA)
    if (g_buf[h].hw) Aica_Gains(g_buf[h].hw, g_buf[h].gl, g_buf[h].gr);
#endif
    SLOG("snd %u volume %d (%d)\n", (unsigned)h, (int)vol, (int)g_buf[h].vol);
}
static void locked_Snd_SetPan(u32 h, u32 pan)
{
    if (h >= NBUF || !g_buf[h].used) return;
    g_buf[h].pan = clamp_cb((s32)((float)(s32)pan * 0.015625f * 10000.0f - 10000.0f), -10000, 10000); gains(&g_buf[h]);
#if defined(DC2_AICA)
    if (g_buf[h].hw) Aica_Gains(g_buf[h].hw, g_buf[h].gl, g_buf[h].gr);
#endif
}
#if defined(DC2_AICA)
#define HW(h) (h < NBUF && g_buf[h].hw)
#else
#define HW(h) 0
#define Aica_IsPlaying(s) 0
#define Aica_Stop(s) ((void)0)
#define Aica_Pause(s) ((void)0)
#define Aica_Resume(s) ((void)0)
#endif
static u32 locked_Snd_IsPlaying(u32 h)
{
    if (HW(h)) return (u32)Aica_IsPlaying(g_buf[h].hw);
    return h < NBUF && g_buf[h].used && g_buf[h].playing;
}
static void locked_Snd_Stop(u32 h) { if (HW(h)) Aica_Stop(g_buf[h].hw); else if (h < NBUF) g_buf[h].playing = 0; }
static void locked_Snd_Free(u32 h) { if (h < NBUF) voice_free(&g_buf[h]); }
static void locked_Snd_Pause(u32 h)
{
    if (HW(h)) { Aica_Pause(g_buf[h].hw); return; }
    if (h < NBUF && g_buf[h].playing) { g_buf[h].playing = 0; g_buf[h].paused = 1; }
}
static void locked_Snd_Resume(u32 h)
{
    if (HW(h)) { Aica_Resume(g_buf[h].hw); return; }
    if (h < NBUF && g_buf[h].paused) { g_buf[h].paused = 0; g_buf[h].playing = 1; }
}

/* ---- streams ---- */

static u32 locked_Strm_Open(gptr mp3, u32 size, u32 loop)
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
    SLOG("strm %u open %u bytes: %u frames at %u loop %u\n", (unsigned)h, (unsigned)size, (unsigned)v->frames, (unsigned)rate, (unsigned)loop);
    return h;
}
/* a stream played from the disc rather than memory (DC2_LOWMEM builds: the
   room music converted by tools/dreamcast/dc2_music.py): record rec of the
   game's file id, looked up in data\BGM\INDEX.TXT ("MS_0001 3 1A2B3C4D"),
   played from data\BGM\1A2B3C4D.WAV (IMA ADPCM). 0xFFFFFFFF: not there. */
static char *g_bgm_index;
static u32 locked_Strm_OpenFile(const char *file, u32 rec, u32 loop)
{
    char path[512], want[64], line[96];
    if (!g_bgm_index) {
        Pc_HostPath("data\\BGM\\INDEX.TXT", path, sizeof path);
        FILE *ix = fopen(path, "rb");
        if (!ix) { g_bgm_index = calloc(1, 1); SLOG("no %s: the music stays silent\n", path); return 0xFFFFFFFF; }
        fseek(ix, 0, SEEK_END); long n = ftell(ix); fseek(ix, 0, SEEK_SET);
        g_bgm_index = calloc(1, (size_t)n + 1);
        if (Host_Read(g_bgm_index, (size_t)n, ix) != (size_t)n) g_bgm_index[0] = 0;
        fclose(ix);
    }
    size_t k = 0;
    for (; file[k] && file[k] != '.' && k < 40; k++) want[k] = (char)(file[k] >= 'a' && file[k] <= 'z' ? file[k] - 32 : file[k]);
    snprintf(want + k, sizeof want - k, " %u ", (unsigned)rec);
    const char *e = g_bgm_index;
    while (e && *e) {
        size_t l = strcspn(e, "\r\n");
        if (l < sizeof line && !strncmp(e, want, strlen(want))) {
            memcpy(line, e, l); line[l] = 0;
            break;
        }
        e += l; while (*e == '\r' || *e == '\n') e++;
    }
    if (!e || !*e) { SLOG("strm: %s record %u not in BGM\\INDEX.TXT\n", file, (unsigned)rec); return 0xFFFFFFFF; }
    char wav[64];
    snprintf(wav, sizeof wav, "data\\BGM\\%s.WAV", line + strlen(want));
    Pc_HostPath(wav, path, sizeof path);
    FILE *f = fopen(path, "rb");
    if (!f) { SLOG("strm: no %s\n", path); return 0xFFFFFFFF; }
    u32 h = 0;
    while (h < NSTRM && g_strm[h].used) h++;
    if (h == NSTRM) { fclose(f); return 0xFFFFFFFF; }
    Voice *v = &g_strm[h];
    memset(v, 0, sizeof *v);
    if (!file_voice(v, f, loop, 0)) {
        SLOG("strm: %s is not an IMA ADPCM WAV\n", path); fclose(f); memset(v, 0, sizeof *v); return 0xFFFFFFFF;
    }
    SLOG("strm %u open %s: %u Hz, %u ch, loop %u\n", (unsigned)h, path, (unsigned)v->rate, (unsigned)v->ch, (unsigned)loop);
    return h;
}

static void locked_Strm_Play(u32 h) { SLOG("strm %u play\n", (unsigned)h); if (h < NSTRM && g_strm[h].used) { g_strm[h].playing = 1; g_strm[h].paused = 0; } }
static void locked_Strm_Stop(u32 h) { if (h < NSTRM) g_strm[h].playing = 0; }
static void locked_Strm_SetVolume(u32 h, s32 vol)
{
    if (h >= NSTRM || !g_strm[h].used) return;
    g_strm[h].vol = clamp_cb(cb_of(vol, 10000.0f), -10000, 0); gains(&g_strm[h]);
    SLOG("strm %u volume %d (%d)\n", (unsigned)h, (int)vol, (int)g_strm[h].vol);
}
static void locked_Strm_Close(u32 h) { if (h < NSTRM) voice_free(&g_strm[h]); }
static void locked_Strm_Pause(u32 h) { if (h < NSTRM && g_strm[h].playing) { g_strm[h].playing = 0; g_strm[h].paused = 1; } }
static void locked_Strm_Resume(u32 h) { if (h < NSTRM && g_strm[h].paused) { g_strm[h].paused = 0; g_strm[h].playing = 1; } }

/* ---- the mix ---- */

static void mix_voice(Voice *v, float *acc, u32 frames)
{
    if (!v->playing || (!v->frames && !v->f)) { if (v->playing) v->playing = 0; return; }
    xreal_pos step = (xreal_pos)(v->freq ? v->freq : v->rate) / (xreal_pos)g_rate;
    for (u32 i = 0; i < frames; i++) {
        while (v->pos >= v->frames) {
            if (v->f) {                             /* a file stream: its next block */
                v->pos -= v->frames;
                if (!file_next_block(v)) { v->playing = 0; v->frames = 0; return; }
                continue;
            }
            if (!v->loop) { v->playing = 0; return; }
            v->pos -= v->frames;
        }
        float l, r;
#ifdef DC2_MIX_SMOOTH
        /* the frames around the position weighed (mix_tap.h): no metallic
           ring from frames repeated or dropped; a file stream's block holds
           its last frame (the next block not read yet) */
        mix_tap(v->pcm, v->ch, v->frames, v->loop && !v->f, (double)v->pos, (float)step, &l, &r);
#else
        u32 k = (u32)v->pos;
        if (v->ch == 2) { l = v->pcm[2 * k]; r = v->pcm[2 * k + 1]; }
        else l = r = v->pcm[k];
#endif
        acc[2 * i] += l * v->gl; acc[2 * i + 1] += r * v->gr;
        v->pos += step;
    }
}

static void locked_PcAudio_Mix(s16 *lr, u32 frames)
{
    static float acc[2 * 8192];
    while (frames) {
        u32 n = frames > 8192 ? 8192 : frames;
        memset(acc, 0, 2 * n * sizeof(float));
        for (u32 h = 0; h < NBUF; h++) if (g_buf[h].used) mix_voice(&g_buf[h], acc, n);
        for (u32 h = 0; h < NSTRM; h++) if (g_strm[h].used) mix_voice(&g_strm[h], acc, n);
        if (g_source) g_source(acc, n);
        for (u32 i = 0; i < 2 * n; i++) {
            float s = acc[i];
            lr[i] = (s16)(s > 32767.0f ? 32767 : s < -32768.0f ? -32768 : (s32)s);
        }
        lr += 2 * n; frames -= n;
    }
}

static void locked_PcAudio_SetSource(u32 (*add)(float *lr, u32 frames)) { g_source = add; }
u32 PcAudio_Rate(void) { return g_rate; }
void PcAudio_Init(u32 rate, void (*sink)(const s16 *lr, u32 frames)) { g_rate = rate ? rate : 44100; g_sink = sink; }

void PcAudio_Frame(u32 sixtieths)
{
    static s16 out[2 * 8192];
    g_carry += (xreal_pos)g_rate * (xreal_pos)(sixtieths ? sixtieths : 1) / (xreal_pos)60;
    u32 n = (u32)g_carry;
    g_carry -= n;
    if (n > 8192) n = 8192;
    PcAudio_Mix(out, n);
    if (g_sink) g_sink(out, n);
}

/* the memory the voices hold (their PCM copies and file blocks), bytes */
u32 PcAudio_Bytes(void)
{
    u32 n = 0;
    for (u32 h = 0; h < NBUF; h++) if (g_buf[h].used && !g_buf[h].borrowed) n += (g_buf[h].f ? g_buf[h].spb : g_buf[h].frames) * g_buf[h].ch * 2;
    for (u32 h = 0; h < NSTRM; h++) if (g_strm[h].used) n += (g_strm[h].f ? g_strm[h].spb : g_strm[h].frames) * g_strm[h].ch * 2;
    return n;
}

/* ---- the calls under the lock ---- */
void PcAudio_Lock(int on) { if (on) ALOCK(); else AUNLOCK(); }
u32 Snd_OpenFileVoice(const char *game_path) { ALOCK(); u32 r = locked_Snd_OpenFileVoice(game_path); AUNLOCK(); return r; }
u32 DC2CALL Snd_CreateBuffer(gptr w, u32 voice) { ALOCK(); u32 r = locked_Snd_CreateBuffer(w, voice); AUNLOCK(); return r; }
void DC2CALL Snd_Play(u32 h, u32 freq, u32 loop) { ALOCK(); locked_Snd_Play(h, freq, loop); AUNLOCK(); }
void DC2CALL Snd_SetVolume(u32 h, s32 vol) { ALOCK(); locked_Snd_SetVolume(h, vol); AUNLOCK(); }
void DC2CALL Snd_SetPan(u32 h, u32 pan) { ALOCK(); locked_Snd_SetPan(h, pan); AUNLOCK(); }
u32 DC2CALL Snd_IsPlaying(u32 h) { ALOCK(); u32 r = locked_Snd_IsPlaying(h); AUNLOCK(); return r; }
void DC2CALL Snd_Stop(u32 h) { ALOCK(); locked_Snd_Stop(h); AUNLOCK(); }
void DC2CALL Snd_Free(u32 h) { ALOCK(); locked_Snd_Free(h); AUNLOCK(); }
void DC2CALL Snd_Pause(u32 h) { ALOCK(); locked_Snd_Pause(h); AUNLOCK(); }
void DC2CALL Snd_Resume(u32 h) { ALOCK(); locked_Snd_Resume(h); AUNLOCK(); }
u32 DC2CALL Strm_Open(gptr mp3, u32 size, u32 loop) { ALOCK(); u32 r = locked_Strm_Open(mp3, size, loop); AUNLOCK(); return r; }
u32 Strm_OpenFile(const char *file, u32 rec, u32 loop) { ALOCK(); u32 r = locked_Strm_OpenFile(file, rec, loop); AUNLOCK(); return r; }
void DC2CALL Strm_Play(u32 h) { ALOCK(); locked_Strm_Play(h); AUNLOCK(); }
void DC2CALL Strm_Stop(u32 h) { ALOCK(); locked_Strm_Stop(h); AUNLOCK(); }
void DC2CALL Strm_SetVolume(u32 h, s32 vol) { ALOCK(); locked_Strm_SetVolume(h, vol); AUNLOCK(); }
void DC2CALL Strm_Close(u32 h) { ALOCK(); locked_Strm_Close(h); AUNLOCK(); }
void DC2CALL Strm_Pause(u32 h) { ALOCK(); locked_Strm_Pause(h); AUNLOCK(); }
void DC2CALL Strm_Resume(u32 h) { ALOCK(); locked_Strm_Resume(h); AUNLOCK(); }
void PcAudio_Mix(s16 *lr, u32 frames) { ALOCK(); locked_PcAudio_Mix(lr, frames); AUNLOCK(); }
void PcAudio_SetSource(u32 (*add)(float *lr, u32 frames)) { ALOCK(); locked_PcAudio_SetSource(add); AUNLOCK(); }
#endif
