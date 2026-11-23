/* The game's sound buffers on the AICA's channels (see aica_sfx.h). */
#include "dc2/crumb.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <kos.h>
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#include <dc/sound/aica_comm.h>
#include <dc/spu.h>
#include "aica_sfx.h"

#define AICA_MAX_FRAMES 65534          /* a channel's loop end is 16 bits */

static int g_ready;
static u32 g_count;

void Aica_Init(void) { g_ready = 1; }
u32 Aica_Count(void) { return g_count; }
u32 Aica_Free(void) { return g_ready ? snd_mem_available() : 0; }

/* one channel's samples into sound RAM (32-byte aligned for the loader) */
static u32 upload(const void *data, u32 bytes)
{
    u32 n = (bytes + 31) & ~31u;
    u32 at = snd_mem_malloc(n);
    if (!at) return 0;
    CRUMB('M');
    spu_memload(at, (void *)data, n);
    CRUMB('m');
    return at;
}

AicaSnd *Aica_Load(const void *pcm, u32 frames, u32 ch, u32 bits, u32 rate)
{
    if (!g_ready || !frames || frames > AICA_MAX_FRAMES || (ch != 1 && ch != 2) || (bits != 8 && bits != 16)) return NULL;
    AicaSnd *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    u32 b = bits / 8, len = frames * b;
    /* each channel's samples apart, padded to 32 bytes; 8-bit made signed */
    u8 *tmp = memalign(32, (len + 31) & ~31u);
    if (!tmp) { free(s); return NULL; }
    for (u32 c = 0; c < ch; c++) {
        memset(tmp, 0, (len + 31) & ~31u);
        if (b == 2) {
            const s16 *in = pcm;
            s16 *out = (s16 *)tmp;
            for (u32 i = 0; i < frames; i++) out[i] = in[i * ch + c];
        } else {
            const u8 *in = pcm;
            for (u32 i = 0; i < frames; i++) tmp[i] = (u8)(in[i * ch + c] ^ 0x80);
        }
        if (!(s->mem[c] = upload(tmp, len))) {
            if (c) snd_mem_free(s->mem[0]);
            free(tmp); free(s);
            return NULL;
        }
    }
    free(tmp);
    s->frames = frames; s->ch = ch; s->bytes = b; s->rate = rate ? rate : 22050;
    s->chn[0] = s->chn[1] = -1;
    g_count++;
    return s;
}

static void chn_cmd(int chn, u32 cmd, u32 base, u32 type, u32 length, int loop, u32 freq, u8 vol, u8 pan)
{
    AICA_CMDSTR_CHANNEL(tmp, c, a);
    c->cmd = AICA_CMD_CHAN;
    c->timestamp = 0;
    c->size = AICA_CMDSTR_CHANNEL_SIZE;
    c->cmd_id = chn;
    a->cmd = cmd;
    a->base = base;
    a->type = type;
    a->length = length;
    a->loop = loop;
    a->loopstart = 0;
    a->loopend = length;
    a->freq = freq;
    a->vol = vol;
    a->pan = pan;
    CRUMB('A');
    snd_sh4_to_aica(tmp, c->size);
}

static void release(AicaSnd *s)
{
    for (int c = 0; c < 2; c++)
        if (s->chn[c] >= 0) { snd_sfx_chn_free(s->chn[c]); s->chn[c] = -1; }
}

void Aica_Stop(AicaSnd *s)
{
    if (!s) return;
    for (int c = 0; c < 2; c++)
        if (s->chn[c] >= 0) chn_cmd(s->chn[c], AICA_CH_CMD_STOP, 0, 0, 0, 0, 0, 0, 0);
    release(s);
    s->playing = 0; s->paused = 0;
}

void Aica_Unload(AicaSnd *s)
{
    if (!s) return;
    Aica_Stop(s);
    for (u32 c = 0; c < s->ch; c++) snd_mem_free(s->mem[c]);
    g_count--;
    free(s);
}

/* the gains as each channel's volume and pan: a mono sound one channel
   panned (the AICA's pan: 3 dB steps, 15 at most); a stereo one its two,
   hard left and right */
static u8 vol255(float g) { float v = g * 255.0f + 0.5f; return v <= 0 ? 0 : v >= 255 ? 255 : (u8)v; }
static void set_gains(AicaSnd *s, float gl, float gr)
{
    if (s->ch == 2) {
        s->vol[0] = vol255(gl); s->pan[0] = 0;
        s->vol[1] = vol255(gr); s->pan[1] = 255;
        return;
    }
    float hi = gl > gr ? gl : gr, lo = gl > gr ? gr : gl;
    s->vol[0] = vol255(hi);
    int steps = 15;
    if (lo > 0.0f && hi > 0.0f) {
        float db = 20.0f * log10f(hi / lo);
        steps = (int)(db / 3.0f + 0.5f);
        if (steps > 15) steps = 15;
    }
    /* the driver's pan x: under 0x80 the right side down (0x7F - x) >> 3
       steps, over it the left (x - 0x80) >> 3 */
    if (!steps) s->pan[0] = 0x80;
    else if (gl > gr) s->pan[0] = (u8)(0x7F - (steps << 3));
    else s->pan[0] = (u8)(0x80 + (steps << 3));
}

static void start(AicaSnd *s)
{
    u32 len = s->frames - s->offset;
    for (u32 c = 0; c < s->ch; c++) {
        if (s->chn[c] < 0) s->chn[c] = snd_sfx_chn_alloc();
        if (s->chn[c] < 0) continue;                  /* (all 64 busy: this side silent) */
        chn_cmd(s->chn[c], AICA_CH_CMD_START, s->mem[c] + s->offset * s->bytes,
                s->bytes == 2 ? AICA_SM_16BIT : AICA_SM_8BIT, len, s->loop, s->freq, s->vol[c], s->pan[c]);
    }
    s->start_us = timer_us_gettime64();
    s->playing = 1; s->paused = 0;
}

void Aica_Play(AicaSnd *s, u32 freq, int loop, float gl, float gr)
{
    if (!s) return;
    if (freq) s->freq = freq;
    if (!s->freq) s->freq = s->rate;
    s->loop = loop != 0;
    s->offset = 0;
    set_gains(s, gl, gr);
    start(s);
}

void Aica_Gains(AicaSnd *s, float gl, float gr)
{
    if (!s) return;
    set_gains(s, gl, gr);
    if (!s->playing) return;
    for (u32 c = 0; c < s->ch; c++)
        if (s->chn[c] >= 0) {
            AICA_CMDSTR_CHANNEL(tmp, cm, a);
            cm->cmd = AICA_CMD_CHAN; cm->timestamp = 0; cm->size = AICA_CMDSTR_CHANNEL_SIZE; cm->cmd_id = s->chn[c];
            a->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_VOL | AICA_CH_UPDATE_SET_PAN;
            a->vol = s->vol[c]; a->pan = s->pan[c];
            CRUMB('a');
            snd_sh4_to_aica(tmp, cm->size);
        }
}

/* frames played since the start (a looped one: within its loop) */
static u32 played(AicaSnd *s)
{
    unsigned long long us = timer_us_gettime64() - s->start_us;
    unsigned long long f = us * s->freq / 1000000u;
    u32 len = s->frames - s->offset;
    return s->loop ? (u32)(f % (len ? len : 1)) : (u32)(f > len ? len : f);
}

int Aica_IsPlaying(AicaSnd *s)
{
    if (!s || !s->playing) return 0;
    if (s->loop) return 1;
    if (played(s) < s->frames - s->offset) return 1;
    s->playing = 0;                                   /* over: its channels back */
    release(s);
    return 0;
}

void Aica_Pause(AicaSnd *s)
{
    if (!s || !Aica_IsPlaying(s)) return;
    u32 at = s->offset + played(s);
    Aica_Stop(s);
    s->offset = s->loop ? 0 : at;                     /* (a looped sound starts its loop again) */
    s->paused = 1;
}

void Aica_Resume(AicaSnd *s)
{
    if (!s || !s->paused) return;
    if (s->offset >= s->frames) { s->paused = 0; return; }
    start(s);
}
