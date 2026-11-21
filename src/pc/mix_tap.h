/* One output frame of a voice resampled (src/pc/audio.c, DC2_MIX_SMOOTH):
 * pure arithmetic, kept apart so the host can test it (tests/mix_test.c).
 *
 * The plain mix took the nearest frame. Played up (a sound at 11025 Hz into
 * a 22050 Hz mix) that repeats each frame and mirrors the sound into the top
 * half of the band; played down (44100 Hz into 22050) it drops frames and
 * folds the highs back into the band: both heard as a metallic ring, loud at
 * the Dreamcast's 22050 Hz. Here a triangle of half-width max(1, step)
 * around the position weighs the frames: up to one frame a step it is the
 * linear interpolation, beyond it also filters what the step would fold. */
#ifndef DC2_MIX_TAP_H
#define DC2_MIX_TAP_H
#include <math.h>

#define MIX_TAP_MAXW 4.0f               /* (half-width at most: 9 frames) */

/* the frame at pos (frames x ch of pcm; ch 1 or 2), step source frames an
   output frame; frames past the ends wrap round when wrap, else the end's */
static inline void mix_tap(const short *pcm, unsigned ch, unsigned frames, int wrap, double pos, float step, float *l, float *r)
{
    float hw = step > 1.0f ? (step < MIX_TAP_MAXW ? step : MIX_TAP_MAXW) : 1.0f;
    long j0 = (long)floor(pos - hw) + 1, j1 = (long)floor(pos + hw);
    float sl = 0, sr = 0, sw = 0;
    for (long j = j0; j <= j1; j++) {
        float w = 1.0f - (float)((j > pos ? j - pos : pos - j)) / hw;
        if (w <= 0) continue;
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
