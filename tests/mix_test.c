/* Host test of src/pc/mix_tap.h (a voice resampled into the mix):
 *   make mix-test
 * At the Dreamcast's 22050 Hz: a 1 kHz and a 4 kHz tone at 11025 Hz played up must not
 * leave its mirror at 10025 Hz, a 15 kHz tone at 44100 Hz played down must
 * fold less into 7050 Hz, a tone at the mix's own rate is passed as it is,
 * and the ends (looped or not) and stereo are right. The nearest frame (the
 * plain mix) is measured alongside. */
#include <stdio.h>
#include <math.h>
#include "mix_tap.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); } } while (0)

#define N 22050
static short src[4 * 44100];
static float out[N];

/* the amplitude of frequency f in out (rate 22050), as a fraction of full scale */
static double amp(double f)
{
    double re = 0, im = 0;
    for (int i = 0; i < N; i++) {
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / (N - 1));          /* (Hann window) */
        re += w * out[i] * cos(2 * M_PI * f * i / 22050); im += w * out[i] * sin(2 * M_PI * f * i / 22050);
    }
    return 2 * sqrt(re * re + im * im) / (N * 0.5) / 32767;
}
static double db(double a) { return 20 * log10(a + 1e-12); }

/* a tone of tf Hz at rate sr, mixed at 22050 (nearest or the taps) */
static void run(double tf, unsigned sr, int nearest)
{
    unsigned frames = sr * 2;
    for (unsigned i = 0; i < frames; i++) src[i] = (short)lrint(30000 * sin(2 * M_PI * tf * i / sr));
    float step = (float)sr / 22050;
    double pos = 0;
    for (int i = 0; i < N; i++, pos += step) {
        float l, r;
        if (nearest) l = src[(unsigned)pos];
        else mix_tap(src, 1, frames, 0, pos, step, &l, &r);
        out[i] = l;
    }
}

int main(void)
{
    /* up: 1 kHz at 11025, its mirror at 11025 - 1000 */
    run(1000, 11025, 1); double n_f = amp(1000), n_m = amp(10025);
    run(1000, 11025, 0); double t_f = amp(1000), t_m = amp(10025);
    printf("up   11025 -> 22050, 1 kHz: tone %.1f / %.1f dB, mirror at 10025 Hz nearest %.1f dB, taps %.1f dB\n", db(n_f), db(t_f), db(n_m), db(t_m));
    CHECK(t_f > 0.85, "the tone lost: %f\n", t_f);
    CHECK(db(t_m) < -60, "the mirror not under -60 dB\n");
    /* up, near the top: 4 kHz at 11025, its mirror at 7025 (linear: ~-12 dB) */
    run(4000, 11025, 1); double n4 = amp(7025);
    run(4000, 11025, 0); double t4 = amp(7025), k4 = amp(4000);
    printf("up   11025 -> 22050, 4 kHz: tone %.1f dB, mirror at 7025 Hz nearest %.1f dB, taps %.1f dB\n", db(k4), db(n4), db(t4));
    CHECK(db(t4) < -40, "the 4 kHz mirror not under -40 dB\n");
    CHECK(k4 > 0.7, "4 kHz lost: %f\n", k4);
    /* down: 15 kHz at 44100 folds to 22050 - 15000 = 7050 */
    run(15000, 44100, 1); n_m = amp(7050);
    run(15000, 44100, 0); t_m = amp(7050);
    run(3000, 44100, 0); t_f = amp(3000);
    printf("down 44100 -> 22050: 15 kHz folded to 7050 Hz nearest %.1f dB, taps %.1f dB; 3 kHz kept %.1f dB\n", db(n_m), db(t_m), db(t_f));
    CHECK(db(t_m) < -30, "the fold not under -30 dB\n");
    CHECK(t_f > 0.7, "3 kHz lost: %f\n", t_f);
    /* at the mix's rate: frames as they are */
    run(2500, 22050, 0);
    int same = 1;
    for (int i = 0; i < N; i++) if (fabsf(out[i] - src[i]) > 0.01f) same = 0;
    CHECK(same, "step 1 not the frames themselves\n");
    /* ends and stereo */
    short st[8] = {100, -100, 200, -200, 300, -300, 400, -400};
    float l, r;
    mix_tap(st, 2, 4, 0, 3.5, 1.0f, &l, &r);
    CHECK(l > 390 && l < 415 && fabsf(l + r) < 0.01f, "the end held: %f %f\n", l, r);
    mix_tap(st, 2, 4, 1, 3.5, 1.0f, &l, &r);
    CHECK(l > 100 && l < 400 && fabsf(l + r) < 0.01f, "the loop's wrap: %f %f\n", l, r);
    {   /* the wrap: the same as the frames repeated */
        short big[16]; for (int q = 0; q < 16; q++) big[q] = st[2 * (q % 4)];
        float l2, r2; mix_tap(big, 1, 16, 0, 7.5, 1.0f, &l2, &r2);
        mix_tap(st, 2, 4, 1, 3.5, 1.0f, &l, &r);
        CHECK(fabsf(l - l2) < 0.5f, "the loop's wrap %f, repeated %f\n", l, l2);
    }
    mix_tap(st, 2, 4, 0, 1.25, 1.0f, &l, &r);
    CHECK(l > 200 && l < 250 && fabsf(l + r) < 0.01f, "between frames: %f %f\n", l, r);
    mix_tap(st, 2, 4, 0, 0.0, 3.0f, &l, &r);
    CHECK(l > 100 && l < 250 && fabsf(l + r) < 0.01f, "the start, wide: %f %f\n", l, r);
    {   /* the table (inside) and the kernel (at the ends) agree */
        static short sq[64]; for (int q = 0; q < 64; q++) sq[q] = (short)(q * 517 % 2000 - 1000);
        double worst = 0;
        for (int q = 0; q < 2000; q++) {
            double p = 6 + (q % 1000) * 0.05;
            float a, b2, c, d;
            mix_tap(sq, 1, 64, 0, p, 0.5f, &a, &b2);           /* the table */
            mix_tap(sq, 1, 64, 0, p, 1.0001f, &c, &d);         /* the kernel (step past 1, the same cut) */
            if (fabs(a - c) > worst) worst = fabs(a - c);
        }
        printf("the table against the kernel: at most %.2f apart (of 1000)\n", worst);
        CHECK(worst < 6, "the table and the kernel differ by %f\n", worst);
    }
    printf(fails ? "%d FAILED\n" : "ok\n", fails);
    return fails != 0;
}
