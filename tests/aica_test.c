/* Host test of src/port/dc/aica_sfx.c (the banks' sounds on the AICA's
 * channels) against stand-ins for KallistiOS that keep the sound RAM and
 * the channel commands:   make aica-test   (KOS_BASE: KallistiOS, for
 * aica_comm.h). Checked: a stereo sound split into its two channels, 8-bit
 * made signed, too long refused; the start command (rate, length, format,
 * base), stereo hard pans, mono pan steps and volume, updates; the end by
 * time and the channels given back; a loop plays on; a one-shot paused and
 * resumed from where it was; everything freed. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "kos.h"
#include "dc/sound/aica_comm.h"
#include "aica_sfx.h"
void (*g_crumb)(int c);                   /* (the breadcrumbs: none here) */
static unsigned long long now; unsigned long long timer_us_gettime64(void) { return now; }
static uint8_t ram[2 << 20]; static uint32_t top = 0x11000, freed;
uint32_t snd_mem_malloc(size_t n) { if (top + n > sizeof ram) return 0; uint32_t a = top; top += n; return a; }
void snd_mem_free(uint32_t a) { freed++; (void)a; }
uint32_t snd_mem_available(void) { return sizeof ram - top; }
void spu_memload(uintptr_t to, void *from, size_t n) { memcpy(ram + to, from, n); }
static uint64_t inuse; int snd_sfx_chn_alloc(void) { for (int c = 2; c < 64; c++) if (!(inuse >> c & 1)) { inuse |= 1ULL << c; return c; } return -1; }
void snd_sfx_chn_free(int c) { inuse &= ~(1ULL << c); }
static aica_channel_t last[64]; static int ncmd;
int snd_sh4_to_aica(void *p, uint32_t size) { aica_cmd_t *c = p; aica_channel_t *a = (aica_channel_t *)c->cmd_data; ncmd++;
    if ((a->cmd & 15) == AICA_CH_CMD_START) last[c->cmd_id] = *a;
    if ((a->cmd & 15) == AICA_CH_CMD_UPDATE) { last[c->cmd_id].vol = a->vol; last[c->cmd_id].pan = a->pan; }
    if ((a->cmd & 15) == AICA_CH_CMD_STOP) last[c->cmd_id].cmd = 0;
    (void)size; return 0; }
static int fails; 
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int dpan(int x) { return x == 0x80 ? 0 : x < 0x80 ? -((0x7f - x) >> 3) : (x - 0x80) >> 3; }   /* driver: - right down, + left down */
int main(void) {
    Aica_Init();
    /* stereo 16-bit split, 8-bit made signed */
    int16_t st[2 * 100]; for (int i = 0; i < 100; i++) { st[2*i] = (int16_t)(i * 3); st[2*i+1] = (int16_t)(-i * 5); }
    AicaSnd *s = Aica_Load(st, 100, 2, 16, 11025);
    CHECK(s && s->mem[0] && s->mem[1], "stereo load");
    CHECK(((int16_t *)(ram + s->mem[0]))[7] == 21 && ((int16_t *)(ram + s->mem[1]))[7] == -35, "split L %d R %d", ((int16_t *)(ram + s->mem[0]))[7], ((int16_t *)(ram + s->mem[1]))[7]);
    uint8_t u8s[50]; for (int i = 0; i < 50; i++) u8s[i] = (uint8_t)(0x80 + i - 25);
    AicaSnd *m8 = Aica_Load(u8s, 50, 1, 8, 22050);
    CHECK(m8 && (int8_t)ram[m8->mem[0] + 0] == -25 && (int8_t)ram[m8->mem[0] + 49] == 24, "8-bit signed");
    CHECK(Aica_Load(st, 65535, 1, 16, 22050) == NULL, "too long refused");
    /* play: channels, freq, length, stereo hard pans */
    now = 1000000;
    Aica_Play(s, 0, 0, 1.0f, 0.5f);
    CHECK(s->chn[0] >= 0 && s->chn[1] >= 0, "two channels");
    aica_channel_t *L = &last[s->chn[0]], *R = &last[s->chn[1]];
    CHECK(L->freq == 11025 && L->length == 100 && L->loopend == 100 && L->type == AICA_SM_16BIT && L->base == s->mem[0], "start L");
    CHECK(L->pan == 0 && R->pan == 255 && L->vol == 255 && (R->vol == 127 || R->vol == 128), "stereo gains L %u R %u", (unsigned)L->vol, (unsigned)R->vol);
    /* ends by time: 100 frames at 11025 = 9070 us */
    now += 9000; CHECK(Aica_IsPlaying(s), "still playing");
    now += 200; CHECK(!Aica_IsPlaying(s), "over"); CHECK(s->chn[0] < 0 && !(inuse >> 2 & 1), "channels back");
    /* mono pan: left louder by 9 dB -> 3 steps right down */
    AicaSnd *mo = Aica_Load(st, 200, 1, 16, 22050);
    Aica_Play(mo, 44100, 1, 1.0f, 0.354f);
    aica_channel_t *M = &last[mo->chn[0]];
    CHECK(M->freq == 44100 && M->loop == 1, "freq/loop");
    CHECK(dpan(M->pan) == -3, "pan %d (x %u)", dpan(M->pan), (unsigned)M->pan);
    Aica_Gains(mo, 0.2f, 0.8f);                              /* right louder by 12 dB: left down 4 */
    CHECK(dpan(M->pan) == 4 && (M->vol == 204), "update pan %d vol %u", dpan(M->pan), (unsigned)M->vol);
    Aica_Gains(mo, 0.5f, 0.5f); CHECK(M->pan == 0x80, "centre");
    Aica_Gains(mo, 1.0f, 0.0f); CHECK(dpan(M->pan) == -15, "hard left %d", dpan(M->pan));
    now += 10000000; CHECK(Aica_IsPlaying(mo), "a loop plays on");
    /* pause / resume a one-shot from where it was */
    Aica_Play(m8, 0, 0, 1, 1);
    now += 1000000ull * 20 / 22050 + 1;                      /* 20 frames */
    Aica_Pause(m8); CHECK(m8->paused && m8->offset == 20 && m8->chn[0] < 0, "paused at %u", (unsigned)m8->offset);
    now += 5000000; Aica_Resume(m8);
    aica_channel_t *P = &last[m8->chn[0]];
    CHECK(P->base == m8->mem[0] + 20 && P->length == 30, "resumed base +%u len %u", (unsigned)(P->base - m8->mem[0]), (unsigned)P->length);
    now += 1000000ull * 31 / 22050; CHECK(!Aica_IsPlaying(m8), "resumed one over");
    Aica_Unload(s); Aica_Unload(mo); Aica_Unload(m8);
    CHECK(freed == 4 && Aica_Count() == 0 && inuse == 0, "unloaded: %u freed, %u left, chans %llx", (unsigned)freed, (unsigned)Aica_Count(), (unsigned long long)inuse);
    printf(fails ? "%d FAILED\n" : "ok\n", fails); return fails != 0;
}
