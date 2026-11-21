/* See include/dc2/task.h. */
#include "dc2/task.h"
#include "dc2/pcio.h"
#include "../hook/hooks.h"
#include <stdlib.h>

void DC2CALL Task_Kill(u32 slot)                                     /* 0x4C2A0D */
{
    gptr s = A_TASK_SLOTS + slot * 0x20;
    for (int i = 0; i < 6; i++) U32(s + 4 * i) = 0;
#ifndef DC2_IN_PROCESS
    extern void task_host_kill(u32 slot);
    task_host_kill(slot);
#endif
}

/* the hook table by address: an index sorted once (by address, then by
   position, so the first entry of an address wins as in a linear search),
   searched by halves - the object and task handlers are looked up here many
   times a frame */
static u16 *g_hook_ix;
static int hook_cmp(const void *a, const void *b)
{
    const Hook *x = &g_hooks[*(const u16 *)a], *y = &g_hooks[*(const u16 *)b];
    if (x->addr != y->addr) return x->addr < y->addr ? -1 : 1;
    return *(const u16 *)a < *(const u16 *)b ? -1 : 1;
}
void *Hook_Find(gptr addr)
{
    if (!g_hook_ix) {
        u16 *ix = malloc((size_t)g_nhooks * sizeof *ix);
        if (!ix) { for (int i = 0; i < g_nhooks; i++) if (g_hooks[i].addr == addr) return g_hooks[i].fn; return 0; }
        for (int i = 0; i < g_nhooks; i++) ix[i] = (u16)i;
        qsort(ix, (size_t)g_nhooks, sizeof *ix, hook_cmp);
        g_hook_ix = ix;
    }
    int lo = 0, hi = g_nhooks;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (g_hooks[g_hook_ix[mid]].addr < addr) lo = mid + 1; else hi = mid;
    }
    return lo < g_nhooks && g_hooks[g_hook_ix[lo]].addr == addr ? g_hooks[g_hook_ix[lo]].fn : 0;
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

/* Each task slot runs on its own host stack: ucontext on POSIX, fibers on Windows.
   ctx_make prepares a slot to start at its entry, ctx_run runs it until it sleeps,
   chains or ends, ctx_leave goes back to the scheduler, ctx_end leaves for good
   (the slot ended, or chained to a new start). */
#define TASK_STACK 0x40000
static gptr g_entry[4];
static int g_running = -1;        /* slot being run by Task_RunAll */
static void task_entry(int slot);

#ifdef _WIN32
#include <windows.h>
static LPVOID g_sched, g_fib[4], g_dead[8];
static int g_ndead;
static void CALLBACK fiber_entry(LPVOID p) { task_entry((int)(intptr_t)p); }
static void ctx_make(u32 slot)
{
    if (!g_sched) g_sched = ConvertThreadToFiber(0);
    if (g_fib[slot]) {             /* (the running one, chaining: deleted back in the scheduler) */
        if (g_fib[slot] == GetCurrentFiber()) g_dead[g_ndead++] = g_fib[slot];
        else DeleteFiber(g_fib[slot]);
    }
    g_fib[slot] = CreateFiber(TASK_STACK, fiber_entry, (LPVOID)(intptr_t)slot);
}
static void ctx_run(u32 slot)
{
    SwitchToFiber(g_fib[slot]);
    while (g_ndead) DeleteFiber(g_dead[--g_ndead]);
}
static void ctx_leave(u32 slot) { (void)slot; SwitchToFiber(g_sched); }
static void ctx_end(u32 slot)
{
    if (g_fib[slot] == GetCurrentFiber()) { g_dead[g_ndead++] = g_fib[slot]; g_fib[slot] = 0; }
    SwitchToFiber(g_sched);
}
#elif defined(__DREAMCAST__)
/* KOS: no ucontext; src/port/dc/ctx_sh4.S swaps the callee-saved registers.
   Each switch brackets the swap with dc2_ctx_enter / dc2_ctx_leave_irq so that
   KOS's stack check sees the stack being run (interrupts off in between). */
#include "../port/dc/ctx_sh4.h"
#undef TASK_STACK
#define TASK_STACK 0x10000
static Dc2Ctx g_sched, g_ctx[4], g_gone;
static u8 *g_stack[4][2];         /* two stacks per slot: Task_Chain starts on the other one */
static int g_which[4];
static u32 g_irq;
static void dc_start(int slot) { dc2_ctx_leave_irq(g_irq); task_entry(slot); }
static void dc_start0(void) { dc_start(0); }
static void dc_start1(void) { dc_start(1); }
static void dc_start2(void) { dc_start(2); }
static void dc_start3(void) { dc_start(3); }
static void (*const g_dc_start[4])(void) = {dc_start0, dc_start1, dc_start2, dc_start3};
static void ctx_make(u32 slot)
{
    int w = g_which[slot] ^= 1;
    if (!g_stack[slot][w]) g_stack[slot][w] = malloc(TASK_STACK);
    dc2_ctx_init(&g_ctx[slot], g_stack[slot][w] + TASK_STACK, g_dc_start[slot]);
}
static void ctx_run(u32 slot)
{
    g_irq = dc2_ctx_enter(g_stack[slot][g_which[slot]], TASK_STACK);
    dc2_ctx_swap(&g_sched, &g_ctx[slot]);
    dc2_ctx_leave_irq(g_irq);
}
static void ctx_leave(u32 slot)
{
    g_irq = dc2_ctx_enter(0, 0);
    dc2_ctx_swap(&g_ctx[slot], &g_sched);
    dc2_ctx_leave_irq(g_irq);
}
static void ctx_end(u32 slot)
{
    (void)slot;
    g_irq = dc2_ctx_enter(0, 0);
    dc2_ctx_swap(&g_gone, &g_sched);
    for (;;) ;                      /* (never resumed) */
}
#else
#include <ucontext.h>
static ucontext_t g_sched, g_ctx[4];
static u8 *g_stack[4][2];         /* two stacks per slot: Task_Chain starts on the other one */
static int g_which[4];
static void ctx_make(u32 slot)
{
    int w = g_which[slot] ^= 1;
    if (!g_stack[slot][w]) g_stack[slot][w] = malloc(TASK_STACK);
    getcontext(&g_ctx[slot]);
    g_ctx[slot].uc_stack.ss_sp = g_stack[slot][w];
    g_ctx[slot].uc_stack.ss_size = TASK_STACK;
    g_ctx[slot].uc_link = &g_sched;
    makecontext(&g_ctx[slot], (void (*)(void))task_entry, 1, (int)slot);
}
static void ctx_run(u32 slot) { swapcontext(&g_sched, &g_ctx[slot]); }
static void ctx_leave(u32 slot) { swapcontext(&g_ctx[slot], &g_sched); }
static void ctx_end(u32 slot) { (void)slot; setcontext(&g_sched); }
#endif

static void task_entry(int slot)
{
    gptr fn = g_entry[slot];
    void (DC2CALL *f)(void) = (void (DC2CALL *)(void))Hook_Find(fn);
    if (f) f();
    else fprintf(stderr, "task %d: 0x%06X not rewritten yet (task stopped)\n", slot, (unsigned)fn);
    /* a task function never returns in the game; stop the slot */
    U16(A_TASK_SLOTS + slot * 0x20) = 0;
    ctx_end((u32)slot);
}

static void make_task(u32 slot, gptr fn)
{
    g_entry[slot] = fn;
    ctx_make(slot);
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
        ctx_run(i);
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
    ctx_leave(i);
}

void DC2CALL Task_Chain(gptr fn)
{
    u32 i = U32(A_TASK_CUR) / 0x20;
    U16(A_TASK_SLOTS + i * 0x20) = 2;
    make_task(i, fn);
    ctx_end(i);          /* the new task starts in the next frame */
}
#endif
