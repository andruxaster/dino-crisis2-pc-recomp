/* Cooperative context switch on the SH-4 (ctx_sh4.S), for the task system
 * of the Dreamcast build (src/game/task.c). Offsets shared with the .S. */
#ifndef DC2_CTX_SH4_H
#define DC2_CTX_SH4_H
#include <stdint.h>

typedef struct {
    uint32_t r[8];          /* 0x00 r8..r15 (r15 = sp) */
    uint32_t pr;            /* 0x20 where it resumes */
    uint32_t fpscr;         /* 0x24 */
    uint32_t mach, macl;    /* 0x28 */
    uint32_t fr[4];         /* 0x30 fr12..fr15 */
} Dc2Ctx;                   /* 0x40 */

void dc2_ctx_swap(Dc2Ctx *save, const Dc2Ctx *load);
uint32_t dc2_ctx_fpscr(void);

/* a context that calls entry() (which never returns) on the stack ending at top */
static inline void dc2_ctx_init(Dc2Ctx *c, void *top, void (*entry)(void))
{
    for (int i = 0; i < 8; i++) c->r[i] = 0;
    for (int i = 0; i < 4; i++) c->fr[i] = 0;
    c->r[7] = (uint32_t)(uintptr_t)top & ~7u;
    c->pr = (uint32_t)(uintptr_t)entry;
    c->fpscr = dc2_ctx_fpscr();
    c->mach = c->macl = 0;
}

/* KOS checks on each reschedule that sp lies within the running thread's
   stack: each switch tells it the stack being entered (NULL: back to the
   thread's own), with interrupts off across the swap (dc2_dc.c) */
uint32_t dc2_ctx_enter(void *stack, uint32_t size);
void dc2_ctx_leave_irq(uint32_t irq);
#endif
