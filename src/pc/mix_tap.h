/* One output frame of a voice resampled (src/pc/audio.c, DC2_MIX_SMOOTH):
 * pure arithmetic, kept apart so the host can test it (tests/mix_test.c).
 *
 * The plain mix took the nearest frame. Played up (a sound at 11025 Hz into
 * a 22050 Hz mix) that repeats each frame and mirrors the sound into the top
 * half of the band; played down (44100 Hz into 22050) it drops frames and
 * folds the highs back into the band: both heard as a metallic ring, loud at
 * the Dreamcast's 22050 Hz. A linear interpolation still leaves the mirror
 * of a sound's upper part (a 4 kHz tone at 11025 Hz: its mirror at 7 kHz
 * only ~12 dB down). Here the frames around the position are weighed by a
 * windowed sinc (Blackman, 6 frames each side) cut off at 0.45 of the
 * source's rate: what is above its half (the mirror) is gone. Played down,
 * the kernel is widened by the step, cutting at the mix's half instead. */
#ifndef DC2_MIX_TAP_H
#define DC2_MIX_TAP_H
#include <math.h>

#define MIX_TAP_N      6                /* frames each side (played up) */
#define MIX_TAP_RES    64               /* the kernel's table: steps a frame */
#define MIX_TAP_CUT    0.45             /* cut-off, of the source's rate */
#define MIX_TAP_MAXW   4.0f             /* widening at most (step 4: 32 frames) */

#define MIX_TAP_PH     256              /* phases of the table for playing up */

static float g_mix_kernel[MIX_TAP_N * MIX_TAP_RES + 2];
/* played up (step <= 1), inside the sound: the weights of the 2N frames
   around a position by its phase, normalised (sum 1) */
static float g_mix_poly[MIX_TAP_PH][2 * MIX_TAP_N];

/* the kernel at x frames (0 <= x <= N): 2c sinc(2c x) times the window */
static inline void mix_tap_init(void)
{
    if (g_mix_kernel[0] != 0.0f) return;
    for (int i = 0; i <= MIX_TAP_N * MIX_TAP_RES + 1; i++) {
        double x = (double)i / MIX_TAP_RES, t = 2.0 * MIX_TAP_CUT * x;
        double s = t == 0.0 ? 1.0 : sin(M_PI * t) / (M_PI * t);
        double u = x / MIX_TAP_N;
        double w = u >= 1.0 ? 0.0 : 0.42 + 0.5 * cos(M_PI * u) + 0.08 * cos(2.0 * M_PI * u);
        g_mix_kernel[i] = (float)(2.0 * MIX_TAP_CUT * s * w);
    }
    for (int p = 0; p < MIX_TAP_PH; p++) {
        double fr = (double)p / MIX_TAP_PH, sum = 0, w[2 * MIX_TAP_N];
        for (int t = 0; t < 2 * MIX_TAP_N; t++) {      /* frame k - N + 1 + t, k the one at or before */
            double x = fabs((double)(t - MIX_TAP_N + 1) - fr), u = x / MIX_TAP_N, c = 2.0 * MIX_TAP_CUT * x;
            double sn = c == 0.0 ? 1.0 : sin(M_PI * c) / (M_PI * c);
            w[t] = u >= 1.0 ? 0.0 : 2.0 * MIX_TAP_CUT * sn * (0.42 + 0.5 * cos(M_PI * u) + 0.08 * cos(2.0 * M_PI * u));
            sum += w[t];
        }
        for (int t = 0; t < 2 * MIX_TAP_N; t++) g_mix_poly[p][t] = (float)(w[t] / sum);
    }
}

static inline float mix_kernel(float x)       /* x >= 0, in frames of the kernel */
{
    float f = x * MIX_TAP_RES;
    if (f >= MIX_TAP_N * MIX_TAP_RES) return 0.0f;
    int i = (int)f;
    float a = f - (float)i;
    return g_mix_kernel[i] + (g_mix_kernel[i + 1] - g_mix_kernel[i]) * a;
}

/* the frame at pos (frames x ch of pcm; ch 1 or 2), step source frames an
   output frame; frames past the ends wrap round when wrap, else the end's */
static inline void mix_tap(const short *pcm, unsigned ch, unsigned frames, int wrap, double pos, float step, float *l, float *r)
{
    mix_tap_init();
    long k0 = (long)pos;
    if (step == 1.0f && pos == (double)k0 && pos >= 0 && k0 < (long)frames) {   /* a sound at the mix's rate: its frames */
        if (ch == 2) { *l = pcm[2 * k0]; *r = pcm[2 * k0 + 1]; } else *l = *r = pcm[k0];
        return;
    }
    if (step <= 1.0f && pos >= 0 && k0 >= MIX_TAP_N - 1 && k0 + MIX_TAP_N < (long)frames) {
        double fr = pos - (double)k0;
        const float *w = g_mix_poly[(int)(fr * MIX_TAP_PH + 0.5) & (MIX_TAP_PH - 1)];
        int roll = fr * MIX_TAP_PH + 0.5 >= MIX_TAP_PH;   /* (the phase rounded up to the next frame) */
        long b = k0 - MIX_TAP_N + 1 + roll;
        float sl = 0, sr = 0;
        if (ch == 2) {
            const short *q = pcm + 2 * b;
            if (b + 2 * MIX_TAP_N > (long)frames) q = 0;
            if (q) { for (int t = 0; t < 2 * MIX_TAP_N; t++) { sl += w[t] * q[2 * t]; sr += w[t] * q[2 * t + 1]; } *l = sl; *r = sr; return; }
        } else if (b + 2 * MIX_TAP_N <= (long)frames) {
            const short *q = pcm + b;
            for (int t = 0; t < 2 * MIX_TAP_N; t++) sl += w[t] * q[t];
            *l = *r = sl;
            return;
        }
    }
    float sc = step > 1.0f ? (step < MIX_TAP_MAXW ? step : MIX_TAP_MAXW) : 1.0f;
    float hw = MIX_TAP_N * sc, inv = 1.0f / sc;
    long j0 = (long)floor(pos - hw) + 1, j1 = (long)floor(pos + hw);
    float sl = 0, sr = 0, sw = 0;
    for (long j = j0; j <= j1; j++) {
        float d = (float)(j > pos ? j - pos : pos - j);
        float w = mix_kernel(d * inv);
        if (w == 0.0f) continue;
        long k = j;
        if (k < 0 || k >= (long)frames) {
            if (wrap) { k %= (long)frames; if (k < 0) k += frames; }
            else k = k < 0 ? 0 : (long)frames - 1;
        }
        if (ch == 2) { sl += w * pcm[2 * k]; sr += w * pcm[2 * k + 1]; }
        else { float s = w * pcm[k]; sl += s; sr += s; }
        sw += w;
    }
    *l = sl / sw; *r = sr / sw;
}
#endif
