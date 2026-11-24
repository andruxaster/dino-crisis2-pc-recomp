/* The game's streams on KallistiOS streams (see aica_strm.h). */
#include <string.h>
#include <math.h>
#include <kos.h>
#include <kos/mutex.h>
#include <dc/sound/stream.h>
#include "aica_strm.h"

#define NS 3                       /* (SND_STREAM_MAX is 4: one is the main mix) */
#define MAXF 4096                  /* frames a callback */

typedef struct {
    snd_stream_hnd_t h;
    AicaFill fill;
    void *ctx;
    u32 rate, ch;
    u8 busy;                       /* taken by a voice (until its close is done) */
    u8 want_start, want_stop, gains_dirty;   /* requests (under g_sm) */
    u8 live, ended;                /* the sound thread's own */
    int vol, lpan, rpan;
    s16 buf[MAXF * 2] __attribute__((aligned(32)));
} Slot;

static Slot g_s[NS];
static int g_n;
static mutex_t g_sm = MUTEX_INITIALIZER;

static void *cb(snd_stream_hnd_t h, int req, int *got)
{
    Slot *s = snd_stream_get_userdata(h);
    u32 fsz = 2 * s->ch, want = (u32)req / fsz;
    if (want > MAXF) want = MAXF;
    u32 n = s->fill ? s->fill(s->ctx, s->buf, want) : 0;
    if (n < want) memset(s->buf + n * s->ch, 0, (want - n) * fsz);
    if (!n) s->ended = 1;
    *got = (int)(want * fsz);
    return s->buf;
}

int AicaStrm_Init(void)
{
    for (g_n = 0; g_n < NS; g_n++) {
        snd_stream_hnd_t h = snd_stream_alloc(cb, SND_STREAM_BUFFER_MAX / 2);   /* (~0.7 s at 22050 Hz: room for a slow disc read) */
        if (h == SND_STREAM_INVALID) break;
        g_s[g_n].h = h;
        snd_stream_set_userdata(h, &g_s[g_n]);
    }
    return g_n;
}

/* the gains as the stream's volume and pan, as the banks' sounds have them
   (aica_sfx.c): a mono stream panned in 3 dB steps, a stereo one hard left
   and right at the louder side's volume */
static void to_aica(Slot *s, float gl, float gr)
{
    float hi = gl > gr ? gl : gr, lo = gl > gr ? gr : gl, v = hi * 255.0f + 0.5f;
    s->vol = v <= 0 ? 0 : v >= 255 ? 255 : (int)v;
    if (s->ch == 2) { s->lpan = 0; s->rpan = 255; return; }
    int steps = 15;
    if (lo > 0.0f && hi > 0.0f) { steps = (int)(20.0f * log10f(hi / lo) / 3.0f + 0.5f); if (steps > 15) steps = 15; }
    s->lpan = s->rpan = !steps ? 0x80 : gl > gr ? 0x7F - (steps << 3) : 0x80 + (steps << 3);
}

int AicaStrm_Open(u32 rate, u32 ch, AicaFill fill, void *ctx, float gl, float gr)
{
    int r = -1;
    mutex_lock(&g_sm);
    for (int i = 0; i < g_n; i++)
        if (!g_s[i].busy) {
            Slot *s = &g_s[i];
            s->busy = 1; s->fill = fill; s->ctx = ctx; s->rate = rate; s->ch = ch == 2 ? 2 : 1;
            to_aica(s, gl, gr);
            s->want_start = 1; s->want_stop = 0; s->gains_dirty = 0;
            r = i;
            break;
        }
    mutex_unlock(&g_sm);
    return r;
}

void AicaStrm_Gains(int i, float gl, float gr)
{
    if (i < 0 || i >= g_n) return;
    mutex_lock(&g_sm);
    to_aica(&g_s[i], gl, gr);
    g_s[i].gains_dirty = 1;
    mutex_unlock(&g_sm);
}

void AicaStrm_Close(int i)
{
    if (i < 0 || i >= g_n) return;
    mutex_lock(&g_sm);
    Slot *s = &g_s[i];
    s->fill = NULL;                                /* (no more calls into the voice) */
    if (s->want_start && !s->live) { s->want_start = 0; s->busy = 0; }   /* (never started) */
    else s->want_stop = 1;
    mutex_unlock(&g_sm);
}

void AicaStrm_Poll(void)
{
    for (int i = 0; i < g_n; i++) {
        Slot *s = &g_s[i];
        mutex_lock(&g_sm);
        u8 start = s->want_start, stop = s->want_stop, gains = s->gains_dirty;
        int vol = s->vol, lp = s->lpan, rp = s->rpan;
        u32 rate = s->rate, ch = s->ch;
        s->want_start = s->want_stop = s->gains_dirty = 0;
        mutex_unlock(&g_sm);
        if (stop) {
            if (s->live) snd_stream_stop(s->h);
            s->live = 0;
            mutex_lock(&g_sm); s->busy = 0; mutex_unlock(&g_sm);
            continue;
        }
        if (start) {
            s->ended = 0;
            snd_stream_start(s->h, rate, ch == 2);   /* (it starts at full volume, centred) */
            snd_stream_volume(s->h, vol);
            snd_stream_pan(s->h, lp, rp);
            s->live = 1;
        } else if (gains && s->live) {
            snd_stream_volume(s->h, vol);
            snd_stream_pan(s->h, lp, rp);
        }
        if (s->live) {
            snd_stream_poll(s->h);
            if (s->ended) { snd_stream_stop(s->h); s->live = 0; }   /* (busy until the voice closes it) */
        }
    }
}

u32 AicaStrm_Live(void)
{
    u32 n = 0;
    for (int i = 0; i < g_n; i++) n += g_s[i].live;
    return n;
}
