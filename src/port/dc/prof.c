/* A sampling profiler for the Dreamcast (DC2_DC_VERBOSE): TMU1 interrupts a
 * thousand times a second and the interrupted PC and PR (the return
 * address: who called a memcpy, say) are counted in 32-byte buckets of the
 * code. The samples of one frame are held until it ends; a slow frame
 * (over 100 ms) adds them to the slow histograms as well, so that a stall's
 * cost shows apart from the steady frames'. Prof_Report prints the hottest
 * buckets as addresses; tools/dreamcast/dc2_prof.py turns them into
 * function names with the ELF the log came from. TMU0 (threads) and TMU2
 * (the clock) are KallistiOS's; TMU1 is free. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arch/timer.h>
#include <arch/irq.h>
#include <kos/irq.h>
#include "prof.h"

#define PROF_HZ 1000
#define BUCKET_SHIFT 5
#define FRAME_MAX 1024

extern char _executable_start[], _etext[];     /* (the link script's __executable_start, __etext) */

static uint32_t *g_all, *g_slow, *g_slow_pr;
static uint32_t g_lo, g_n, g_samples_all, g_samples_slow, g_samples_out;
static volatile uint32_t g_frame_pc[FRAME_MAX], g_frame_pr[FRAME_MAX];
static volatile uint32_t g_frame_n;

static inline int bucket(uint32_t a) { return a >= g_lo && ((a - g_lo) >> BUCKET_SHIFT) < g_n ? (int)((a - g_lo) >> BUCKET_SHIFT) : -1; }

static void tick(irq_t code, irq_context_t *cx, void *data)
{
    (void)code; (void)data;
    timer_clear(TMU1);
    irq_context_t *c = irq_get_context();
    if (!c) c = cx;
    if (!c) return;
    uint32_t pc = (uint32_t)CONTEXT_PC(*c), pr = (uint32_t)c->pr;
    int b = bucket(pc);
    if (b >= 0) g_all[b]++; else g_samples_out++;
    g_samples_all++;
    uint32_t n = g_frame_n;
    if (n < FRAME_MAX) { g_frame_pc[n] = pc; g_frame_pr[n] = pr; g_frame_n = n + 1; }
}

void Prof_Init(void)
{
    g_lo = (uint32_t)(uintptr_t)_executable_start;
    g_n = (((uint32_t)(uintptr_t)_etext - g_lo) >> BUCKET_SHIFT) + 1;
    g_all = calloc(g_n, 4); g_slow = calloc(g_n, 4); g_slow_pr = calloc(g_n, 4);
    if (!g_all || !g_slow || !g_slow_pr) { printf("profiler: no memory\n"); return; }
    irq_set_handler(EXC_TMU1_TUNI1, tick, NULL);
    timer_prime(TMU1, PROF_HZ, 1);
    timer_clear(TMU1);
    timer_start(TMU1);
    printf("profiler: %u Hz, code %08X..%08X in %u-byte buckets\n", PROF_HZ, (unsigned)g_lo, (unsigned)(uintptr_t)_etext, 1u << BUCKET_SHIFT);
}

void Prof_FrameEnd(int slow)
{
    if (!g_all) return;
    irq_mask_t m = irq_disable();
    uint32_t n = g_frame_n;
    if (slow)
        for (uint32_t i = 0; i < n; i++) {
            int b = bucket(g_frame_pc[i]), r = bucket(g_frame_pr[i]);
            if (b >= 0) g_slow[b]++;
            if (r >= 0) g_slow_pr[r]++;
            g_samples_slow++;
        }
    g_frame_n = 0;
    irq_restore(m);
}

/* the k hottest buckets of h: "address:count" */
static void top(const char *what, const uint32_t *h, uint32_t total, int k)
{
    static uint32_t idx[64];
    int n = 0;
    for (uint32_t i = 0; i < g_n; i++) {
        if (!h[i]) continue;
        if (n < k) idx[n++] = i;
        else if (h[i] > h[idx[n - 1]]) idx[n - 1] = i;
        else continue;
        for (int j = n - 1; j > 0 && h[idx[j]] > h[idx[j - 1]]; j--) { uint32_t t = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t; }
    }
    printf("prof %s %u:", what, (unsigned)total);
    for (int i = 0; i < n; i++) printf(" %X:%u", (unsigned)(g_lo + (idx[i] << BUCKET_SHIFT)), (unsigned)h[idx[i]]);
    printf("\n");
}

void Prof_Report(void)
{
    if (!g_all) return;
    top("all", g_all, g_samples_all, 40);
    top("slow", g_slow, g_samples_slow, 60);
    top("slow-callers", g_slow_pr, g_samples_slow, 30);
}
