/* C runtime functions of Dino2.exe (MSVC) whose exact behaviour the game
 * depends on. */
#include "dc2/game.h"
#include "dc2/orig.h"
#include "dc2/task.h"

/* comparator: a game function given by its original address (looked up
   once a sort, not once a comparison: g_cmp_fn) */
#ifndef DC2_IN_PROCESS
static s32 (DC2CALL *g_cmp_fn)(gptr, gptr);
static gptr g_cmp_addr;
#endif
static s32 call_cmp(gptr fn, gptr a, gptr b)
{
#ifdef DC2_IN_PROCESS
    return ((s32 (DC2CALL *)(gptr, gptr))(uintptr_t)fn)(a, b);
#else
    if (fn != g_cmp_addr) { g_cmp_fn = (s32 (DC2CALL *)(gptr, gptr))Hook_Find(fn); g_cmp_addr = fn; }
    return g_cmp_fn ? g_cmp_fn(a, b) : 0;
#endif
}

static void swap_elems(gptr a, gptr b, u32 width)
{
    if (a == b) return;
    u8 *p = MEM(a), *q = MEM(b);                       /* (host pointers: the bytes as U8 would swap them) */
    for (u32 i = 0; i < width; i++) { u8 t = p[i]; p[i] = q[i]; q[i] = t; }
}

/* selection sort of lo..hi (both inclusive) */
static void shortsort(gptr lo, gptr hi, u32 width, gptr cmp)
{
    while (hi > lo) {
        gptr max = lo;
        for (gptr p = lo + width; p <= hi; p += width)
            if (call_cmp(cmp, p, max) > 0) max = p;
        swap_elems(max, hi, width);
        hi -= width;
    }
}

/* MSVC qsort: quicksort with the middle element as pivot, explicit stack,
 * the smaller part first; 8 elements or fewer by selection sort. The
 * order of equal elements is part of the game's behaviour. */
void DC2CALL Crt_qsort(gptr base, u32 num, u32 width, gptr cmp)    /* 0x4DDBD4 */
{
    gptr lostk[30], histk[30];
    s32 sp = 0;
    if (num < 2 || !width) return;
    gptr lo = base, hi = base + width * (num - 1);
    for (;;) {
        u32 size = (hi - lo) / width + 1;
        if (size <= 8) shortsort(lo, hi, width, cmp);
        else {
            gptr mid = lo + (size / 2) * width;
            swap_elems(mid, lo, width);
            gptr loguy = lo, higuy = hi + width;
            for (;;) {
                do loguy += width; while (loguy <= hi && call_cmp(cmp, loguy, lo) <= 0);
                do higuy -= width; while (higuy > lo && call_cmp(cmp, higuy, lo) >= 0);
                if (higuy < loguy) break;
                swap_elems(loguy, higuy, width);
            }
            swap_elems(lo, higuy, width);
            if ((s32)(higuy - 1 - lo) >= (s32)(hi - loguy)) {
                if (lo + width < higuy) { lostk[sp] = lo; histk[sp] = higuy - width; sp++; }
                if (loguy < hi) { lo = loguy; continue; }
            } else {
                if (loguy < hi) { lostk[sp] = loguy; histk[sp] = hi; sp++; }
                if (lo + width < higuy) { hi = higuy - width; continue; }
            }
        }
        if (--sp < 0) return;
        lo = lostk[sp]; hi = histk[sp];
    }
}
