/* See include/dc2/task.h. */
#include "dc2/task.h"
#include "dc2/pcio.h"
#include "../hook/hooks.h"

void DC2CALL Task_Kill(u32 slot)                                     /* 0x4C2A0D */
{
    gptr s = A_TASK_SLOTS + slot * 0x20;
    for (int i = 0; i < 6; i++) U32(s + 4 * i) = 0;
#ifndef DC2_IN_PROCESS
    extern void task_host_kill(u32 slot);
    task_host_kill(slot);
#endif
}

void *Hook_Find(gptr addr)
{
    for (int i = 0; i < g_nhooks; i++) if (g_hooks[i].addr == addr) return g_hooks[i].fn;
    return 0;
}

#ifdef DC2_IN_PROCESS
#define ORIG(addr, type) ((type)(uintptr_t)(addr))
void DC2CALL Task_RunAll(void)             { ORIG(0x4C2900, void (DC2CALL *)(void))(); }
void DC2CALL Task_InitStackBase(void)      { ORIG(0x4C2967, void (DC2CALL *)(void))(); }
void DC2CALL Task_Start(u32 s, gptr fn)    { ORIG(0x4C2974, void (DC2CALL *)(u32, gptr))(s, fn); }
void DC2CALL Task_Chain(gptr fn)           { ORIG(0x4C29D6, void (DC2CALL *)(gptr))(fn); }
#else
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

#define TASK_STACK 0x40000
static ucontext_t g_sched, g_ctx[4];
static u8 *g_stack[4][2];         /* two stacks per slot: Task_Chain starts on the other one */
static int g_which[4];
static gptr g_entry[4];
static int g_running = -1;        /* slot being run by Task_RunAll */

static void task_entry(int slot)
{
    gptr fn = g_entry[slot];
    void (DC2CALL *f)(void) = (void (DC2CALL *)(void))Hook_Find(fn);
    if (f) f();
    else fprintf(stderr, "task %d: 0x%06X not rewritten yet (task stopped)\n", slot, fn);
    /* a task function never returns in the game; stop the slot */
    U16(A_TASK_SLOTS + slot * 0x20) = 0;
    setcontext(&g_sched);
}

static void make_task(u32 slot, gptr fn)
{
    int w = g_which[slot] ^= 1;
    if (!g_stack[slot][w]) g_stack[slot][w] = malloc(TASK_STACK);
    getcontext(&g_ctx[slot]);
    g_ctx[slot].uc_stack.ss_sp = g_stack[slot][w];
    g_ctx[slot].uc_stack.ss_size = TASK_STACK;
    g_ctx[slot].uc_link = &g_sched;
    g_entry[slot] = fn;
    makecontext(&g_ctx[slot], (void (*)(void))task_entry, 1, (int)slot);
}

void task_host_kill(u32 slot) { g_entry[slot] = 0; }

void DC2CALL Task_InitStackBase(void) {}

void DC2CALL Task_Start(u32 slot, gptr fn)
{
    U16(A_TASK_SLOTS + slot * 0x20) = 2;
    make_task(slot, fn);
}

void DC2CALL Task_RunAll(void)
{
    for (u32 i = 0; i < 4; i++) {
        gptr s = A_TASK_SLOTS + i * 0x20;
        U32(A_TASK_CUR) = i * 0x20;
        if (!U16(s)) continue;
        if (U16(s) == 1) {
            if (--U16(s + 2)) continue;
            U16(s) = 2;
        }
        g_running = (int)i;
        swapcontext(&g_sched, &g_ctx[i]);
        g_running = -1;
    }
}

void DC2CALL Task_Sleep(u32 n)
{
    if (g_running < 0) {            /* called outside a task (test drivers): only the loader runs */
        extern void DC2CALL File_Tick(void);
        while (n--) File_Tick();
        return;
    }
    u32 i = U32(A_TASK_CUR) / 0x20;
    U16(A_TASK_SLOTS + i * 0x20) = 1;
    U16(A_TASK_SLOTS + i * 0x20 + 2) = (u16)n;
    swapcontext(&g_ctx[i], &g_sched);
}

void DC2CALL Task_Chain(gptr fn)
{
    u32 i = U32(A_TASK_CUR) / 0x20;
    U16(A_TASK_SLOTS + i * 0x20) = 2;
    make_task(i, fn);
    setcontext(&g_sched);          /* the new task starts in the next frame */
}
#endif
